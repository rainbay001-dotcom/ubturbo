/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Dynamic Proactive Reclaim Controller implementation.
 */

#include "proactive_reclaim.h"
#include "proactive_reclaim_c_api.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace smap {
namespace reclaim {

namespace {

uint64_t SteadyNowNs()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

uint8_t ClampColdWindow(uint32_t w)
{
    if (w < 1U) {
        return 1U;
    }
    if (w > COLD_TRACKER_MAX_WINDOWS) {
        return static_cast<uint8_t>(COLD_TRACKER_MAX_WINDOWS);
    }
    return static_cast<uint8_t>(w);
}

void ParseMeminfo(const char *path, uint32_t page_shift, HostMemSnapshot *out)
{
    FILE *fp = std::fopen(path, "r");
    if (!fp) {
        return;
    }
    char line[256];
    uint64_t mem_total_kb = 0;
    uint64_t mem_free_kb = 0;
    uint64_t mem_avail_kb = 0;
    bool has_avail = false;

    while (std::fgets(line, sizeof(line), fp) != nullptr) {
        unsigned long long val = 0;
        if (std::strncmp(line, "MemTotal:", 9) == 0) {
            if (std::sscanf(line + 9, "%llu", &val) == 1) {
                mem_total_kb = static_cast<uint64_t>(val);
            }
        } else if (std::strncmp(line, "MemFree:", 8) == 0) {
            if (std::sscanf(line + 8, "%llu", &val) == 1) {
                mem_free_kb = static_cast<uint64_t>(val);
            }
        } else if (std::strncmp(line, "MemAvailable:", 13) == 0) {
            if (std::sscanf(line + 13, "%llu", &val) == 1) {
                mem_avail_kb = static_cast<uint64_t>(val);
                has_avail = true;
            }
        }
    }
    std::fclose(fp);

    const uint32_t kb_to_page_shift = (page_shift >= 10U) ? (page_shift - 10U) : 2U;
    out->total_pages = mem_total_kb >> kb_to_page_shift;
    uint64_t eff_kb = has_avail ? mem_avail_kb : mem_free_kb;
    out->free_pages = eff_kb >> kb_to_page_shift;
}

void ParseZoneinfo(const char *path, HostMemSnapshot *out)
{
    FILE *fp = std::fopen(path, "r");
    if (!fp) {
        return;
    }
    char line[256];
    uint64_t sum_min = 0;
    uint64_t sum_low = 0;
    uint64_t sum_high = 0;
    uint64_t sum_zone_free = 0;
    bool got_zone_free = false;

    while (std::fgets(line, sizeof(line), fp) != nullptr) {
        const char *p = line;
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
        unsigned long long val = 0;
        if (std::strncmp(p, "pages free", 10) == 0) {
            if (std::sscanf(p + 10, "%llu", &val) == 1) {
                sum_zone_free += static_cast<uint64_t>(val);
                got_zone_free = true;
            }
        } else if (std::strncmp(p, "min ", 4) == 0 || std::strncmp(p, "min\t", 4) == 0) {
            if (std::sscanf(p + 3, "%llu", &val) == 1) {
                sum_min += static_cast<uint64_t>(val);
            }
        } else if (std::strncmp(p, "low ", 4) == 0 || std::strncmp(p, "low\t", 4) == 0) {
            if (std::sscanf(p + 3, "%llu", &val) == 1) {
                sum_low += static_cast<uint64_t>(val);
            }
        } else if (std::strncmp(p, "high ", 5) == 0 || std::strncmp(p, "high\t", 5) == 0) {
            if (std::sscanf(p + 4, "%llu", &val) == 1) {
                sum_high += static_cast<uint64_t>(val);
            }
        }
    }
    std::fclose(fp);

    if (sum_low > 0) {
        out->wmark_min_pages = sum_min;
        out->wmark_low_pages = sum_low;
        out->wmark_high_pages = (sum_high >= sum_low) ? sum_high : (sum_low * 6ULL / 5ULL);
    }
    /*
     * When reclaimable page cache is tiny, zone NR_FREE_PAGES directly governs
     * whether __alloc_pages_slowpath wakes kswapd or enters direct reclaim.
     */
    if (got_zone_free && (out->free_pages == 0 || sum_zone_free < out->free_pages)) {
        out->free_pages = sum_zone_free;
    }
}

void ParseVmstat(const char *path, HostMemSnapshot *out)
{
    FILE *fp = std::fopen(path, "r");
    if (!fp) {
        return;
    }
    char line[256];
    uint64_t pgalloc = 0;
    uint64_t kswapd_steal = 0;
    uint64_t direct_steal = 0;
    uint64_t stalls = 0;

    while (std::fgets(line, sizeof(line), fp) != nullptr) {
        char key[64] = { 0 };
        unsigned long long val = 0;
        if (std::sscanf(line, "%63s %llu", key, &val) != 2) {
            continue;
        }
        if (std::strncmp(key, "pgalloc_", 8) == 0) {
            pgalloc += static_cast<uint64_t>(val);
        } else if (std::strncmp(key, "pgsteal_kswapd", 14) == 0) {
            kswapd_steal += static_cast<uint64_t>(val);
        } else if (std::strncmp(key, "pgsteal_direct", 14) == 0) {
            direct_steal += static_cast<uint64_t>(val);
        } else if (std::strncmp(key, "allocstall_", 11) == 0) {
            stalls += static_cast<uint64_t>(val);
        }
    }
    std::fclose(fp);

    out->pgalloc_total = pgalloc;
    out->pgsteal_kswapd = kswapd_steal;
    out->pgsteal_direct = direct_steal;
    out->allocstall_total = stalls;
}

void ParseMemoryPsi(const char *path, HostMemSnapshot *out)
{
    FILE *fp = std::fopen(path, "r");
    if (!fp) {
        return;
    }
    char line[256];
    while (std::fgets(line, sizeof(line), fp) != nullptr) {
        if (std::strncmp(line, "some ", 5) == 0) {
            const char *p = std::strstr(line, "avg10=");
            if (p) {
                double v = 0.0;
                if (std::sscanf(p + 6, "%lf", &v) == 1) {
                    out->psi_some_avg10 = v;
                }
            }
        } else if (std::strncmp(line, "full ", 5) == 0) {
            const char *p = std::strstr(line, "avg10=");
            if (p) {
                double v = 0.0;
                if (std::sscanf(p + 6, "%lf", &v) == 1) {
                    out->psi_full_avg10 = v;
                }
            }
        }
    }
    std::fclose(fp);
}

} // namespace

ProactiveReclaimController::ProactiveReclaimController()
{
    ProactiveReclaimDefaultConfig(&cfg_);
    long ps = sysconf(_SC_PAGESIZE);
    page_shift_ = (ps == 65536) ? 16U : 12U;
}

ProactiveReclaimController::~ProactiveReclaimController()
{
    Stop();
}

bool ProactiveReclaimController::ReadLinuxHostSnapshotFromRoot(const char *proc_root,
                                                               uint32_t page_shift,
                                                               HostMemSnapshot *out)
{
    if (!proc_root || !out) {
        return false;
    }
    *out = HostMemSnapshot{};
    out->timestamp_ns = SteadyNowNs();

    char path[512];
    std::snprintf(path, sizeof(path), "%s/meminfo", proc_root);
    ParseMeminfo(path, page_shift, out);
    if (out->total_pages == 0) {
        return false;
    }

    std::snprintf(path, sizeof(path), "%s/zoneinfo", proc_root);
    ParseZoneinfo(path, out);
    if (out->wmark_low_pages == 0) {
        out->wmark_min_pages = std::max<uint64_t>(1024ULL, out->total_pages / 256ULL);
        out->wmark_low_pages = (out->wmark_min_pages * 5ULL) / 4ULL;
        out->wmark_high_pages = (out->wmark_min_pages * 3ULL) / 2ULL;
    }

    std::snprintf(path, sizeof(path), "%s/vmstat", proc_root);
    ParseVmstat(path, out);

    std::snprintf(path, sizeof(path), "%s/pressure/memory", proc_root);
    ParseMemoryPsi(path, out);
    return true;
}

bool ProactiveReclaimController::ReadLinuxHostSnapshot(uint32_t page_shift, HostMemSnapshot *out)
{
    return ReadLinuxHostSnapshotFromRoot("/proc", page_shift, out);
}

int ProactiveReclaimController::Start(const ProactiveReclaimConfig &cfg,
                                      EvictExecutor evict_fn,
                                      bool spawn_worker)
{
    if (running_.load(std::memory_order_acquire)) {
        SetEvictExecutor(std::move(evict_fn));
        return UpdateConfig(cfg);
    }

    {
        std::lock_guard<std::mutex> lk(mu_);
        cfg_ = cfg;
        if (evict_fn) {
            evict_fn_ = std::move(evict_fn);
        }
        has_prev_snap_ = false;
        prev_reclaimed_pages_ = 0;
        alloc_vel_pps_ = 0.0;
        alloc_accel_pps2_ = 0.0;
        wakeup_pending_ = false;
        kernel_pressure_pending_ = false;
        urgent_pages_pending_ = 0;
        hint_zone_free_pages_ = 0;
        hint_zone_wmark_low_ = 0;
        stats_ = ProactiveReclaimStats{};
        stats_.thrash_brake_scale = 1.0;
        uint8_t init_cw = ClampColdWindow(cfg_.calm_cold_window);
        stats_.effective_cold_window = init_cw;
        recommended_cold_window_.store(init_cw, std::memory_order_release);
        current_urgency_.store(static_cast<uint32_t>(RECLAIM_CALM), std::memory_order_release);
        should_track_cold_.store(false, std::memory_order_release);
    }

    mext::MextEngine *eng = bound_engine_;
    if (!eng) {
        eng = &mext::GlobalMextEngine();
    }
    eng->SetPressureCallback([this](uint32_t nid, uint64_t free_pages,
                                    uint64_t wmark_low, uint16_t order, uint64_t ts) {
        this->NotifyKernelPressure(nid, free_pages, wmark_low, order, ts);
    });

    running_.store(true, std::memory_order_release);
    if (spawn_worker) {
        worker_ = std::thread(&ProactiveReclaimController::WorkerLoop, this);
    }
    return 0;
}

void ProactiveReclaimController::Stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        return;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }

    mext::MextEngine *eng = bound_engine_ ? bound_engine_ : &mext::GlobalMextEngine();
    eng->SetPressureCallback(nullptr);
}

