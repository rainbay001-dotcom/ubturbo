/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 */

#include "mext_engine.h"
#include "mext_predictor_c_api.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/syscall.h>
#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434
#endif
#ifndef __NR_process_madvise
#define __NR_process_madvise 440
#endif
#ifndef MADV_WILLNEED
#define MADV_WILLNEED 3
#endif
#endif

namespace smap {
namespace mext {

namespace {

constexpr uint8_t kSmapMigMagic = 0xB9;
#define MEXT_IOCTL_PREFAULT _IOWR(kSmapMigMagic, 5, struct mext_prefault_msg)
#define MEXT_IOCTL_CTRL     _IOWR(kSmapMigMagic, 6, struct mext_ctrl_msg)

constexpr char kDefaultWeightsPath[] = "/opt/ubturbo/conf/smap/mext_tiny_tf.bin";

uint64_t SteadyNowNs()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

void MarkTargetOffset(uint8_t mask[MEXT_OUT_DIM], bool is_evict_space, int64_t delta)
{
    int32_t idx = OffsetToActionIndex(delta);
    if (idx >= 0 && idx < static_cast<int32_t>(MEXT_ACTION_OFFSETS)) {
        uint32_t pos = is_evict_space ? (MEXT_ACTION_OFFSETS + static_cast<uint32_t>(idx))
                                      : static_cast<uint32_t>(idx);
        mask[pos] = 1;
    }
}

} // namespace

MextEngine::MextEngine()
{
    MextDefaultConfig(&cfg_);
}

MextEngine::~MextEngine()
{
    Stop();
}

void MextEngine::BootstrapPretrain(TinyTransformer *tf, uint32_t epochs)
{
    if (!tf) {
        return;
    }
    TinyTransformerTrainer trainer(20261122U);
    std::vector<std::array<TrainingStepSample, TF_CTX_LEN>> dataset;
    dataset.reserve(48);

    /* 1. Sequential & strided streams (+1, -1, +2, +4, +8) */
    const int32_t strides[] = { 1, -1, 2, -2, 4, 8, 16, -4 };
    for (int s_idx = 0; s_idx < 16; ++s_idx) {
        int32_t st = strides[s_idx % 8];
        uint8_t vma = static_cast<uint8_t>((s_idx * 3) & 31);
        std::array<TrainingStepSample, TF_CTX_LEN> seq{};
        for (uint32_t t = 0; t < TF_CTX_LEN; ++t) {
            seq[t].token.dvpn_tok = EncodeDeltaToken(st, t > 0);
            seq[t].token.devict_tok = static_cast<uint8_t>(MEXT_TOKEN_UNKNOWN);
            seq[t].token.vma_tok = vma;
            seq[t].token.dt_tok = 5;
            seq[t].supervised = (t >= 2);
            for (int k = 1; k <= 4; ++k) {
                MarkTargetOffset(seq[t].target_mask, false, static_cast<int64_t>(st) * k);
            }
        }
        dataset.push_back(seq);
    }

    /* 2. Co-eviction pointer-chasing graphs (random large dVPN, dEvictSeq = +1..+4) */
    for (int g = 0; g < 16; ++g) {
        std::array<TrainingStepSample, TF_CTX_LEN> seq{};
        int64_t step_ev = (g % 3 == 0) ? 2 : 1;
        for (uint32_t t = 0; t < TF_CTX_LEN; ++t) {
            int64_t wild_dvpn = ((t + g) & 1U) ? (4096LL * (t + 3)) : (-8192LL * (t + 2));
            seq[t].token.dvpn_tok = EncodeDeltaToken(wild_dvpn, t > 0);
            seq[t].token.devict_tok = EncodeDeltaToken(step_ev, t > 0);
            seq[t].token.vma_tok = static_cast<uint8_t>((10 + g) & 31);
            seq[t].token.dt_tok = 6;
            seq[t].supervised = (t >= 2);
            for (int k = 1; k <= 4; ++k) {
                MarkTargetOffset(seq[t].target_mask, true, step_ev * k);
            }
        }
        dataset.push_back(seq);
    }

    /* 3. Interleaved dual-stream workloads (Stream A in vma1, Stream B in vma2) */
    for (int m = 0; m < 16; ++m) {
        std::array<TrainingStepSample, TF_CTX_LEN> seq{};
        uint8_t vma_a = static_cast<uint8_t>((m * 2) & 31);
        uint8_t vma_b = static_cast<uint8_t>((m * 2 + 7) & 31);
        int64_t vpn_a = 10000 + m * 100;
        int64_t vpn_b = 900000 + m * 500;
        int64_t stride_a = (m % 2 == 0) ? 1 : 4;
        int64_t stride_b = (m % 2 == 0) ? 2 : -1;
        int64_t prev_vpn = 0;
        std::array<int64_t, TF_CTX_LEN + 8> stream_vpns{};

        for (uint32_t t = 0; t < TF_CTX_LEN + 8; ++t) {
            if ((t & 1U) == 0) {
                stream_vpns[t] = vpn_a;
                vpn_a += stride_a;
            } else {
                stream_vpns[t] = vpn_b;
                vpn_b += stride_b;
            }
        }

        for (uint32_t t = 0; t < TF_CTX_LEN; ++t) {
            int64_t cur = stream_vpns[t];
            seq[t].token.dvpn_tok = EncodeDeltaToken(cur - prev_vpn, t > 0);
            seq[t].token.devict_tok = static_cast<uint8_t>(MEXT_TOKEN_UNKNOWN);
            seq[t].token.vma_tok = ((t & 1U) == 0) ? vma_a : vma_b;
            seq[t].token.dt_tok = 4;
            seq[t].supervised = (t >= 3);
            for (uint32_t h = 1; h <= 6; ++h) {
                MarkTargetOffset(seq[t].target_mask, false, stream_vpns[t + h] - cur);
            }
            prev_vpn = cur;
        }
        dataset.push_back(seq);
    }

    const size_t batch_sz = 8;
    for (uint32_t ep = 0; ep < epochs; ++ep) {
        float lr = (ep < epochs / 2) ? 0.012f : 0.005f;
        for (size_t i = 0; i < dataset.size(); i += batch_sz) {
            size_t end = std::min(dataset.size(), i + batch_sz);
            trainer.ZeroGrad();
            for (size_t j = i; j < end; ++j) {
                trainer.AccumulateGradients(dataset[j].data(), TF_CTX_LEN);
            }
            trainer.StepAdamW(lr, 1e-4f, 1.0f / static_cast<float>(end - i));
        }
    }

    tf->LoadFromFp32(trainer.Params());
}

int MextEngine::Start(int mig_fd, const MextConfig &cfg, bool spawn_worker)
{
    if (running_.load(std::memory_order_acquire)) {
        return UpdateConfig(cfg);
    }

    {
        std::lock_guard<std::mutex> lk(mu_);
        cfg_ = cfg;
        mig_fd_ = mig_fd;
        long ps = sysconf(_SC_PAGESIZE);
        if (ps == 65536) {
            page_shift_ = 16;
        } else {
            page_shift_ = 12;
        }

        bool loaded = false;
        if (cfg_.weights_path[0] != '\0') {
            loaded = transformer_.LoadWeights(cfg_.weights_path);
        } else if (access(kDefaultWeightsPath, R_OK) == 0) {
            loaded = transformer_.LoadWeights(kDefaultWeightsPath);
        }
        if (!loaded) {
            BootstrapPretrain(&transformer_, 6);
        }

        if (stats_json_path_.empty()) {
            const char *env_path = std::getenv("SMAP_MEXT_STATS_PATH");
            if (env_path && env_path[0] != '\0') {
                stats_json_path_ = env_path;
            } else if (mig_fd_ >= 0) {
                stats_json_path_ = (access("/var/run", W_OK) == 0)
                                       ? "/var/run/smap_mext_stats.json"
                                       : "/tmp/smap_mext_stats.json";
            }
        }

        if (mig_fd_ >= 0 && !mmap_ring_) {
            size_t page_sz = 1ULL << page_shift_;
            mmap_bytes_ = (sizeof(mext_ring_buffer) + page_sz - 1ULL) & ~(page_sz - 1ULL);
            void *addr = mmap(nullptr, mmap_bytes_, PROT_READ | PROT_WRITE, MAP_SHARED, mig_fd_, 0);
            if (addr != MAP_FAILED) {
                auto *rb = static_cast<mext_ring_buffer *>(addr);
                if (rb->hdr.magic == MEXT_RING_MAGIC && rb->hdr.capacity == MEXT_RING_CAPACITY) {
                    mmap_ring_ = rb;
                    owns_mmap_ = true;
                    if (rb->hdr.page_shift >= 12 && rb->hdr.page_shift <= 16) {
                        page_shift_ = rb->hdr.page_shift;
                    }
                } else {
                    munmap(addr, mmap_bytes_);
                }
            }

            mext_ctrl_msg ctrl{};
            ctrl.op = MEXT_CTRL_ENABLE;
            (void)ioctl(mig_fd_, MEXT_IOCTL_CTRL, &ctrl);
        }
    }

    running_.store(true, std::memory_order_release);
    if (spawn_worker) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            async_pool_active_ = true;
        }
        for (uint32_t s = 0; s < kPrefetchShards; ++s) {
            prefetch_shards_[s].worker =
                std::thread(&MextEngine::PrefetchShardLoop, this, s);
        }
        worker_ = std::thread(&MextEngine::WorkerLoop, this);
    } else {
        std::lock_guard<std::mutex> lk(mu_);
        async_pool_active_ = false;
    }
    return 0;
}

