/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Dynamic Proactive Reclaim Controller for SMAP-MEXT.
 *              Replaces static Linux watermarks with a compensation-aware
 *              allocation velocity / acceleration / Time-To-Exhaustion (TTE)
 *              controller coupled to MEXT's fault ring and refault telemetry.
 */
#ifndef SMAP_USER_RECLAIM_PROACTIVE_RECLAIM_H
#define SMAP_USER_RECLAIM_PROACTIVE_RECLAIM_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

#include "../manage/swap_types.h"
#include "../predictor/mext_engine.h"

namespace smap {
namespace reclaim {

struct HostMemSnapshot {
    uint64_t timestamp_ns = 0;
    uint64_t total_pages = 0;
    uint64_t free_pages = 0;       /* MemAvailable or tightest zone NR_FREE_PAGES */
    uint64_t wmark_min_pages = 0;  /* Direct Reclaim slowpath threshold */
    uint64_t wmark_low_pages = 0;  /* kswapd wakeup threshold */
    uint64_t wmark_high_pages = 0; /* kswapd sleep threshold */
    uint64_t pgalloc_total = 0;    /* cumulative page allocations */
    uint64_t pgsteal_kswapd = 0;   /* cumulative background kswapd steals */
    uint64_t pgsteal_direct = 0;   /* cumulative synchronous direct-reclaim steals */
    uint64_t allocstall_total = 0; /* cumulative __alloc_pages_slowpath entries */
    double psi_some_avg10 = 0.0;
    double psi_full_avg10 = 0.0;
};

struct MextFeedbackSnapshot {
    uint64_t faults_processed = 0;
    uint64_t evicts_tracked = 0;
    double hit_rate = 0.0;
};

struct ReclaimDecision {
    ProactiveReclaimUrgency urgency = RECLAIM_CALM;
    uint8_t effective_cold_window = 8;
    uint64_t target_reclaim_pages = 0;
    uint64_t actual_reclaimed_pages = 0;
    uint64_t dynamic_wmark_pages = 0;
    double alloc_velocity_pps = 0.0;
    double alloc_accel_pps2 = 0.0;
    double estimated_tte_ms = 99999.0;
    double thrash_brake_scale = 1.0;
};

class ProactiveReclaimController {
public:
    using TelemetryProvider = std::function<bool(HostMemSnapshot *out)>;
    using MextFeedbackProvider = std::function<MextFeedbackSnapshot()>;
    using EvictExecutor = std::function<uint64_t(uint64_t target_pages, uint8_t cold_window)>;

    ProactiveReclaimController();
    ~ProactiveReclaimController();

    int Start(const ProactiveReclaimConfig &cfg, EvictExecutor evict_fn, bool spawn_worker = true);
    void Stop();
    bool IsRunning() const { return running_.load(std::memory_order_acquire); }

    int UpdateConfig(const ProactiveReclaimConfig &cfg);

    /* Custom providers for discrete-event benchmarks & unit tests */
    void SetTelemetryProvider(TelemetryProvider provider);
    void SetMextFeedbackProvider(MextFeedbackProvider provider);
    void SetEvictExecutor(EvictExecutor evict_fn);
    void BindMextEngine(mext::MextEngine *engine);

    /* Instant out-of-cycle wakeups from kernel wakeup_kswapd kprobe or urgent SDK call */
    void NotifyKernelPressure(uint32_t nid, uint64_t free_pages, uint64_t wmark_low_pages,
                              uint16_t order, uint64_t timestamp_ns);
    void TriggerUrgentBytes(uint64_t urgent_bytes);

    /* Execute a single control tick synchronously (used by worker thread & tests) */
    ReclaimDecision StepOnce();

    ProactiveReclaimStats GetStatsSnapshot() const;
    uint8_t RecommendedColdWindow() const;
    ProactiveReclaimUrgency CurrentUrgency() const;

    /* Native Linux /proc telemetry reader (/proc/meminfo, zoneinfo, vmstat, pressure) */
    static bool ReadLinuxHostSnapshot(uint32_t page_shift, HostMemSnapshot *out);
    static bool ReadLinuxHostSnapshotFromRoot(const char *proc_root,
                                              uint32_t page_shift,
                                              HostMemSnapshot *out);

private:
    void WorkerLoop();
    ReclaimDecision EvaluateLocked(const HostMemSnapshot &snap,
                                   const MextFeedbackSnapshot &mext_snap,
                                   uint64_t urgent_pages,
                                   bool kernel_pressure);

    mutable std::mutex mu_;
    std::condition_variable cv_;
    ProactiveReclaimConfig cfg_{};
    uint32_t page_shift_ = 12;

    TelemetryProvider telemetry_fn_;
    MextFeedbackProvider mext_feedback_fn_;
    EvictExecutor evict_fn_;
    mext::MextEngine *bound_engine_ = nullptr;

    std::atomic<bool> running_{ false };
    std::thread worker_;

    /* Pending asynchronous wakeup state */
    bool wakeup_pending_ = false;
    bool kernel_pressure_pending_ = false;
    uint64_t urgent_pages_pending_ = 0;
    uint64_t hint_zone_free_pages_ = 0;
    uint64_t hint_zone_wmark_low_ = 0;

    /* Estimator state */
    bool has_prev_snap_ = false;
    HostMemSnapshot prev_snap_{};
    MextFeedbackSnapshot prev_mext_{};
    uint64_t prev_reclaimed_pages_ = 0;
    double alloc_vel_pps_ = 0.0;
    double alloc_accel_pps2_ = 0.0;

    /* Live telemetry counters */
    ProactiveReclaimStats stats_{};
    std::atomic<uint8_t> recommended_cold_window_{ 8 };
    std::atomic<uint32_t> current_urgency_{ static_cast<uint32_t>(RECLAIM_CALM) };
};

ProactiveReclaimController &GlobalProactiveReclaim();

} // namespace reclaim
} // namespace smap

#endif /* SMAP_USER_RECLAIM_PROACTIVE_RECLAIM_H */