int ProactiveReclaimController::UpdateConfig(const ProactiveReclaimConfig &cfg)
{
    {
        std::lock_guard<std::mutex> lk(mu_);
        cfg_ = cfg;
    }
    cv_.notify_one();
    return 0;
}

void ProactiveReclaimController::SetTelemetryProvider(TelemetryProvider provider)
{
    std::lock_guard<std::mutex> lk(mu_);
    telemetry_fn_ = std::move(provider);
}

void ProactiveReclaimController::SetMextFeedbackProvider(MextFeedbackProvider provider)
{
    std::lock_guard<std::mutex> lk(mu_);
    mext_feedback_fn_ = std::move(provider);
}

void ProactiveReclaimController::SetEvictExecutor(EvictExecutor evict_fn)
{
    std::lock_guard<std::mutex> lk(mu_);
    evict_fn_ = std::move(evict_fn);
}

void ProactiveReclaimController::BindMextEngine(mext::MextEngine *engine)
{
    std::lock_guard<std::mutex> lk(mu_);
    bound_engine_ = engine;
}

void ProactiveReclaimController::NotifyKernelPressure(uint32_t nid,
                                                      uint64_t free_pages,
                                                      uint64_t wmark_low_pages,
                                                      uint16_t order,
                                                      uint64_t timestamp_ns)
{
    (void)nid;
    (void)order;
    (void)timestamp_ns;
    {
        std::lock_guard<std::mutex> lk(mu_);
        wakeup_pending_ = true;
        kernel_pressure_pending_ = true;
        hint_zone_free_pages_ = free_pages;
        hint_zone_wmark_low_ = wmark_low_pages;
        ++stats_.proactive_wakeups;
    }
    cv_.notify_one();
}