void MextEngine::Stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        return;
    }
    for (uint32_t s = 0; s < kPrefetchShards; ++s) {
        prefetch_shards_[s].cv.notify_all();
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    for (uint32_t s = 0; s < kPrefetchShards; ++s) {
        if (prefetch_shards_[s].worker.joinable()) {
            prefetch_shards_[s].worker.join();
        }
        std::lock_guard<std::mutex> slk(prefetch_shards_[s].mu);
        prefetch_shards_[s].queue.clear();
        prefetch_shards_[s].pending_close_pids.clear();
        CloseAllShardPidfds(&prefetch_shards_[s]);
    }

    FlushStatsJsonOutsideLock(SteadyNowNs(), true);
    std::lock_guard<std::mutex> lk(mu_);
    async_pool_active_ = false;
    if (mig_fd_ >= 0) {
        mext_ctrl_msg ctrl{};
        ctrl.op = MEXT_CTRL_DISABLE;
        (void)ioctl(mig_fd_, MEXT_IOCTL_CTRL, &ctrl);
    }
    pid_states_.clear();
    shadow_map_.Clear();
    tier1_.Clear();
    transformer_.ClearState();

    if (trace_fp_) {
        std::fflush(trace_fp_);
        std::fclose(trace_fp_);
        trace_fp_ = nullptr;
    }

    if (owns_mmap_ && mmap_ring_) {
        munmap(mmap_ring_, mmap_bytes_);
        mmap_ring_ = nullptr;
        owns_mmap_ = false;
    }
}

int MextEngine::UpdateConfig(const MextConfig &cfg)
{
    std::lock_guard<std::mutex> lk(mu_);
    bool reload_weights = (std::strncmp(cfg_.weights_path, cfg.weights_path, sizeof(cfg_.weights_path)) != 0 &&
                           cfg.weights_path[0] != '\0');
    cfg_ = cfg;
    if (reload_weights) {
        if (!transformer_.LoadWeights(cfg_.weights_path)) {
            return -1;
        }
    }
    return 0;
}

int MextEngine::TrackPid(int32_t pid)
{
    if (pid <= 0) {
        return -1;
    }
    int fd = -1;
    {
        std::lock_guard<std::mutex> lk(mu_);
        fd = mig_fd_;
    }
    /*
     * Register PID with kernel fault filter outside mu_ and defer allocating
     * PidRuntimeState until the PID actually experiences a swap event (0 B
     * overhead for non-swapping sandboxes).
     */
    if (fd >= 0) {
        mext_ctrl_msg ctrl{};
        ctrl.op = MEXT_CTRL_ADD_PID;
        ctrl.pid = pid;
        return ioctl(fd, MEXT_IOCTL_CTRL, &ctrl);
    }
    return 0;
}

void MextEngine::UntrackPid(int32_t pid)
{
    int fd = -1;
    bool pool_active = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        fd = mig_fd_;
        pool_active = async_pool_active_;
        pid_states_.erase(pid);
        shadow_map_.RemovePid(pid);
        tier1_.RemovePid(pid);
        transformer_.ResetPid(pid);
    }

    if (pid > 0) {
        PrefetchShard &sh = prefetch_shards_[static_cast<uint32_t>(pid) & (kPrefetchShards - 1U)];
        std::lock_guard<std::mutex> slk(sh.mu);
        sh.queue.erase(
            std::remove_if(sh.queue.begin(), sh.queue.end(),
                           [pid](const PrefetchWorkItem &it) { return it.pid == pid; }),
            sh.queue.end());
        if (pool_active) {
            sh.pending_close_pids.push_back(pid);
            sh.cv.notify_one();
        } else {
            CloseShardPidfd(&sh, pid);
        }
    }

    if (fd >= 0) {
        mext_ctrl_msg ctrl{};
        ctrl.op = MEXT_CTRL_REMOVE_PID;
        ctrl.pid = pid;
        (void)ioctl(fd, MEXT_IOCTL_CTRL, &ctrl);
    }
}

void MextEngine::AttachExternalRing(mext_ring_buffer *ring)
{
    std::lock_guard<std::mutex> lk(mu_);
    if (owns_mmap_ && mmap_ring_) {
        munmap(mmap_ring_, mmap_bytes_);
        owns_mmap_ = false;
    }
    mmap_ring_ = ring;
}

void MextEngine::SetDispatchCallback(DispatchCallback cb)
{
    std::lock_guard<std::mutex> lk(mu_);
    dispatch_cb_ = std::move(cb);
}

void MextEngine::SetPressureCallback(PressureCallback cb)
{
    std::lock_guard<std::mutex> lk(mu_);
    pressure_cb_ = std::move(cb);
}

void MextEngine::SetStatsJsonPath(const std::string &path)
{
    std::lock_guard<std::mutex> lk(mu_);
    stats_json_path_ = path;
}

