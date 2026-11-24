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
        worker_ = std::thread(&MextEngine::WorkerLoop, this);
    }
    return 0;
}

void MextEngine::Stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }

    std::lock_guard<std::mutex> lk(mu_);
    if (mig_fd_ >= 0) {
        mext_ctrl_msg ctrl{};
        ctrl.op = MEXT_CTRL_DISABLE;
        (void)ioctl(mig_fd_, MEXT_IOCTL_CTRL, &ctrl);
    }
    for (auto &kv : pid_states_) {
        if (kv.second.pidfd >= 0) {
            close(kv.second.pidfd);
            kv.second.pidfd = -1;
        }
    }
    pid_states_.clear();
    shadow_map_.Clear();
    tier1_.Clear();
    transformer_.ClearState();

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
    {
        std::lock_guard<std::mutex> lk(mu_);
        fd = mig_fd_;
        auto it = pid_states_.find(pid);
        if (it != pid_states_.end()) {
            if (it->second.pidfd >= 0) {
                close(it->second.pidfd);
            }
            pid_states_.erase(it);
        }
        shadow_map_.RemovePid(pid);
        tier1_.RemovePid(pid);
        transformer_.ResetPid(pid);
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
    batch.reserve(std::min<size_t>(max_events, 64));
    PressureCallback pressure_cb;

    {
        std::lock_guard<std::mutex> lk(mu_);
        pressure_cb = pressure_cb_;
        mext_ring_event ev{};
        while (batch.size() < max_events && PopRingEvent(&ev)) {
            batch.push_back(ev);
        }
    }

    if (batch.empty()) {
        return 0;
    }

    /*
     * Frontier Coalescing: when fast synchronous swap backends (like zram) burst
     * multiple faults before the worker wakes up, fast-forward intermediate faults
     * through state tracking only and emit predictions/prefaults exclusively from
     * each PID's newest frontier fault so the helper thread never chases stale VAs.
     */
    std::unordered_map<uint32_t, size_t> last_fault_idx;
    for (size_t i = 0; i < batch.size(); ++i) {
        if (batch[i].type == MEXT_EVT_SWAP_FAULT) {
            last_fault_idx[batch[i].pid] = i;
        }
    }

    for (size_t i = 0; i < batch.size(); ++i) {
        const auto &ev = batch[i];
        if (ev.type == MEXT_EVT_EVICTED) {
            HandleEvictionEvent(static_cast<int32_t>(ev.pid), ev.vpn, ev.aux);
        } else if (ev.type == MEXT_EVT_SWAP_FAULT) {
            bool is_frontier = (last_fault_idx[ev.pid] == i);
            HandleFaultEvent(static_cast<int32_t>(ev.pid), ev.vpn,
                             static_cast<uint8_t>(ev.aux & 0x1fULL), ev.timestamp, is_frontier);
        } else if (ev.type == MEXT_EVT_MEM_PRESSURE) {
            if (pressure_cb) {
                uint64_t wmark_low = ev.aux >> 16;
                uint16_t order = static_cast<uint16_t>(ev.aux & 0xffffULL);
                pressure_cb(ev.pid, ev.vpn, wmark_low, order, ev.timestamp);
            }
        }
    }
    return batch.size();
}