void ProactiveReclaimController::TriggerUrgentBytes(uint64_t urgent_bytes)
{
    uint64_t pages = urgent_bytes >> page_shift_;
    if (pages == 0 && urgent_bytes > 0) {
        pages = 1ULL;
    }
    {
        std::lock_guard<std::mutex> lk(mu_);
        wakeup_pending_ = true;
        urgent_pages_pending_ += pages;
        ++stats_.proactive_wakeups;
    }
    cv_.notify_one();
}

ReclaimDecision ProactiveReclaimController::EvaluateLocked(const HostMemSnapshot &snap,
                                                           const MextFeedbackSnapshot &mext_snap,
                                                           uint64_t urgent_pages,
                                                           bool kernel_pressure)
{
    ReclaimDecision dec{};
    const double tau = std::max(0.005, static_cast<double>(cfg_.lookahead_horizon_ms) * 1e-3);
    const double poll_dt_cfg = std::max(0.001, static_cast<double>(cfg_.poll_interval_ms) * 1e-3);
    const double base_ratio = std::max(1.0, cfg_.base_headroom_ratio);
    const double max_ratio = std::clamp(cfg_.max_headroom_ratio, 0.02, 0.50);

    uint64_t w_low = std::max<uint64_t>(64ULL, snap.wmark_low_pages);
    uint64_t w_min = (snap.wmark_min_pages > 0 && snap.wmark_min_pages <= w_low)
                         ? snap.wmark_min_pages
                         : ((w_low * 4ULL) / 5ULL);
    uint64_t w_base = static_cast<uint64_t>(std::ceil(static_cast<double>(w_low) * base_ratio));
    uint64_t w_max = std::max<uint64_t>(
        w_base, static_cast<uint64_t>(static_cast<double>(snap.total_pages) * max_ratio));

    double dt_sec = poll_dt_cfg;
    uint64_t stall_delta = 0;
    double fault_rate_pps = 0.0;

    if (!has_prev_snap_) {
        has_prev_snap_ = true;
        prev_snap_ = snap;
        prev_mext_ = mext_snap;
        prev_reclaimed_pages_ = 0;
    } else {
        if (snap.timestamp_ns > prev_snap_.timestamp_ns) {
            dt_sec = std::clamp(
                static_cast<double>(snap.timestamp_ns - prev_snap_.timestamp_ns) * 1e-9,
                5e-4, 5.0);
        }

        /*
         * Disturbance-observer velocity estimation:
         * Subtract pages reclaimed by SMAP (prev_reclaimed_pages_) and kernel steals
         * during [t_{k-1}, t_k] so our own proactive eviction never masks ongoing
         * application memory growth (eliminating bang-bang hunting).
         */
        uint64_t d_kswapd = (snap.pgsteal_kswapd >= prev_snap_.pgsteal_kswapd)
                                ? (snap.pgsteal_kswapd - prev_snap_.pgsteal_kswapd)
                                : 0ULL;
        uint64_t d_direct = (snap.pgsteal_direct >= prev_snap_.pgsteal_direct)
                                ? (snap.pgsteal_direct - prev_snap_.pgsteal_direct)
                                : 0ULL;
        double reclaimed_since_prev =
            static_cast<double>(prev_reclaimed_pages_ + d_kswapd + d_direct);
        double d_free = static_cast<double>(snap.free_pages) -
                        static_cast<double>(prev_snap_.free_pages);
        double uncomp_dfree = d_free - reclaimed_since_prev;
        double a_net_pps = std::max(0.0, -uncomp_dfree / dt_sec);

        double prev_vel = alloc_vel_pps_;
        double alpha = (a_net_pps > prev_vel) ? 0.65 : 0.22;
        alloc_vel_pps_ = (1.0 - alpha) * prev_vel + alpha * a_net_pps;
        if (alloc_vel_pps_ < 1.0) {
            alloc_vel_pps_ = 0.0;
        }

        double accel_inst = (alloc_vel_pps_ - prev_vel) / dt_sec;
        alloc_accel_pps2_ = 0.60 * alloc_accel_pps2_ + 0.40 * accel_inst;
        if (std::fabs(alloc_accel_pps2_) < 1.0) {
            alloc_accel_pps2_ = 0.0;
        }

        if (snap.allocstall_total >= prev_snap_.allocstall_total) {
            stall_delta = snap.allocstall_total - prev_snap_.allocstall_total;
        }
        if (mext_snap.faults_processed >= prev_mext_.faults_processed) {
            fault_rate_pps = static_cast<double>(
                                 mext_snap.faults_processed - prev_mext_.faults_processed) /
                             dt_sec;
        }

        prev_snap_ = snap;
        prev_mext_ = mext_snap;
        prev_reclaimed_pages_ = 0;
    }

    /* Dynamic lookahead watermark: W_dyn(t) = W_base + alpha*A(t)*tau + 0.5*beta*max(0,dA/dt)*tau^2 */
    double vel_term = std::max(0.0, cfg_.velocity_gain) * alloc_vel_pps_ * tau;
    double acc_term = 0.5 * std::max(0.0, cfg_.accel_gain) *
                      std::max(0.0, alloc_accel_pps2_) * tau * tau;
    uint64_t w_dyn = std::clamp<uint64_t>(
        w_base + static_cast<uint64_t>(std::ceil(vel_term + acc_term)), w_base, w_max);

    /* Time-To-Exhaustion before hitting Linux Direct Reclaim cliff (WMARK_MIN) */
    double margin_above_min = (snap.free_pages > w_min)
                                  ? static_cast<double>(snap.free_pages - w_min)
                                  : 0.0;
    double tte_ms = (alloc_vel_pps_ > 16.0)
                        ? ((margin_above_min / alloc_vel_pps_) * 1000.0)
                        : 99999.0;

    /* 4-regime urgency state machine & cold-age quantile stepping */
    ProactiveReclaimUrgency urgency = RECLAIM_CALM;
    uint32_t cold_win = cfg_.calm_cold_window;

    if (snap.free_pages <= w_low || tte_ms <= 40.0 || stall_delta > 0) {
        urgency = RECLAIM_EMERGENCY;
        cold_win = cfg_.min_cold_window;
    } else if (tte_ms <= 160.0 || kernel_pressure || urgent_pages > 0 ||
               (snap.free_pages < w_dyn && alloc_vel_pps_ > (static_cast<double>(w_base) / tau))) {
        urgency = RECLAIM_BURST;
        cold_win = std::max<uint32_t>(cfg_.min_cold_window, 2U);
    } else if (snap.free_pages < w_dyn) {
        urgency = RECLAIM_PROACTIVE;
        cold_win = std::clamp<uint32_t>(5U, cfg_.min_cold_window,
                                        std::max<uint32_t>(cfg_.min_cold_window, cfg_.calm_cold_window));
    } else {
        urgency = RECLAIM_CALM;
        cold_win = cfg_.calm_cold_window;
    }

    /* Closed-loop thrash brake using PSI full + MEXT uncompensated fault rate */
    double brake_scale = 1.0;
    double uncomp_fault_pps = fault_rate_pps * std::clamp(1.0 - mext_snap.hit_rate, 0.0, 1.0);
    double psi_limit = std::max(1.0, cfg_.psi_full_brake_limit);

    if (snap.psi_full_avg10 > psi_limit ||
        (uncomp_fault_pps > 4096.0 && snap.free_pages > w_low)) {
        double psi_scale = (snap.psi_full_avg10 > psi_limit)
                               ? (psi_limit / snap.psi_full_avg10)
                               : 1.0;
        double refault_scale = (uncomp_fault_pps > 4096.0)
                                   ? (4096.0 / uncomp_fault_pps)
                                   : 1.0;
        double min_floor = (snap.free_pages <= w_low) ? 0.60 : 0.25;
        brake_scale = std::clamp(std::min(psi_scale, refault_scale), min_floor, 1.0);
        if (brake_scale < 0.95) {
            ++stats_.thrash_brake_events;
        }
        if (brake_scale < 0.85 && snap.free_pages > w_low) {
            cold_win = std::max<uint32_t>(cold_win, 3U);
        }
    }

    uint64_t target_pages = 0;
    if (urgency != RECLAIM_CALM) {
        uint64_t deficit = (w_dyn > snap.free_pages) ? (w_dyn - snap.free_pages) : 0ULL;
        double step_horizon = std::max(dt_sec, poll_dt_cfg);
        double ff_pages = alloc_vel_pps_ * step_horizon;
        double rec_pages = 0.0;

        if (urgency == RECLAIM_EMERGENCY) {
            rec_pages = static_cast<double>(deficit + (w_low / 4ULL));
        } else if (urgency == RECLAIM_BURST) {
            rec_pages = static_cast<double>(deficit) * std::min(1.0, (2.0 * step_horizon) / tau);
        } else {
            rec_pages = static_cast<double>(deficit) * std::min(1.0, step_horizon / tau);
        }

        double raw_target = ff_pages + rec_pages + static_cast<double>(urgent_pages);
        uint64_t scaled = static_cast<uint64_t>(std::ceil(raw_target * brake_scale));
        uint64_t cap = (cfg_.max_pages_per_tick > 0) ? cfg_.max_pages_per_tick : 8192ULL;
        if (urgency == RECLAIM_EMERGENCY) {
            cap = std::max<uint64_t>(cap, cap * 2ULL);
        }
        target_pages = std::min<uint64_t>(cap, scaled);
    }

    uint8_t clamped_cw = ClampColdWindow(cold_win);
    dec.urgency = urgency;
    dec.effective_cold_window = clamped_cw;
    dec.target_reclaim_pages = target_pages;
    dec.dynamic_wmark_pages = w_dyn;
    dec.alloc_velocity_pps = alloc_vel_pps_;
    dec.alloc_accel_pps2 = alloc_accel_pps2_;
    dec.estimated_tte_ms = tte_ms;
    dec.thrash_brake_scale = brake_scale;

    /* Update telemetry snapshot */
    const double pages_to_mb = static_cast<double>(1ULL << page_shift_) / (1024.0 * 1024.0);
    ++stats_.ticks_total;
    if (urgency == RECLAIM_BURST || urgency == RECLAIM_EMERGENCY) {
        ++stats_.burst_activations;
    }
    stats_.kernel_allocstalls_observed += stall_delta;
    stats_.current_urgency = static_cast<uint32_t>(urgency);
    stats_.effective_cold_window = clamped_cw;
    stats_.current_free_pages = snap.free_pages;
    stats_.dynamic_wmark_pages = w_dyn;
    stats_.static_wmark_low_pages = w_low;
    stats_.alloc_velocity_mbps = alloc_vel_pps_ * pages_to_mb;
    stats_.alloc_accel_mbps2 = alloc_accel_pps2_ * pages_to_mb;
    stats_.estimated_tte_ms = tte_ms;
    stats_.thrash_brake_scale = brake_scale;

    /*
     * Abundant-RAM fast bypass: when host free RAM exceeds BOTH 10% of total RAM
     * and 4x W_dyn (e.g. 258 GB free on a 768 GB server), skip ColdTracker bitmap
     * rebuilds and ProcessManager lock sweeps completely; enable cold aging well
     * ahead of pressure once free RAM drops into the top 90% utilization band.
     */
    uint64_t cold_track_gate = std::max<uint64_t>(w_dyn * 4ULL, snap.total_pages / 10ULL);
    bool track_cold = (urgency != RECLAIM_CALM) || (snap.free_pages <= cold_track_gate);

    recommended_cold_window_.store(clamped_cw, std::memory_order_release);
    current_urgency_.store(static_cast<uint32_t>(urgency), std::memory_order_release);
    should_track_cold_.store(track_cold, std::memory_order_release);

    return dec;
}