void MextEngine::SetTraceDumpPath(const std::string &path)
{
    std::lock_guard<std::mutex> lk(mu_);
    if (trace_fp_) {
        std::fflush(trace_fp_);
        std::fclose(trace_fp_);
        trace_fp_ = nullptr;
    }
    trace_events_written_ = 0;
    if (!path.empty()) {
        trace_fp_ = std::fopen(path.c_str(), "wb");
        if (!trace_fp_) {
            std::perror(("Failed to open trace dump " + path).c_str());
        }
    }
}

void MextEngine::SetRecordOnly(bool on)
{
    record_only_.store(on, std::memory_order_release);
}

void MextEngine::FlushStatsJson()
{
    FlushStatsJsonOutsideLock(SteadyNowNs(), true);
}

bool MextEngine::PopRingEvent(mext_ring_event *out)
{
    if (!mmap_ring_ || !out) {
        return false;
    }
    mext_ring_header *hdr = &mmap_ring_->hdr;
    uint64_t r = hdr->tail;
    mext_ring_event *ev = &mmap_ring_->events[r & MEXT_RING_MASK];
    uint32_t type = __atomic_load_n(&ev->type, __ATOMIC_ACQUIRE);
    if (type == MEXT_EVT_NONE) {
        return false;
    }
    *out = *ev;
    __atomic_store_n(&ev->type, static_cast<uint32_t>(MEXT_EVT_NONE), __ATOMIC_RELEASE);
    __atomic_store_n(&hdr->tail, r + 1ULL, __ATOMIC_RELEASE);
    return true;
}

size_t MextEngine::ProcessRingOnce(size_t max_events)
{
    std::vector<mext_ring_event> batch;
    batch.reserve(std::min<size_t>(max_events, 256U));
    PressureCallback pressure_cb;
    std::vector<uint8_t> skip_retry_dup;
    std::unordered_map<uint32_t, size_t> last_fault_idx;

    {
        std::lock_guard<std::mutex> lk(mu_);
        pressure_cb = pressure_cb_;
        mext_ring_event ev{};
        while (batch.size() < max_events && PopRingEvent(&ev)) {
            batch.push_back(ev);
        }
        if (trace_fp_ && !batch.empty()) {
            std::fwrite(batch.data(), sizeof(mext_ring_event), batch.size(), trace_fp_);
            trace_events_written_ += batch.size();
        }
        if (batch.empty()) {
            return 0;
        }

        /*
         * Pre-identify Linux `do_swap_page` VM_FAULT_RETRY second-pass duplicates
         * BEFORE Frontier Coalescing so a trailing retry duplicate never steals
         * `is_frontier = true` from the preceding genuine swap fault.
         */
        skip_retry_dup.assign(batch.size(), 0U);
        struct TempRecent {
            RecentFaultSlot slots[kRecentFaultSlots] = {};
            uint32_t cursor = 0;
        };
        std::unordered_map<uint32_t, TempRecent> temp_recent;
        std::unordered_map<uint32_t, std::vector<uint64_t>> batch_evicted;

        for (size_t i = 0; i < batch.size(); ++i) {
            const auto &b_ev = batch[i];
            if (b_ev.pid == 0 || b_ev.vpn == 0) {
                continue;
            }
            if (b_ev.type == MEXT_EVT_EVICTED) {
                batch_evicted[b_ev.pid].push_back(b_ev.vpn);
                auto tr_it = temp_recent.find(b_ev.pid);
                if (tr_it != temp_recent.end()) {
                    for (uint32_t s = 0; s < kRecentFaultSlots; ++s) {
                        if (tr_it->second.slots[s].vpn == b_ev.vpn) {
                            tr_it->second.slots[s].vpn = 0;
                        }
                    }
                }
            } else if (b_ev.type == MEXT_EVT_SWAP_FAULT) {
                auto [tr_it, inserted] = temp_recent.emplace(b_ev.pid, TempRecent{});
                if (inserted) {
                    auto pst_it = pid_states_.find(static_cast<int32_t>(b_ev.pid));
                    if (pst_it != pid_states_.end()) {
                        std::memcpy(tr_it->second.slots, pst_it->second.recent_faults,
                                    sizeof(tr_it->second.slots));
                        tr_it->second.cursor = pst_it->second.recent_fault_cursor;
                    }
                }
                bool re_evicted = shadow_map_.IsSwappedOut(static_cast<int32_t>(b_ev.pid), b_ev.vpn);
                if (!re_evicted) {
                    auto bev_it = batch_evicted.find(b_ev.pid);
                    if (bev_it != batch_evicted.end()) {
                        re_evicted = (std::find(bev_it->second.begin(),
                                                bev_it->second.end(),
                                                b_ev.vpn) != bev_it->second.end());
                    }
                }
                uint64_t f_ts = b_ev.timestamp;
                bool is_dup = false;
                if (!re_evicted && f_ts > 0) {
                    for (uint32_t s = 0; s < kRecentFaultSlots; ++s) {
                        if (tr_it->second.slots[s].vpn == b_ev.vpn &&
                            f_ts >= tr_it->second.slots[s].ts_ns &&
                            (f_ts - tr_it->second.slots[s].ts_ns) <= kRetryDedupWindowNs) {
                            is_dup = true;
                            break;
                        }
                    }
                }
                if (is_dup) {
                    skip_retry_dup[i] = 1U;
                    ++retry_dups_skipped_;
                } else {
                    auto bev_it = batch_evicted.find(b_ev.pid);
                    if (bev_it != batch_evicted.end()) {
                        bev_it->second.erase(
                            std::remove(bev_it->second.begin(), bev_it->second.end(), b_ev.vpn),
                            bev_it->second.end());
                    }
                    tr_it->second.slots[tr_it->second.cursor & (kRecentFaultSlots - 1U)] =
                        { b_ev.vpn, f_ts };
                    ++tr_it->second.cursor;
                    last_fault_idx[b_ev.pid] = i;
                }
            }
        }
    }

    /*
     * Process in 64-event locked slices so large eviction bursts avoid per-event
     * mutex ping-pong while keeping lock hold times bounded (< 50 us).
     */
    constexpr size_t kSlice = 64U;
    for (size_t base = 0; base < batch.size(); base += kSlice) {
        size_t end = std::min(batch.size(), base + kSlice);
        struct PressureNote {
            uint32_t nid;
            uint64_t free_pages;
            uint64_t wmark_low;
            uint16_t order;
            uint64_t ts;
        };
        std::vector<PressureNote> p_notes;

        {
            std::lock_guard<std::mutex> lk(mu_);
            for (size_t i = base; i < end; ++i) {
                if (skip_retry_dup[i]) {
                    continue;
                }
                const auto &ev = batch[i];
                if (ev.type == MEXT_EVT_EVICTED) {
                    HandleEvictionEventLocked(static_cast<int32_t>(ev.pid), ev.vpn, ev.aux);
                } else if (ev.type == MEXT_EVT_SWAP_FAULT) {
                    auto l_it = last_fault_idx.find(ev.pid);
                    bool is_frontier = (l_it != last_fault_idx.end() && l_it->second == i);
                    HandleFaultEventLocked(static_cast<int32_t>(ev.pid), ev.vpn,
                                           static_cast<uint8_t>(ev.aux & 0x1fULL),
                                           ev.timestamp, is_frontier);
                } else if (ev.type == MEXT_EVT_MEM_PRESSURE && pressure_cb) {
                    p_notes.push_back({ ev.pid, ev.vpn, ev.aux >> 16,
                                        static_cast<uint16_t>(ev.aux & 0xffffULL),
                                        ev.timestamp });
                }
            }
        }

        for (const auto &pn : p_notes) {
            pressure_cb(pn.nid, pn.free_pages, pn.wmark_low, pn.order, pn.ts);
        }
    }
    return batch.size();
}