void MextEngine::WorkerLoop()
{
    uint32_t idle_spins = 0;
    while (running_.load(std::memory_order_acquire)) {
        size_t processed = ProcessRingOnce(256);
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

void MextEngine::HandleEvictionEvent(int32_t pid, uint64_t vpn, uint64_t epoch)
{
    std::lock_guard<std::mutex> lk(mu_);
    if (shadow_map_.RecordEviction(pid, vpn, epoch) > 0) {
        ++evicts_tracked_;
    }
}

void MextEngine::RefreshPsiIfNeeded(uint64_t now_ns)
{
#ifdef __linux__
    if (now_ns - last_psi_check_ns_ < 100000000ULL) { /* 100 ms */
        return;
    }
    last_psi_check_ns_ = now_ns;
    FILE *f = std::fopen("/proc/pressure/memory", "r");
    if (!f) {
        return;
    }
    char line[256];
    if (std::fgets(line, sizeof(line), f)) {
        double avg10 = 0.0;
        if (std::sscanf(line, "some avg10=%lf", &avg10) == 1) {
            psi_some_avg10_ = avg10;
        }
    }
    std::fclose(f);
#else
    (void)now_ns;
    (void)last_psi_check_ns_;
#endif
}

int MextEngine::EnsurePidfd(int32_t pid, PidRuntimeState *pst)
{
#ifdef __linux__
    if (pst->pidfd >= 0) {
        return pst->pidfd;
    }
    long fd = syscall(__NR_pidfd_open, pid, 0U);
    if (fd >= 0) {
        pst->pidfd = static_cast<int>(fd);
    }
    return pst->pidfd;
#else
    (void)pid;
    (void)pst;
    return -1;
#endif
}

void MextEngine::DispatchCandidates(int32_t pid,
                                    PidRuntimeState *pst,
                                    const std::vector<CandidatePage> &merged)
{
    if (merged.empty()) {
        return;
    }

    float eff_prefetch_th = cfg_.prefetch_threshold;
    float eff_prefault_th = cfg_.prefault_threshold;
    uint32_t eff_top_k = cfg_.top_k > 0 ? cfg_.top_k : 16U;

    /* PSI guardrail: throttle speculative prefetching under DRAM stall pressure */
    if (cfg_.psi_stall_limit > 0.0 && psi_some_avg10_ > cfg_.psi_stall_limit) {
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
        /* If shadow map has tracked pages for this PID, skip pages known to be resident */
        if (evicts_tracked_ > 0 && !shadow_map_.IsSwappedOut(pid, cand.vpn)) {
            continue;
        }

        shadow_map_.MarkPrefetched(pid, cand.vpn);
        pst->predicted_ring[pst->predicted_cursor & 511U] = cand.vpn;
        ++pst->predicted_cursor;
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

#ifdef __linux__
    if (!prefetch_vpns.empty()) {
        int pfd = EnsurePidfd(pid, pst);
        if (pfd >= 0) {
            std::vector<struct iovec> iov(prefetch_vpns.size());
            size_t page_bytes = 1ULL << page_shift_;
            for (size_t i = 0; i < prefetch_vpns.size(); ++i) {
                iov[i].iov_base = reinterpret_cast<void *>(static_cast<uintptr_t>(prefetch_vpns[i] << page_shift_));
                iov[i].iov_len = page_bytes;
            }
            (void)syscall(__NR_process_madvise, pfd, iov.data(), iov.size(), MADV_WILLNEED, 0U);
        }
    }
#endif

    if (!prefault_vpns.empty() && mig_fd_ >= 0) {
        mext_prefault_msg pf{};
        pf.pid = pid;
        pf.nr_pages = static_cast<uint32_t>(std::min<size_t>(prefault_vpns.size(), MEXT_MAX_PREFAULT_PAGES));
        pf.flags = 0;
        pf.vpns_ptr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(prefault_vpns.data()));
        (void)ioctl(mig_fd_, MEXT_IOCTL_PREFAULT, &pf);
    }

    prefetch_issued_ += prefetch_vpns.size();
    prefault_issued_ += prefault_vpns.size();
}

void MextEngine::HandleFaultEvent(int32_t pid, uint64_t vpn, uint8_t vma_slot,
                                  uint64_t timestamp_ns, bool emit_predictions)
{
    if (pid <= 0 || vpn == 0) {
        return;
    }

    std::lock_guard<std::mutex> lk(mu_);
    if (!cfg_.enabled) {
        return;
    }

    ++faults_processed_;
    uint64_t now_ns = (timestamp_ns > 0) ? timestamp_ns : SteadyNowNs();
    RefreshPsiIfNeeded(now_ns);

    PidRuntimeState &pst = pid_states_[pid];
    ++pst.fault_count;

    /* Check whether this access matches a previously predicted page */
    for (uint32_t i = 0; i < 512U; ++i) {
        if (pst.predicted_ring[i] == vpn) {
            ++prediction_hits_;
            pst.predicted_ring[i] = 0;
            break;
        }
    }

    /* Look up eviction sequence BEFORE OnPageFault marks it resident */
    uint64_t evict_seq = 0;
    uint64_t evict_epoch = 0;
    bool has_evict = shadow_map_.OnPageFault(pid, vpn, &evict_seq, &evict_epoch);

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
    st.psi_some_avg10 = psi_some_avg10_;
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

int MextEngineQueryStats(MextStats *stats_out)
{
    if (!stats_out) {
        return -1;
    }
    *stats_out = smap::mext::GlobalEngine().GetStatsSnapshot();
    return 0;
}

} /* extern "C" */