ReclaimDecision ProactiveReclaimController::StepOnce()
{
    TelemetryProvider telemetry_fn;
    MextFeedbackProvider feedback_fn;
    EvictExecutor evict_fn;
    mext::MextEngine *eng = nullptr;
    uint32_t page_shift = 12;

    {
        std::lock_guard<std::mutex> lk(mu_);
        telemetry_fn = telemetry_fn_;
        feedback_fn = mext_feedback_fn_;
        evict_fn = evict_fn_;
        eng = bound_engine_ ? bound_engine_ : &mext::GlobalMextEngine();
        page_shift = page_shift_;
    }

    HostMemSnapshot snap{};
    bool ok = telemetry_fn ? telemetry_fn(&snap) : ReadLinuxHostSnapshot(page_shift, &snap);
    if (!ok) {
        return ReclaimDecision{};
    }

    MextFeedbackSnapshot mext_snap{};
    if (feedback_fn) {
        mext_snap = feedback_fn();
    } else if (eng) {
        MextStats ms = eng->GetStatsSnapshot();
        mext_snap.faults_processed = ms.faults_processed;
        mext_snap.evicts_tracked = ms.evicts_tracked;
        mext_snap.hit_rate = ms.hit_rate;
    }

    ReclaimDecision dec{};
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!cfg_.enabled) {
            return dec;
        }
        uint64_t urgent_pages = urgent_pages_pending_;
        bool kp = kernel_pressure_pending_;
        if (kp && hint_zone_free_pages_ > 0 && hint_zone_free_pages_ < snap.free_pages) {
            snap.free_pages = hint_zone_free_pages_;
        }
        if (kp && hint_zone_wmark_low_ > snap.wmark_low_pages) {
            snap.wmark_low_pages = hint_zone_wmark_low_;
        }
        urgent_pages_pending_ = 0;
        kernel_pressure_pending_ = false;
        wakeup_pending_ = false;

        dec = EvaluateLocked(snap, mext_snap, urgent_pages, kp);
    }

    if (dec.target_reclaim_pages > 0 && evict_fn) {
        uint64_t reclaimed = evict_fn(dec.target_reclaim_pages, dec.effective_cold_window);
        dec.actual_reclaimed_pages = reclaimed;

        std::lock_guard<std::mutex> lk(mu_);
        prev_reclaimed_pages_ += reclaimed;
        stats_.pages_reclaimed_proactive += reclaimed;
    }

    return dec;
}