void MextEngine::WorkerLoop()
{
    uint32_t idle_spins = 0;
    uint32_t batches_since_maint = 0;
    while (running_.load(std::memory_order_acquire)) {
        size_t processed = ProcessRingOnce(1024);
        if ((++batches_since_maint & 15U) == 0U || processed == 0) {
            uint64_t now = SteadyNowNs();
            RefreshPsiOutsideLock(now);
            FlushStatsJsonOutsideLock(now, false);
        }
        if (processed > 0) {
            idle_spins = 0;
            continue;
        }
        /*
         * Three-stage adaptive hybrid poll:
         *   1. <256 empty checks: spin-yield for ~2-5 us zram burst tracking;
         *   2. <1024 empty checks: 100 us short nap between multi-batch NVMe faults;
         *   3. >=1024 empty checks: 2 ms deep-idle sleep so unconstrained hosts
         *      with zero swap activity incur ~0% background CPU & timer interrupts.
         */
        if (idle_spins < 256U) {
            ++idle_spins;
            std::this_thread::yield();
        } else if (idle_spins < 1024U) {
            ++idle_spins;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

void MextEngine::HandleEvictionEventLocked(int32_t pid, uint64_t vpn, uint64_t epoch)
{
    if (shadow_map_.RecordEviction(pid, vpn, epoch) > 0) {
        ++evicts_tracked_;
    }
    auto it = pid_states_.find(pid);
    if (it != pid_states_.end()) {
        for (uint32_t s = 0; s < kRecentFaultSlots; ++s) {
            if (it->second.recent_faults[s].vpn == vpn) {
                it->second.recent_faults[s].vpn = 0;
            }
        }
    }
}

void MextEngine::HandleEvictionEvent(int32_t pid, uint64_t vpn, uint64_t epoch)
{
    std::lock_guard<std::mutex> lk(mu_);
    HandleEvictionEventLocked(pid, vpn, epoch);
}

bool MextEngine::BuildStatsJsonLocked(uint64_t now_ns,
                                      bool force,
                                      std::string *out_path,
                                      std::string *out_json)
{
    if (trace_fp_) {
        std::fflush(trace_fp_);
    }
    if (stats_json_path_.empty() || !out_path || !out_json) {
        return false;
    }
    if (!force && (now_ns - last_stats_flush_ns_ < 1000000000ULL)) {
        return false;
    }
    last_stats_flush_ns_ = now_ns;

    double hit_rate = (predictions_evaluated_ > 0)
                          ? (static_cast<double>(prediction_hits_) /
                             static_cast<double>(predictions_evaluated_))
                          : 0.0;
    double coverage_rate = (faults_processed_ + prediction_hits_ > 0)
                               ? (static_cast<double>(prediction_hits_) /
                                  static_cast<double>(faults_processed_ + prediction_hits_))
                               : 0.0;
    uint64_t total_prefetched = prefetch_issued_ + prefault_issued_;
    uint64_t ring_dropped = mmap_ring_ ? mmap_ring_->hdr.dropped : 0ULL;
    uint64_t pflt_pop = prefault_populated_.load(std::memory_order_relaxed);
    double psi_avg10 = static_cast<double>(
                           psi_some_milli_.load(std::memory_order_relaxed)) /
                       1000.0;

    char buf[1280];
    int n = std::snprintf(
        buf, sizeof(buf),
        "{\n"
        "  \"faults_processed\": %llu,\n"
        "  \"retry_dups_skipped\": %llu,\n"
        "  \"shadow_matched_faults\": %llu,\n"
        "  \"evicts_tracked\": %llu,\n"
        "  \"tier1_predictions\": %llu,\n"
        "  \"transformer_predictions\": %llu,\n"
        "  \"prefetch_issued\": %llu,\n"
        "  \"prefault_issued\": %llu,\n"
        "  \"prefault_populated\": %llu,\n"
        "  \"total_prefetched_pages\": %llu,\n"
        "  \"prediction_hits\": %llu,\n"
        "  \"predictions_evaluated\": %llu,\n"
        "  \"hit_rate\": %.6f,\n"
        "  \"coverage_rate\": %.6f,\n"
        "  \"ring_dropped\": %llu,\n"
        "  \"avg_infer_us\": %.3f,\n"
        "  \"trace_events_written\": %llu,\n"
        "  \"psi_some_avg10\": %.3f\n"
        "}\n",
        static_cast<unsigned long long>(faults_processed_),
        static_cast<unsigned long long>(retry_dups_skipped_),
        static_cast<unsigned long long>(shadow_matched_faults_),
        static_cast<unsigned long long>(evicts_tracked_),
        static_cast<unsigned long long>(tier1_preds_),
        static_cast<unsigned long long>(tf_preds_),
        static_cast<unsigned long long>(prefetch_issued_),
        static_cast<unsigned long long>(prefault_issued_),
        static_cast<unsigned long long>(pflt_pop),
        static_cast<unsigned long long>(total_prefetched),
        static_cast<unsigned long long>(prediction_hits_),
        static_cast<unsigned long long>(predictions_evaluated_),
        hit_rate, coverage_rate,
        static_cast<unsigned long long>(ring_dropped),
        avg_infer_us_,
        static_cast<unsigned long long>(trace_events_written_),
        psi_avg10);
    if (n <= 0) {
        return false;
    }
    *out_path = stats_json_path_;
    out_json->assign(buf, static_cast<size_t>(std::min<int>(n, static_cast<int>(sizeof(buf) - 1))));
    return true;
}

void MextEngine::FlushStatsJsonOutsideLock(uint64_t now_ns, bool force)
{
    std::string target_path;
    std::string payload;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!BuildStatsJsonLocked(now_ns, force, &target_path, &payload)) {
            return;
        }
    }
    std::string tmp_path = target_path + ".tmp";
    FILE *fp = std::fopen(tmp_path.c_str(), "w");
    if (!fp) {
        return;
    }
    (void)std::fwrite(payload.data(), 1U, payload.size(), fp);
    std::fclose(fp);
    (void)std::rename(tmp_path.c_str(), target_path.c_str());
}

void MextEngine::RefreshPsiOutsideLock(uint64_t now_ns)
{
#ifdef __linux__
    uint64_t prev = last_psi_check_ns_.load(std::memory_order_relaxed);
    if (now_ns >= prev && (now_ns - prev) < 100000000ULL) { /* 100 ms */
        return;
    }
    last_psi_check_ns_.store(now_ns, std::memory_order_relaxed);
    FILE *f = std::fopen("/proc/pressure/memory", "r");
    if (!f) {
        return;
    }
    char line[256];
    if (std::fgets(line, sizeof(line), f)) {
        double avg10 = 0.0;
        if (std::sscanf(line, "some avg10=%lf", &avg10) == 1 && avg10 >= 0.0) {
            psi_some_milli_.store(static_cast<uint64_t>(avg10 * 1000.0 + 0.5),
                                  std::memory_order_relaxed);
        }
    }
    std::fclose(f);
#else
    (void)now_ns;
#endif
}

void MextEngine::CloseShardPidfd(PrefetchShard *shard, int32_t pid)
{
    if (!shard || pid <= 0) {
        return;
    }
    for (uint32_t i = 0; i < kPidfdsPerShard; ++i) {
        if (shard->pidfd_ring[i].pid == pid) {
            if (shard->pidfd_ring[i].fd >= 0) {
                close(shard->pidfd_ring[i].fd);
                shard->pidfd_ring[i].fd = -1;
                total_open_pidfds_.fetch_sub(1U, std::memory_order_relaxed);
            }
            shard->pidfd_ring[i].pid = 0;
        }
    }
}

void MextEngine::CloseAllShardPidfds(PrefetchShard *shard)
{
    if (!shard) {
        return;
    }
    for (uint32_t i = 0; i < kPidfdsPerShard; ++i) {
        if (shard->pidfd_ring[i].fd >= 0) {
            close(shard->pidfd_ring[i].fd);
            shard->pidfd_ring[i].fd = -1;
            total_open_pidfds_.fetch_sub(1U, std::memory_order_relaxed);
        }
        shard->pidfd_ring[i].pid = 0;
    }
    shard->pidfd_cursor = 0;
}

int MextEngine::EnsureShardPidfd(PrefetchShard *shard, int32_t pid)
{
#ifdef __linux__
    if (!shard || pid <= 0) {
        return -1;
    }
    for (uint32_t i = 0; i < kPidfdsPerShard; ++i) {
        if (shard->pidfd_ring[i].pid == pid && shard->pidfd_ring[i].fd >= 0) {
            return shard->pidfd_ring[i].fd;
        }
    }
    const uint32_t slot = shard->pidfd_cursor & (kPidfdsPerShard - 1U);
    if (shard->pidfd_ring[slot].fd >= 0) {
        close(shard->pidfd_ring[slot].fd);
        shard->pidfd_ring[slot].fd = -1;
        shard->pidfd_ring[slot].pid = 0;
        total_open_pidfds_.fetch_sub(1U, std::memory_order_relaxed);
    }
    long fd = syscall(__NR_pidfd_open, pid, 0U);
    if (fd >= 0) {
        shard->pidfd_ring[slot].pid = pid;
        shard->pidfd_ring[slot].fd = static_cast<int>(fd);
        shard->pidfd_cursor = (slot + 1U) & (kPidfdsPerShard - 1U);
        total_open_pidfds_.fetch_add(1U, std::memory_order_relaxed);
        return static_cast<int>(fd);
    }
    return -1;
#else
    (void)shard;
    (void)pid;
    return -1;
#endif
}

uint32_t MextEngine::OpenPidfdCount() const
{
    return total_open_pidfds_.load(std::memory_order_relaxed);
}

bool MextEngine::WasPidRecentlyFaulting(int32_t pid, uint64_t within_ns) const
{
    if (pid <= 0) {
        return false;
    }
    const uint64_t now = SteadyNowNs();
    std::lock_guard<std::mutex> lk(mu_);
    auto it = pid_states_.find(pid);
    if (it == pid_states_.end() || it->second.last_steady_ns == 0) {
        return false;
    }
    return (now >= it->second.last_steady_ns) &&
           ((now - it->second.last_steady_ns) <= within_ns);
}

int MextEngine::PageoutPidCold(int mig_fd, int32_t pid, uint32_t nr_pages, bool emergency)
{
    if (pid <= 0 || nr_pages == 0) {
        return 0;
    }
    int fd = mig_fd;
    if (fd < 0) {
        std::lock_guard<std::mutex> lk(mu_);
        fd = mig_fd_;
    }
    if (fd < 0) {
        return -1;
    }

    mext_prefault_msg pf{};
    pf.pid = pid;
    pf.nr_pages = std::min<uint32_t>(nr_pages, MEXT_MAX_PAGEOUT_PAGES);
    pf.flags = MEXT_PREFAULT_FLAG_PAGEOUT_COLD |
               (emergency ? MEXT_PREFAULT_FLAG_FORCE_AGE : 0U);
    pf.vpns_ptr = 0;
    if (ioctl(fd, MEXT_IOCTL_PREFAULT, &pf) != 0) {
        return -1;
    }
    return static_cast<int>(pf.nr_succeeded);
}

bool MextEngine::ExecuteAsyncMadvise(PrefetchShard *shard,
                                     uint32_t page_shift,
                                     const PrefetchWorkItem &item)
{
    bool madv_succeeded = false;

#ifdef __linux__
    /*
     * Sort and coalesce contiguous VPNs into single `iovec` spans so
     * `process_madvise(MADV_WILLNEED)` -> `force_swapin_readahead()` merges
     * adjacent swap pages into a single NVMe bio under one `blk_plug`.
     */
    std::vector<uint64_t> async_vpns = item.prefetch_vpns;
    if (!item.sync_zram_mode && !item.prefault_vpns.empty()) {
        async_vpns.insert(async_vpns.end(),
                          item.prefault_vpns.begin(),
                          item.prefault_vpns.end());
    }
    if (!async_vpns.empty()) {
        int pfd = EnsureShardPidfd(shard, item.pid);
        if (pfd >= 0) {
            std::sort(async_vpns.begin(), async_vpns.end());
            async_vpns.erase(std::unique(async_vpns.begin(), async_vpns.end()),
                             async_vpns.end());

            std::vector<struct iovec> iov;
            iov.reserve(async_vpns.size());
            const size_t page_bytes = 1ULL << page_shift;
            size_t idx = 0;
            while (idx < async_vpns.size()) {
                uint64_t start_vpn = async_vpns[idx];
                size_t run_len = 1U;
                while (idx + run_len < async_vpns.size() &&
                       async_vpns[idx + run_len] == start_vpn + run_len) {
                    ++run_len;
                }
                struct iovec vec{};
                vec.iov_base = reinterpret_cast<void *>(
                    static_cast<uintptr_t>(start_vpn << page_shift));
                vec.iov_len = run_len * page_bytes;
                iov.push_back(vec);
                idx += run_len;
            }
            long ret = syscall(__NR_process_madvise, pfd, iov.data(),
                               iov.size(), MADV_WILLNEED, 0U);
            if (ret >= 0) {
                madv_succeeded = true;
            } else if (errno == ESRCH || errno == EBADF) {
                CloseShardPidfd(shard, item.pid);
            }
        }
    }
#else
    (void)shard;
    (void)page_shift;
    (void)item;
#endif
    return madv_succeeded;
}

void MextEngine::ExecutePrefaultGup(int mig_fd,
                                    const PrefetchWorkItem &item,
                                    bool madv_succeeded)
{
    if (mig_fd < 0) {
        return;
    }
    if (item.sync_zram_mode || !item.prefault_vpns.empty() || !madv_succeeded) {
        /*
         * Populate high-confidence `prefault_vpns` (and full fallback when
         * `process_madvise` is unavailable) via `MEXT_IOCTL_PREFAULT`: in the
         * kernel, `mext_ioctl_prefault` checks `is_swap_pte` first (never
         * touching `pte_none` holes in 4 GiB Firecracker GPA arenas) and
         * cleanly swaps in CoW swap PTEs inside file-backed `MAP_PRIVATE` VMAs
         * (such as `/dev/ublkb2` snapshots, where `MADV_WILLNEED` is a no-op).
         */
        std::vector<uint64_t> gup_vpns = item.prefault_vpns;
        if (!item.sync_zram_mode && !madv_succeeded) {
            gup_vpns.insert(gup_vpns.end(),
                            item.prefetch_vpns.begin(),
                            item.prefetch_vpns.end());
        }
        if (!gup_vpns.empty()) {
            mext_prefault_msg pf{};
            pf.pid = item.pid;
            pf.nr_pages = static_cast<uint32_t>(
                std::min<size_t>(gup_vpns.size(), MEXT_MAX_PREFAULT_PAGES));
            pf.flags = 0U;
            pf.vpns_ptr = static_cast<uint64_t>(
                reinterpret_cast<uintptr_t>(gup_vpns.data()));
            if (ioctl(mig_fd, MEXT_IOCTL_PREFAULT, &pf) == 0 && pf.nr_succeeded > 0U) {
                prefault_populated_.fetch_add(pf.nr_succeeded, std::memory_order_relaxed);
            }
        }
    }
}

void MextEngine::ExecutePrefetchItem(PrefetchShard *shard,
                                     int mig_fd,
                                     uint32_t page_shift,
                                     const PrefetchWorkItem &item)
{
    bool madv_ok = ExecuteAsyncMadvise(shard, page_shift, item);
    ExecutePrefaultGup(mig_fd, item, madv_ok);
}

void MextEngine::PrefetchShardLoop(uint32_t shard_idx)
{
    PrefetchShard &shard = prefetch_shards_[shard_idx];
    while (running_.load(std::memory_order_acquire)) {
        std::vector<PrefetchWorkItem> batch_items;
        std::vector<int32_t> to_close;

        {
            std::unique_lock<std::mutex> slk(shard.mu);
            shard.cv.wait(slk, [&]() {
                return !running_.load(std::memory_order_acquire) ||
                       !shard.queue.empty() ||
                       !shard.pending_close_pids.empty();
            });
            if (!shard.pending_close_pids.empty()) {
                to_close.swap(shard.pending_close_pids);
            }
            while (!shard.queue.empty() && batch_items.size() < 8U) {
                batch_items.push_back(std::move(shard.queue.front()));
                shard.queue.pop_front();
            }
        }

        for (int32_t cpid : to_close) {
            CloseShardPidfd(&shard, cpid);
        }
        if (!running_.load(std::memory_order_acquire)) {
            break;
        }
        if (!batch_items.empty()) {
            int fd = -1;
            uint32_t ps = 12;
            {
                std::lock_guard<std::mutex> lk(mu_);
                fd = mig_fd_;
                ps = page_shift_;
            }
            /*
             * Two-phase dispatch: submit non-blocking `process_madvise(MADV_WILLNEED)`
             * across all queued sandboxes FIRST so NVMe bios enter the block queue
             * with zero head-of-line delay behind another sandbox's GUP!
             */
            std::vector<uint8_t> madv_ok(batch_items.size(), 0U);
            for (size_t k = 0; k < batch_items.size(); ++k) {
                madv_ok[k] = ExecuteAsyncMadvise(&shard, ps, batch_items[k]) ? 1U : 0U;
            }
            for (size_t k = 0; k < batch_items.size(); ++k) {
                ExecutePrefaultGup(fd, batch_items[k], madv_ok[k] != 0U);
            }
        }
    }
}

void MextEngine::DispatchCandidates(int32_t pid,
                                    PidRuntimeState *pst,
                                    const std::vector<CandidatePage> &merged)
{
    if (merged.empty()) {
        return;
    }
    if (record_only_.load(std::memory_order_acquire)) {
        return;
    }

    float eff_prefetch_th = cfg_.prefetch_threshold;
    float eff_prefault_th = cfg_.prefault_threshold;
    uint32_t eff_top_k = cfg_.top_k > 0 ? cfg_.top_k : 16U;

    const uint64_t pid_evicts = shadow_map_.TotalEvictionsForPid(pid);
    const bool shadow_is_complete =
        (pid_evicts > 0) &&
        ((pst->recent_shadow_rate >= 0.45) ||
         (pst->shadow_matched_faults * 2ULL >= pst->fault_count));

    /*
     * Per-PID closed-loop precision gate: when kernel eviction tracking is
     * unavailable (`!shadow_is_complete`) inside memory-capped cgroups (e.g.
     * fragmented Firecracker GPAs on older kernels without `try_to_unmap_one`
     * kprobe) and MADV_WILLNEED minor-fault hit rate drops below 25%, tighten
     * threshold and top-k so low-hit guesses never evict live guest pages.
     */
    const bool sync_zram_mode =
        (cfg_.fault_trigger_stride == 1U && cfg_.prefault_threshold <= 0.55f);
    if (!shadow_is_complete && !sync_zram_mode &&
        pst->pid_evaluated >= 64ULL && (pst->pid_hits * 4ULL < pst->pid_evaluated)) {
        eff_prefetch_th = std::max(eff_prefetch_th, 0.52f);
        eff_top_k = std::min<uint32_t>(eff_top_k, 4U);
    }

    /* PSI guardrail: throttle speculative prefetching under DRAM stall pressure */
    const double cur_psi = static_cast<double>(
                               psi_some_milli_.load(std::memory_order_relaxed)) /
                           1000.0;
    if (cfg_.psi_stall_limit > 0.0 && cur_psi > cfg_.psi_stall_limit) {
        eff_prefetch_th = std::min(0.85f, eff_prefetch_th + 0.15f);
        eff_prefault_th = std::min(0.95f, eff_prefault_th + 0.10f);
        eff_top_k = std::max(2U, eff_top_k / 2U);
    }

    std::vector<uint64_t> prefetch_vpns;
    std::vector<uint64_t> prefault_vpns;
    prefetch_vpns.reserve(eff_top_k);
    prefault_vpns.reserve(eff_top_k);

    uint32_t selected = 0;
    for (const auto &cand : merged) {
        if (selected >= eff_top_k) {
            break;
        }
        if (cand.vpn == 0 || cand.prob < eff_prefetch_th) {
            continue;
        }
        if (shadow_map_.WasRecentlyPrefetched(pid, cand.vpn)) {
            continue;
        }
        if (shadow_is_complete && !shadow_map_.IsSwappedOut(pid, cand.vpn)) {
            continue;
        }

        shadow_map_.MarkPrefetched(pid, cand.vpn);
        pst->predicted_ring[pst->predicted_cursor & 511U] = cand.vpn;
        ++pst->predicted_cursor;
        ++pst->pid_evaluated;
        ++predictions_evaluated_;

        if (cfg_.enable_prefault && cand.prob >= eff_prefault_th) {
            prefault_vpns.push_back(cand.vpn);
        } else {
            prefetch_vpns.push_back(cand.vpn);
        }
        ++selected;
    }

    if (prefetch_vpns.empty() && prefault_vpns.empty()) {
        return;
    }

    if (dispatch_cb_) {
        dispatch_cb_(pid, prefetch_vpns, prefault_vpns);
    }

    prefetch_issued_ += prefetch_vpns.size();
    prefault_issued_ += prefault_vpns.size();

    PrefetchWorkItem item{};
    item.pid = pid;
    item.sync_zram_mode = sync_zram_mode;
    item.prefetch_vpns = std::move(prefetch_vpns);
    item.prefault_vpns = std::move(prefault_vpns);

    PrefetchShard &shard =
        prefetch_shards_[static_cast<uint32_t>(pid) & (kPrefetchShards - 1U)];
    if (async_pool_active_ && !sync_zram_mode) {
        {
            std::lock_guard<std::mutex> slk(shard.mu);
            /* Replace stale queued batch for the same PID if queue is busy */
            bool replaced = false;
            if (shard.queue.size() >= 16U) {
                for (auto &q : shard.queue) {
                    if (q.pid == pid) {
                        q = std::move(item);
                        replaced = true;
                        break;
                    }
                }
            }
            if (!replaced) {
                if (shard.queue.size() >= kMaxShardQueue) {
                    shard.queue.pop_front();
                }
                shard.queue.push_back(std::move(item));
            }
        }
        shard.cv.notify_one();
    } else {
        ExecutePrefetchItem(&shard, mig_fd_, page_shift_, item);
    }
}

void MextEngine::HandleFaultEventLocked(int32_t pid, uint64_t vpn, uint8_t vma_slot,
                                        uint64_t timestamp_ns, bool emit_predictions)
{
    if (pid <= 0 || vpn == 0 || !cfg_.enabled) {
        return;
    }

    uint64_t now_ns = (timestamp_ns > 0) ? timestamp_ns : SteadyNowNs();
    PidRuntimeState &pst = pid_states_[pid];

    /*
     * Filter Linux `do_swap_page` VM_FAULT_RETRY second-pass duplicates (even
     * across 2-16 interleaved vCPUs within 50 ms when the page was not evicted
     * again in between). Prevents polluting `untracked_faults`, KV cache tokens,
     * and odd/even `fault_trigger_stride` parity for Transformer Head B.
     */
    if (!shadow_map_.IsSwappedOut(pid, vpn) && now_ns > 0) {
        for (uint32_t s = 0; s < kRecentFaultSlots; ++s) {
            if (pst.recent_faults[s].vpn == vpn &&
                now_ns >= pst.recent_faults[s].ts_ns &&
                (now_ns - pst.recent_faults[s].ts_ns) <= kRetryDedupWindowNs) {
                ++retry_dups_skipped_;
                return;
            }
        }
    }
    pst.recent_faults[pst.recent_fault_cursor & (kRecentFaultSlots - 1U)] = { vpn, now_ns };
    ++pst.recent_fault_cursor;

    ++faults_processed_;
    ++pst.fault_count;

    /* Check whether this access matches a previously predicted page */
    for (uint32_t i = 0; i < 512U; ++i) {
        if (pst.predicted_ring[i] == vpn) {
            ++prediction_hits_;
            ++pst.pid_hits;
            pst.predicted_ring[i] = 0;
            break;
        }
    }

    /* Look up eviction sequence BEFORE OnPageFault marks it resident */
    uint64_t evict_seq = 0;
    uint64_t evict_epoch = 0;
    bool has_evict = shadow_map_.OnPageFault(pid, vpn, &evict_seq, &evict_epoch);
    if (has_evict) {
        ++pst.shadow_matched_faults;
        ++shadow_matched_faults_;
        pst.recent_shadow_rate = (pst.fault_count == 1ULL)
                                     ? 1.0
                                     : (0.95 * pst.recent_shadow_rate + 0.05);
    } else {
        ++pst.untracked_faults;
        pst.recent_shadow_rate = 0.95 * pst.recent_shadow_rate;
    }

    /* Update delayed self-supervised labels for online output-head adaptation */
    for (auto &pend : pst.pending) {
        int64_t dvpn = static_cast<int64_t>(vpn) - static_cast<int64_t>(pend.anchor_vpn);
        MarkTargetOffset(pend.target_mask, false, dvpn);
        if (pend.has_evict_seq && has_evict) {
            int64_t dseq = static_cast<int64_t>(evict_seq) - static_cast<int64_t>(pend.anchor_evict_seq);
            MarkTargetOffset(pend.target_mask, true, dseq);
        }
        if (pend.remaining_horizon > 0) {
            --pend.remaining_horizon;
        }
    }
    while (!pst.pending.empty() && pst.pending.front().remaining_horizon == 0) {
        if (cfg_.enable_online_adapt) {
            transformer_.OnlineAdaptHead(pst.pending.front().hidden, pst.pending.front().target_mask, 0.015f);
            ++online_train_steps_;
        }
        pst.pending.pop_front();
    }

    /* Multi-space delta tokenization */
    TokenEvent tok{};
    int64_t dvpn = pst.has_last_vpn ? (static_cast<int64_t>(vpn) - static_cast<int64_t>(pst.last_vpn)) : 0;
    int64_t dseq = (pst.has_last_evict && has_evict)
                       ? (static_cast<int64_t>(evict_seq) - static_cast<int64_t>(pst.last_evict_seq))
                       : 0;
    uint64_t dt_ns = (pst.last_ts_ns > 0 && now_ns >= pst.last_ts_ns) ? (now_ns - pst.last_ts_ns) : 0;

    tok.dvpn_tok = EncodeDeltaToken(dvpn, pst.has_last_vpn);
    tok.devict_tok = EncodeDeltaToken(dseq, pst.has_last_evict && has_evict);
    tok.vma_tok = vma_slot & (MEXT_VMA_VOCAB - 1U);
    tok.dt_tok = EncodeDtToken(dt_ns);

    pst.last_vpn = vpn;
    pst.has_last_vpn = true;
    pst.last_ts_ns = now_ns;
    pst.last_steady_ns = SteadyNowNs();
    if (has_evict) {
        pst.last_evict_seq = evict_seq;
        pst.has_last_evict = true;
    }

    /* Tier 1: Sub-microsecond heuristic candidates */
    std::vector<CandidatePage> raw_cands;
    if (emit_predictions) {
        raw_cands.reserve(32);
    }
    tier1_.ObserveAndPredict(pid, vpn, vma_slot, has_evict, evict_seq, evict_epoch,
                             shadow_map_, emit_predictions ? &raw_cands : nullptr);
    if (!emit_predictions) {
        return;
    }
    tier1_preds_ += raw_cands.size();

    /* Tier 2: INT8 TinyTransformer with circular RoPE KV cache */
    if (cfg_.enable_transformer) {
        uint32_t stride = cfg_.fault_trigger_stride > 0 ? cfg_.fault_trigger_stride : 4U;
        bool run_head = ((pst.fault_count % stride) == 0);

        float probs[MEXT_OUT_DIM];
        float hidden[TF_D_MODEL];
        uint64_t t0 = SteadyNowNs();
        transformer_.ForwardStep(pid, tok, run_head, run_head ? probs : nullptr, run_head ? hidden : nullptr);
        uint64_t t1 = SteadyNowNs();

        if (run_head) {
            double us = static_cast<double>(t1 - t0) / 1000.0;
            avg_infer_us_ = (avg_infer_us_ == 0.0) ? us : (0.92 * avg_infer_us_ + 0.08 * us);

            /* Decode Head A (dVPN) & Head B (dEvictSeq) */
            float min_th = std::max(0.15f, cfg_.prefetch_threshold * 0.8f);
            for (uint32_t k = 0; k < MEXT_ACTION_OFFSETS; ++k) {
                float pa = probs[k];
                if (pa >= min_th) {
                    int64_t target = static_cast<int64_t>(vpn) + ActionIndexToOffset(k);
                    if (target > 0) {
                        raw_cands.push_back(CandidatePage{ static_cast<uint64_t>(target), pa });
                        ++tf_preds_;
                    }
                }

                float pb = probs[MEXT_ACTION_OFFSETS + k];
                if (has_evict && pb >= min_th) {
                    uint64_t nbr_vpn = 0;
                    if (shadow_map_.ResolveEvictNeighbor(pid, evict_seq, ActionIndexToOffset(k), &nbr_vpn)) {
                        raw_cands.push_back(CandidatePage{ nbr_vpn, pb });
                        ++tf_preds_;
                    }
                }
            }

            if (cfg_.enable_online_adapt && pst.pending.size() < 16) {
                PendingSupervision sup{};
                sup.anchor_vpn = vpn;
                sup.anchor_evict_seq = evict_seq;
                sup.has_evict_seq = has_evict;
                sup.remaining_horizon = 16;
                std::memcpy(sup.hidden, hidden, sizeof(hidden));
                pst.pending.push_back(sup);
            }
        }
    }

    if (raw_cands.empty()) {
        return;
    }

    /* Deduplicate & combine probabilities via Noisy-OR: p = 1 - prod(1 - p_i) */
    std::sort(raw_cands.begin(), raw_cands.end(),
              [](const CandidatePage &a, const CandidatePage &b) { return a.vpn < b.vpn; });

    std::vector<CandidatePage> merged;
    merged.reserve(raw_cands.size());
    for (const auto &c : raw_cands) {
        if (c.vpn == vpn) {
            continue;
        }
        if (!merged.empty() && merged.back().vpn == c.vpn) {
            float p1 = merged.back().prob;
            float p2 = c.prob;
            merged.back().prob = 1.0f - (1.0f - p1) * (1.0f - p2);
        } else {
            merged.push_back(c);
        }
    }

    std::sort(merged.begin(), merged.end(),
              [](const CandidatePage &a, const CandidatePage &b) { return a.prob > b.prob; });

    DispatchCandidates(pid, &pst, merged);
}

void MextEngine::HandleFaultEvent(int32_t pid, uint64_t vpn, uint8_t vma_slot,
                                  uint64_t timestamp_ns, bool emit_predictions)
{
    if (pid <= 0 || vpn == 0) {
        return;
    }
    std::lock_guard<std::mutex> lk(mu_);
    HandleFaultEventLocked(pid, vpn, vma_slot, timestamp_ns, emit_predictions);
}

MextStats MextEngine::GetStatsSnapshot()
{
    std::lock_guard<std::mutex> lk(mu_);
    MextStats st{};
    st.faults_processed = faults_processed_;
    st.evicts_tracked = evicts_tracked_;
    st.shadow_active_pages = shadow_map_.TotalActivePages();
    st.tier1_predictions = tier1_preds_;
    st.transformer_predictions = tf_preds_;
    st.prefetch_issued = prefetch_issued_;
    st.prefault_issued = prefault_issued_;
    st.prediction_hits = prediction_hits_;
    st.online_train_steps = online_train_steps_;
    st.kernel_ring_dropped = mmap_ring_ ? mmap_ring_->hdr.dropped : 0;
    st.avg_infer_us = avg_infer_us_;
    st.hit_rate = (predictions_evaluated_ > 0)
                      ? (static_cast<double>(prediction_hits_) / static_cast<double>(predictions_evaluated_))
                      : 0.0;
    st.psi_some_avg10 = static_cast<double>(
                            psi_some_milli_.load(std::memory_order_relaxed)) /
                        1000.0;
    return st;
}

MextEngine &GlobalMextEngine()
{
    static MextEngine instance;
    return instance;
}

namespace {
MextEngine &GlobalEngine()
{
    return GlobalMextEngine();
}
} // namespace

} // namespace mext
} // namespace smap

extern "C" {

void MextDefaultConfig(MextConfig *cfg)
{
    if (!cfg) {
        return;
    }
    std::memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = true;
    cfg->enable_transformer = true;
    cfg->enable_online_adapt = true;
    cfg->enable_prefault = true;
    cfg->fault_trigger_stride = 4;
    cfg->top_k = 16;
    cfg->prefetch_threshold = 0.25f;
    cfg->prefault_threshold = 0.75f;
    cfg->psi_stall_limit = 25.0;
}

void MextApplyZramPreset(MextConfig *cfg)
{
    if (!cfg) {
        return;
    }
    /*
     * zram (SWP_SYNCHRONOUS_IO) tuning:
     *   1. Favor SMAP_MIG_PREFAULT (threshold 0.50) over MADV_WILLNEED (0.38) so
     *      decompressed pages install directly into PTEs and immediately free the
     *      compressed zram slot, avoiding DRAM Swap-Cache + zram double-buffering.
     *   2. Because PTE-prefaulted pages bypass do_swap_page completely, the fault
     *      stream is already naturally subsampled by ~top_k (only 1 boundary fault
     *      per prefault batch). Use fault_trigger_stride = 1 so every batch-boundary
     *      miss immediately renews the next lookahead batch (~0.37 us/page amortized).
     */
    cfg->enabled = true;
    cfg->enable_transformer = true;
    cfg->enable_online_adapt = true;
    cfg->enable_prefault = true;
    cfg->fault_trigger_stride = 1;
    cfg->top_k = 12;
    cfg->prefetch_threshold = 0.38f;
    cfg->prefault_threshold = 0.50f;
    cfg->psi_stall_limit = 20.0;
}

int MextEngineStart(int mig_fd, const MextConfig *cfg)
{
    MextConfig local_cfg{};
    if (cfg) {
        local_cfg = *cfg;
    } else {
        MextDefaultConfig(&local_cfg);
    }
    return smap::mext::GlobalEngine().Start(mig_fd, local_cfg, true);
}

void MextEngineStop(void)
{
    smap::mext::GlobalEngine().Stop();
}

bool MextEngineIsRunning(void)
{
    return smap::mext::GlobalEngine().IsRunning();
}

int MextEngineUpdateConfig(const MextConfig *cfg)
{
    if (!cfg) {
        return -1;
    }
    return smap::mext::GlobalEngine().UpdateConfig(*cfg);
}

int MextEngineTrackPid(pid_t pid)
{
    return smap::mext::GlobalEngine().TrackPid(static_cast<int32_t>(pid));
}

void MextEngineUntrackPid(pid_t pid)
{
    smap::mext::GlobalEngine().UntrackPid(static_cast<int32_t>(pid));
}

int MextEnginePageoutPidCold(int mig_fd, pid_t pid, uint32_t nr_pages, bool emergency)
{
    return smap::mext::GlobalEngine().PageoutPidCold(
        mig_fd, static_cast<int32_t>(pid), nr_pages, emergency);
}

bool MextEngineWasPidRecentlyFaulting(pid_t pid, uint64_t within_ns)
{
    return smap::mext::GlobalEngine().WasPidRecentlyFaulting(
        static_cast<int32_t>(pid), within_ns);
}

int MextEngineQueryStats(MextStats *stats_out)
{
    if (!stats_out) {
        return -1;
    }
    *stats_out = smap::mext::GlobalEngine().GetStatsSnapshot();
    return 0;
}

} /* extern "C" */