void ProactiveReclaimController::WorkerLoop()
{
    while (running_.load(std::memory_order_acquire)) {
        (void)StepOnce();

        std::unique_lock<std::mutex> lk(mu_);
        if (!running_.load(std::memory_order_acquire)) {
            break;
        }
        if (wakeup_pending_) {
            continue;
        }
        uint32_t wait_ms = (cfg_.poll_interval_ms > 0) ? cfg_.poll_interval_ms : 15U;
        if (stats_.current_urgency == static_cast<uint32_t>(RECLAIM_BURST) ||
            stats_.current_urgency == static_cast<uint32_t>(RECLAIM_EMERGENCY)) {
            wait_ms = std::max<uint32_t>(2U, wait_ms / 3U);
        } else if (!should_track_cold_.load(std::memory_order_relaxed) &&
                   alloc_vel_pps_ < 64.0) {
            /* Relax /proc seq_file polling when >10% host RAM is idle */
            wait_ms = std::max<uint32_t>(wait_ms, 100U);
        }
        cv_.wait_for(lk, std::chrono::milliseconds(wait_ms), [this]() {
            return !running_.load(std::memory_order_acquire) || wakeup_pending_;
        });
    }
}

ProactiveReclaimStats ProactiveReclaimController::GetStatsSnapshot() const
{
    std::lock_guard<std::mutex> lk(mu_);
    return stats_;
}

uint8_t ProactiveReclaimController::RecommendedColdWindow() const
{
    return recommended_cold_window_.load(std::memory_order_acquire);
}

ProactiveReclaimUrgency ProactiveReclaimController::CurrentUrgency() const
{
    return static_cast<ProactiveReclaimUrgency>(
        current_urgency_.load(std::memory_order_acquire));
}

bool ProactiveReclaimController::ShouldTrackCold() const
{
    return should_track_cold_.load(std::memory_order_acquire);
}

ProactiveReclaimController &GlobalProactiveReclaim()
{
    static ProactiveReclaimController instance;
    return instance;
}

} // namespace reclaim
} // namespace smap

extern "C" {

void ProactiveReclaimDefaultConfig(ProactiveReclaimConfig *cfg)
{
    if (!cfg) {
        return;
    }
    std::memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = true;
    cfg->is_zram_backend = false;
    cfg->poll_interval_ms = 15U;
    cfg->lookahead_horizon_ms = 80U;
    cfg->base_headroom_ratio = 1.15;
    cfg->max_headroom_ratio = 0.20;
    cfg->velocity_gain = 1.35;
    cfg->accel_gain = 0.50;
    cfg->psi_full_brake_limit = 15.0;
    cfg->min_cold_window = 1U;
    cfg->calm_cold_window = 8U;
    cfg->max_pages_per_tick = 8192ULL;
}

void ProactiveReclaimApplyZramPreset(ProactiveReclaimConfig *cfg)
{
    if (!cfg) {
        return;
    }
    /*
     * zram (in-DRAM compressed swap) drains orders of magnitude faster than NVMe
     * (~3-8 us/page CPU compression without block queue latency), so a tighter
     * 35 ms lookahead horizon and 10 ms sample tick eliminate Direct Reclaim
     * while holding an even smaller steady-state free buffer.
     */
    ProactiveReclaimDefaultConfig(cfg);
    cfg->is_zram_backend = true;
    cfg->poll_interval_ms = 10U;
    cfg->lookahead_horizon_ms = 35U;
    cfg->base_headroom_ratio = 1.10;
    cfg->max_headroom_ratio = 0.15;
    cfg->velocity_gain = 1.25;
    cfg->accel_gain = 0.40;
    cfg->psi_full_brake_limit = 18.0;
    cfg->max_pages_per_tick = 12288ULL;
}

int ProactiveReclaimStart(const ProactiveReclaimConfig *cfg, ProactiveEvictFn evict_fn)
{
    ProactiveReclaimConfig local_cfg{};
    if (cfg) {
        local_cfg = *cfg;
    } else {
        ProactiveReclaimDefaultConfig(&local_cfg);
    }

    smap::reclaim::ProactiveReclaimController::EvictExecutor exec_cb;
    if (evict_fn) {
        exec_cb = [evict_fn](uint64_t target_pages, uint8_t cold_win) -> uint64_t {
            return evict_fn(target_pages, cold_win);
        };
    }
    return smap::reclaim::GlobalProactiveReclaim().Start(local_cfg, std::move(exec_cb), true);
}

void ProactiveReclaimStop(void)
{
    smap::reclaim::GlobalProactiveReclaim().Stop();
}

bool ProactiveReclaimIsRunning(void)
{
    return smap::reclaim::GlobalProactiveReclaim().IsRunning();
}

int ProactiveReclaimUpdateConfig(const ProactiveReclaimConfig *cfg)
{
    if (!cfg) {
        return -1;
    }
    return smap::reclaim::GlobalProactiveReclaim().UpdateConfig(*cfg);
}

void ProactiveReclaimTriggerUrgent(uint64_t size_bytes)
{
    smap::reclaim::GlobalProactiveReclaim().TriggerUrgentBytes(size_bytes);
}

uint8_t ProactiveReclaimRecommendedColdWindow(void)
{
    return smap::reclaim::GlobalProactiveReclaim().RecommendedColdWindow();
}

uint32_t ProactiveReclaimCurrentUrgency(void)
{
    return static_cast<uint32_t>(smap::reclaim::GlobalProactiveReclaim().CurrentUrgency());
}

bool ProactiveReclaimShouldTrackCold(void)
{
    return smap::reclaim::GlobalProactiveReclaim().ShouldTrackCold();
}

int ProactiveReclaimQueryStats(ProactiveReclaimStats *stats_out)
{
    if (!stats_out) {
        return -1;
    }
    *stats_out = smap::reclaim::GlobalProactiveReclaim().GetStatsSnapshot();
    return 0;
}

} /* extern "C" */
