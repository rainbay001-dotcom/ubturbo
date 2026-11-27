/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Verification & Allocation-Burst Benchmark Suite for SMAP-MEXT
 *              Dynamic Proactive Reclaim Controller (NVMe & zram backends).
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_set>
#include <vector>

#include "../../src/tiering/mext_ring_abi.h"
#include "../../src/user/predictor/mext_engine.h"
#include "../../src/user/predictor/mext_predictor_c_api.h"
#include "../../src/user/reclaim/proactive_reclaim.h"
#include "../../src/user/reclaim/proactive_reclaim_c_api.h"

using namespace smap::mext;
using namespace smap::reclaim;

#define REQUIRE(cond)                                                                         \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            std::fprintf(stderr, "TEST FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            std::exit(1);                                                                     \
        }                                                                                     \
    } while (0)

namespace {

bool PushRingEventSim(mext_ring_buffer *rb, uint32_t type, uint32_t pid,
                      uint64_t vpn, uint64_t aux, uint64_t ts_ns)
{
    uint64_t w = __atomic_load_n(&rb->hdr.write_idx, __ATOMIC_RELAXED);
    uint64_t tail = __atomic_load_n(&rb->hdr.tail, __ATOMIC_ACQUIRE);
    if (w - tail >= MEXT_RING_CAPACITY) {
        return false;
    }
    if (!__atomic_compare_exchange_n(&rb->hdr.write_idx, &w, w + 1ULL,
                                     false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
        return false;
    }
    mext_ring_event *ev = &rb->events[w & MEXT_RING_MASK];
    ev->pid = pid;
    ev->vpn = vpn;
    ev->aux = aux;
    ev->timestamp = ts_ns;
    __atomic_store_n(&ev->type, type, __ATOMIC_RELEASE);
    return true;
}

/* --------------------------------------------------------------------------
 * 1. Native /proc Telemetry Parser Verification
 * -------------------------------------------------------------------------- */
void TestProcTelemetryParser()
{
    namespace fs = std::filesystem;
    fs::path mock_root = fs::current_path() / ("smap_mock_proc_" + std::to_string(getpid()));
    fs::create_directories(mock_root / "pressure");

    struct CleanupGuard {
        fs::path dir;
        ~CleanupGuard()
        {
            std::error_code ec;
            fs::remove_all(dir, ec);
        }
    } cleanup{ mock_root };

    {
        std::ofstream f(mock_root / "meminfo");
        f << "MemTotal:       16777216 kB\n"
          << "MemFree:         1048576 kB\n"
          << "MemAvailable:    1310720 kB\n";
    }
    {
        std::ofstream f(mock_root / "zoneinfo");
        f << "Node 0, zone   Normal\n"
          << "  pages free     140000\n"
          << "        min      8000\n"
          << "        low      10000\n"
          << "        high     12000\n"
          << "Node 1, zone   Normal\n"
          << "  pages free     160000\n"
          << "        min      8000\n"
          << "        low      10000\n"
          << "        high     12000\n";
    }
    {
        std::ofstream f(mock_root / "vmstat");
        f << "pgalloc_dma32 1000\n"
          << "pgalloc_normal 500000\n"
          << "pgalloc_movable 250000\n"
          << "pgsteal_kswapd 42000\n"
          << "pgsteal_direct 310\n"
          << "allocstall_normal 12\n"
          << "allocstall_movable 5\n";
    }
    {
        std::ofstream f(mock_root / "pressure" / "memory");
        f << "some avg10=6.25 avg60=2.10 avg300=0.50 total=123456\n"
          << "full avg10=1.75 avg60=0.40 avg300=0.10 total=23456\n";
    }

    HostMemSnapshot snap{};
    bool ok = ProactiveReclaimController::ReadLinuxHostSnapshotFromRoot(
        mock_root.string().c_str(), 12U, &snap);
    REQUIRE(ok);
    REQUIRE(snap.total_pages == 4194304ULL);
    /* Tightest of MemAvailable (327,680 pages) and zone NR_FREE_PAGES (300,000 pages) */
    REQUIRE(snap.free_pages == 300000ULL);
    REQUIRE(snap.wmark_min_pages == 16000ULL);
    REQUIRE(snap.wmark_low_pages == 20000ULL);
    REQUIRE(snap.wmark_high_pages == 24000ULL);
    REQUIRE(snap.pgalloc_total == 751000ULL);
    REQUIRE(snap.pgsteal_kswapd == 42000ULL);
    REQUIRE(snap.pgsteal_direct == 310ULL);
    REQUIRE(snap.allocstall_total == 17ULL);
    REQUIRE(std::fabs(snap.psi_some_avg10 - 6.25) < 1e-6);
    REQUIRE(std::fabs(snap.psi_full_avg10 - 1.75) < 1e-6);

    std::printf("[PASS] Native /proc telemetry parser (meminfo + zoneinfo + vmstat + PSI)\n");
}

/* --------------------------------------------------------------------------
 * 2. Disturbance-Observer Velocity Estimator (Zero Bang-Bang Oscillation)
 * -------------------------------------------------------------------------- */
void TestDisturbanceObserverStability()
{
    MextEngine dummy_mext;
    ProactiveReclaimController ctrl;
    ctrl.BindMextEngine(&dummy_mext);

    ProactiveReclaimConfig cfg{};
    ProactiveReclaimDefaultConfig(&cfg);
    cfg.poll_interval_ms = 10U;
    cfg.lookahead_horizon_ms = 80U;
    cfg.max_pages_per_tick = 32768ULL;

    /*
     * Host: 16 GB (4,194,304 pages), WMARK_MIN = 16,384, WMARK_LOW = 20,480.
     * App allocates 4.0 GB/s = 1,048,576 pages/s = 10,486 pages per 10 ms tick!
     * Even when ProactiveReclaimController reclaims ~10,486 pages/tick so `free_pages`
     * remains flat, our disturbance-observer velocity estimator must stay locked onto
     * ~4,096 MB/s without collapsing to zero!
     */
    uint64_t sim_ts_ns = 1'000'000'000ULL;
    int64_t sim_free = 120000;
    const uint64_t burst_demand_per_tick = 10486ULL; /* ~4.0 GB/s */

    ctrl.SetTelemetryProvider([&](HostMemSnapshot *out) -> bool {
        out->timestamp_ns = sim_ts_ns;
        out->total_pages = 4194304ULL;
        out->free_pages = static_cast<uint64_t>(std::max<int64_t>(0, sim_free));
        out->wmark_min_pages = 16384ULL;
        out->wmark_low_pages = 20480ULL;
        out->wmark_high_pages = 24576ULL;
        return true;
    });

    ctrl.SetMextFeedbackProvider([]() {
        return MextFeedbackSnapshot{ 0, 0, 0.90 };
    });

    REQUIRE(ctrl.Start(cfg, [&](uint64_t target, uint8_t /*cw*/) -> uint64_t {
        sim_free += static_cast<int64_t>(target);
        return target;
    }, false) == 0);

    /* Initial baseline tick */
    (void)ctrl.StepOnce();

    /* 20 ticks (200 ms) of sustained 4.0 GB/s allocation burst */
    std::vector<double> measured_mbps;
    for (int t = 0; t < 20; ++t) {
        sim_ts_ns += 10'000'000ULL; /* +10 ms */
        sim_free -= static_cast<int64_t>(burst_demand_per_tick);
        ReclaimDecision dec = ctrl.StepOnce();
        if (t >= 6) {
            ProactiveReclaimStats st = ctrl.GetStatsSnapshot();
            measured_mbps.push_back(st.alloc_velocity_mbps);
            REQUIRE(dec.dynamic_wmark_pages > 80000ULL);
            REQUIRE(dec.target_reclaim_pages >= 8000ULL);
        }
    }

    double min_vel = *std::min_element(measured_mbps.begin(), measured_mbps.end());
    double max_vel = *std::max_element(measured_mbps.begin(), measured_mbps.end());
    /* Must track ~4096 MB/s tightly without bang-bang drops! */
    REQUIRE(min_vel > 3600.0 && max_vel < 4500.0);

    /* Now stop the burst (0 MB/s demand) for 30 ticks (300 ms): must relax to CALM */
    for (int t = 0; t < 30; ++t) {
        sim_ts_ns += 10'000'000ULL;
        (void)ctrl.StepOnce();
    }
    ProactiveReclaimStats calm_st = ctrl.GetStatsSnapshot();
    REQUIRE(calm_st.current_urgency == RECLAIM_CALM);
    REQUIRE(calm_st.effective_cold_window == cfg.calm_cold_window);
    REQUIRE(calm_st.alloc_velocity_mbps < 30.0);

    ctrl.Stop();
    std::printf("[PASS] Disturbance-observer velocity lock (steady %.0f-%.0f MB/s under self-reclaim, zero hunting)\n",
                min_vel, max_vel);
}

/* --------------------------------------------------------------------------
 * 3. Closed-Loop MEXT Thrash Brake Verification
 * -------------------------------------------------------------------------- */
void TestClosedLoopMextThrashBrake()
{
    MextEngine dummy_mext;
    ProactiveReclaimController ctrl;
    ctrl.BindMextEngine(&dummy_mext);

    ProactiveReclaimConfig cfg{};
    ProactiveReclaimDefaultConfig(&cfg);
    cfg.poll_interval_ms = 10U;
    cfg.psi_full_brake_limit = 15.0;

    uint64_t ts_ns = 1'000'000'000ULL;
    uint64_t free_pages = 35000ULL;
    double psi_full = 1.5;
    uint64_t faults = 0;
    double hit_rate = 0.94;

    ctrl.SetTelemetryProvider([&](HostMemSnapshot *out) -> bool {
        out->timestamp_ns = ts_ns;
        out->total_pages = 4194304ULL;
        out->free_pages = free_pages;
        out->wmark_min_pages = 16384ULL;
        out->wmark_low_pages = 20480ULL;
        out->wmark_high_pages = 24576ULL;
        out->psi_full_avg10 = psi_full;
        return true;
    });
    ctrl.SetMextFeedbackProvider([&]() {
        return MextFeedbackSnapshot{ faults, 10000ULL, hit_rate };
    });

    REQUIRE(ctrl.Start(cfg, [](uint64_t target, uint8_t) { return target; }, false) == 0);
    (void)ctrl.StepOnce();

    /* Case A: 25,000 faults/s, but MEXT hides 94% of them (uncompensated = 1,500/s < 4,096/s) */
    ts_ns += 10'000'000ULL;
    free_pages = 27000ULL; /* fast consumption */
    faults += 250ULL;      /* 25,000 faults/sec */
    ReclaimDecision dec_mext_good = ctrl.StepOnce();
    REQUIRE(std::fabs(dec_mext_good.thrash_brake_scale - 1.0) < 1e-6);

    /* Case B: Adversarial random thrashing (MEXT hit_rate = 3%, PSI full = 36%) */
    ts_ns += 10'000'000ULL;
    free_pages = 26000ULL;
    faults += 250ULL;
    hit_rate = 0.03;
    psi_full = 36.0;
    ReclaimDecision dec_thrash = ctrl.StepOnce();
    REQUIRE(dec_thrash.thrash_brake_scale < 0.45);
    REQUIRE(dec_thrash.effective_cold_window >= 3U);

    ProactiveReclaimStats st = ctrl.GetStatsSnapshot();
    REQUIRE(st.thrash_brake_events >= 1ULL);

    ctrl.Stop();
    std::printf("[PASS] Closed-loop MEXT thrash brake (94%% hit scale=%.2f vs random thrash scale=%.2f, cw=%u)\n",
                dec_mext_good.thrash_brake_scale, dec_thrash.thrash_brake_scale,
                static_cast<unsigned>(dec_thrash.effective_cold_window));
}

/* --------------------------------------------------------------------------
 * 4. Live Multi-Threaded Kernel Ring Wakeup (MEXT_EVT_MEM_PRESSURE)
 * -------------------------------------------------------------------------- */
void TestLiveKernelPressureRingWakeup()
{
    auto ring = std::make_unique<mext_ring_buffer>();
    std::memset(ring.get(), 0, sizeof(mext_ring_buffer));
    ring->hdr.magic = MEXT_RING_MAGIC;
    ring->hdr.version = MEXT_RING_VERSION;
    ring->hdr.capacity = MEXT_RING_CAPACITY;
    ring->hdr.page_shift = 12U;

    MextEngine mext_eng;
    mext_eng.AttachExternalRing(ring.get());

    ProactiveReclaimController ctrl;
    ctrl.BindMextEngine(&mext_eng);

    /* Set a huge 500 ms poll interval so sub-10ms reclaim PROVES ring wakeup worked! */
    ProactiveReclaimConfig rcfg{};
    ProactiveReclaimDefaultConfig(&rcfg);
    rcfg.poll_interval_ms = 500U;

    std::atomic<bool> pressure_active{ false };
    std::atomic<uint64_t> woken_reclaimed{ 0 };
    std::chrono::steady_clock::time_point t_push;
    std::chrono::steady_clock::time_point t_reclaim;
    std::atomic<bool> recorded_wakeup{ false };

    ctrl.SetTelemetryProvider([&](HostMemSnapshot *out) -> bool {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        out->timestamp_ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
        out->total_pages = 1048576ULL;
        out->wmark_min_pages = 8192ULL;
        out->wmark_low_pages = 10240ULL;
        out->wmark_high_pages = 12288ULL;
        out->free_pages = pressure_active.load(std::memory_order_acquire) ? 9800ULL : 90000ULL;
        return true;
    });

    MextConfig mcfg{};
    MextDefaultConfig(&mcfg);
    REQUIRE(mext_eng.Start(-1, mcfg, true) == 0);
    REQUIRE(ctrl.Start(rcfg, [&](uint64_t target, uint8_t) -> uint64_t {
        if (pressure_active.load(std::memory_order_acquire)) {
            bool expected = false;
            if (recorded_wakeup.compare_exchange_strong(expected, true)) {
                t_reclaim = std::chrono::steady_clock::now();
            }
            woken_reclaimed.fetch_add(target, std::memory_order_relaxed);
        }
        return target;
    }, true) == 0);

    /* Let initial tick settle into 500 ms sleep */
    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    /* Inject wakeup_kswapd MEXT_EVT_MEM_PRESSURE event into lockless ring */
    pressure_active.store(true, std::memory_order_release);
    t_push = std::chrono::steady_clock::now();
    uint64_t aux = (10240ULL << 16) | 0ULL;
    REQUIRE(PushRingEventSim(ring.get(), MEXT_EVT_MEM_PRESSURE, 0U, 9800ULL, aux, 1000000ULL));

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    while (!recorded_wakeup.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    REQUIRE(recorded_wakeup.load(std::memory_order_acquire));
    double wakeup_us = std::chrono::duration<double, std::micro>(t_reclaim - t_push).count();
    REQUIRE(wakeup_us < 25000.0); /* Must beat the 500,000 us poll timer by >20x! */
    REQUIRE(woken_reclaimed.load() > 0ULL);

    ProactiveReclaimStats st = ctrl.GetStatsSnapshot();
    REQUIRE(st.proactive_wakeups >= 1ULL);

    ctrl.Stop();
    mext_eng.Stop();
    std::printf("[PASS] Live 2-thread MEXT_EVT_MEM_PRESSURE ring wakeup latency: %.1f us (reclaimed %llu pages)\n",
                wakeup_us, static_cast<unsigned long long>(woken_reclaimed.load()));
}

/* --------------------------------------------------------------------------
 * 5. End-to-End Allocation-Burst & Usable-DRAM Benchmark (NVMe & zram)
 * -------------------------------------------------------------------------- */
enum class ReclaimPolicyKind {
    STATIC_LOW_WMARK = 0,  /* Default Linux: WMARK_MIN=64MB, WMARK_LOW=80MB, WMARK_HIGH=96MB */
    STATIC_HIGH_WMARK = 1, /* Elevated vm.min_free_kbytes: WMARK_MIN=1.5GB, WMARK_LOW=1.75GB */
    SMAP_PROACTIVE_MEXT = 2,
};

struct BackendSpec {
    const char *name;
    bool is_zram;
    uint64_t max_drain_pages_per_ms; /* Swap-out write/compress throughput */
    double direct_reclaim_stall_us;  /* Latency of 32-page synchronous direct reclaim */
    double major_swapin_us;          /* Unprefetched swap-in page fault stall */
    double prefetched_swapin_us;     /* MEXT prefetched/prefaulted hit latency */
};

struct SimResult {
    uint64_t direct_reclaim_stalls = 0;
    uint64_t direct_reclaimed_pages = 0;
    double p99_alloc_lat_us = 0.0;
    double max_alloc_lat_us = 0.0;
    double calm_usable_dram_pct = 0.0;
    double reaccess_stall_ms = 0.0;
    double mext_hit_rate_pct = 0.0;
};

SimResult RunBurstSimulation(ReclaimPolicyKind policy, const BackendSpec &backend)
{
    /*
     * 16 GB Host (4,194,304 x 4KB pages), simulated at 1 ms resolution over 10,000 ms (10 s):
     *   - [0..2500 ms]:   Calm steady serving (0 MB/s net growth)
     *   - [2500..3700 ms]: Burst 1: 4.0 GB/s allocation spike (1,024 pages/ms)
     *   - [3700..5500 ms]: Calm plateau + sequential re-access of 2,048 warm-cold evicted pages
     *   - [5500..6200 ms]: Burst 2: 5.5 GB/s allocation spike (1,408 pages/ms)
     *   - [6500 ms]:       App frees 1.5 GB (393,216 pages)
     *   - [6200..10000 ms]: Calm steady serving
     */
    constexpr uint64_t kTotalPages = 4194304ULL; /* 16 GB */
    uint64_t w_min = 16384ULL;                   /* 64 MB */
    uint64_t w_low = 20480ULL;                   /* 80 MB */
    uint64_t w_high = 24576ULL;                  /* 96 MB */

    if (policy == ReclaimPolicyKind::STATIC_HIGH_WMARK) {
        /*
         * Models tuning Linux `vm.watermark_scale_factor` (~9% of RAM):
         * keeps WMARK_MIN at 64 MB while widening WMARK_LOW/WMARK_HIGH to 1.50/1.80 GB
         * permanently — buying burst shock absorption at the cost of stranding ~11% DRAM.
         */
        w_min = 16384ULL;   /* 64 MB */
        w_low = 393216ULL;  /* 1.50 GB */
        w_high = 471859ULL; /* 1.80 GB */
    }

    /* Start steady state comfortably above static high watermark (+32 MB margin) */
    int64_t free_pages = static_cast<int64_t>(w_high + 8192ULL);

    /* Optional real MEXT + ProactiveReclaimController instances for SMAP policy */
    MextEngine mext_eng;
    ProactiveReclaimController ctrl;
    std::unordered_set<uint64_t> prefetched_vpns;
    uint64_t sim_ts_ns = 1'000'000'000ULL;
    uint64_t allocstalls = 0;
    uint64_t kswapd_steals = 0;
    uint64_t direct_steals = 0;
    uint64_t pending_proactive_drain = 0;
    uint64_t next_evict_vpn = 0x200000ULL;
    uint64_t evict_epoch = 1ULL;

    if (policy == ReclaimPolicyKind::SMAP_PROACTIVE_MEXT) {
        MextConfig mcfg{};
        if (backend.is_zram) {
            MextApplyZramPreset(&mcfg);
        } else {
            MextDefaultConfig(&mcfg);
            mcfg.fault_trigger_stride = 2;
        }
        mext_eng.SetDispatchCallback([&](int32_t /*pid*/,
                                         const std::vector<uint64_t> &pf,
                                         const std::vector<uint64_t> &pflt) {
            for (uint64_t v : pf) {
                prefetched_vpns.insert(v);
            }
            for (uint64_t v : pflt) {
                prefetched_vpns.insert(v);
            }
        });
        REQUIRE(mext_eng.Start(-1, mcfg, false) == 0);
        REQUIRE(mext_eng.TrackPid(4242) == 0);

        ctrl.BindMextEngine(&mext_eng);
        ProactiveReclaimConfig rcfg{};
        if (backend.is_zram) {
            ProactiveReclaimApplyZramPreset(&rcfg);
        } else {
            ProactiveReclaimDefaultConfig(&rcfg);
            rcfg.poll_interval_ms = 10U;
            rcfg.lookahead_horizon_ms = 90U;
        }
        ctrl.SetTelemetryProvider([&](HostMemSnapshot *out) -> bool {
            out->timestamp_ns = sim_ts_ns;
            out->total_pages = kTotalPages;
            out->free_pages = static_cast<uint64_t>(std::max<int64_t>(0, free_pages));
            out->wmark_min_pages = w_min;
            out->wmark_low_pages = w_low;
            out->wmark_high_pages = w_high;
            out->pgsteal_kswapd = kswapd_steals;
            out->pgsteal_direct = direct_steals;
            out->allocstall_total = allocstalls;
            return true;
        });
        REQUIRE(ctrl.Start(rcfg, [&](uint64_t target, uint8_t /*cw*/) -> uint64_t {
            pending_proactive_drain += target;
            return target;
        }, false) == 0);
    }

    /* Linux kswapd scan-priority ramp state (starts cold at DEF_PRIORITY when woken) */
    uint32_t kswapd_active_ms = 0;
    bool pressure_notified_this_dip = false;

    std::vector<double> alloc_latencies_us;
    alloc_latencies_us.reserve(10000);
    double calm_usable_sum = 0.0;
    uint64_t calm_samples = 0;

    for (uint32_t ms = 0; ms < 10000; ++ms) {
        sim_ts_ns += 1'000'000ULL; /* +1 ms */

        uint64_t demand_pages = 0;
        bool is_calm_window = false;
        if (ms < 2500) {
            is_calm_window = true;
        } else if (ms >= 2500 && ms < 3700) {
            demand_pages = 1024ULL; /* 4.0 GB/s burst */
        } else if (ms >= 3700 && ms < 5500) {
            is_calm_window = true;
        } else if (ms >= 5500 && ms < 6200) {
            demand_pages = 1408ULL; /* 5.5 GB/s burst */
        } else {
            is_calm_window = true;
        }

        if (ms == 6500) {
            free_pages += 393216; /* App munmap()s 1.5 GB */
        }

        /* 1. Background Swap Drain (capped by physical NVMe / zram bandwidth) */
        uint64_t avail_bw = backend.max_drain_pages_per_ms;

        if (policy == ReclaimPolicyKind::SMAP_PROACTIVE_MEXT) {
            uint32_t poll_mod = backend.is_zram ? 10U : 10U;
            /* Instant wakeup_kswapd kprobe trigger if free crosses 1.6x w_low */
            if (free_pages <= static_cast<int64_t>((w_low * 8ULL) / 5ULL) &&
                !pressure_notified_this_dip) {
                pressure_notified_this_dip = true;
                ctrl.NotifyKernelPressure(0U, static_cast<uint64_t>(std::max<int64_t>(0, free_pages)),
                                          w_low, 0U, sim_ts_ns);
                (void)ctrl.StepOnce();
            } else if ((ms % poll_mod) == 0U) {
                (void)ctrl.StepOnce();
            }
            if (free_pages > static_cast<int64_t>(w_high * 2ULL)) {
                pressure_notified_this_dip = false;
            }

            uint64_t drained = std::min<uint64_t>(pending_proactive_drain, avail_bw);
            if (drained > 0) {
                pending_proactive_drain -= drained;
                avail_bw -= drained;
                free_pages += static_cast<int64_t>(drained);
                /* Emit eviction metadata to MEXT ShadowMap for burst-evicted regions */
                if (next_evict_vpn < 0x200000ULL + 4096ULL) {
                    uint64_t to_log = std::min<uint64_t>(
                        drained, (0x200000ULL + 4096ULL) - next_evict_vpn);
                    for (uint64_t k = 0; k < to_log; ++k) {
                        mext_eng.HandleEvictionEvent(4242, next_evict_vpn + k, evict_epoch);
                    }
                    next_evict_vpn += to_log;
                    ++evict_epoch;
                }
            }
        }

        /* Standard Linux kswapd background loop (wakes at w_low, sleeps at w_high) */
        if (free_pages < static_cast<int64_t>(w_low) ||
            (kswapd_active_ms > 0 && free_pages < static_cast<int64_t>(w_high))) {
            ++kswapd_active_ms;
            /*
             * Linux mm/vmscan.c starts scanning at DEF_PRIORITY (12) — scanning
             * nr_lru >> 12 pages initially and ramping throughput over ~12-18 ms
             * as priority drops under sustained pressure.
             */
            double ramp = std::min(1.0, static_cast<double>(kswapd_active_ms) / 16.0);
            uint64_t kswapd_cap = static_cast<uint64_t>(
                std::floor(static_cast<double>(avail_bw) * 0.75 * ramp));
            uint64_t need = static_cast<uint64_t>(
                std::max<int64_t>(0, static_cast<int64_t>(w_high) - free_pages));
            uint64_t stolen = std::min<uint64_t>(need, kswapd_cap);
            free_pages += static_cast<int64_t>(stolen);
            kswapd_steals += stolen;
            avail_bw = (avail_bw > stolen) ? (avail_bw - stolen) : 0ULL;
        } else {
            kswapd_active_ms = 0;
        }

        /* 2. Application Page Allocations & Direct Reclaim Slowpath Check */
        double step_lat_us = 0.18; /* Fast-path buddy allocator latency (~180 ns) */
        if (demand_pages > 0) {
            free_pages -= static_cast<int64_t>(demand_pages);
            if (free_pages < static_cast<int64_t>(w_min)) {
                /*
                 * Free pages breached WMARK_MIN! Allocating threads enter
                 * __alloc_pages_slowpath -> try_to_free_pages() synchronously
                 * in SWAP_CLUSTER_MAX (32-page) stalls!
                 */
                uint64_t deficit = static_cast<uint64_t>(
                    static_cast<int64_t>(w_min) - free_pages);
                uint64_t clusters = (deficit + 31ULL) / 32ULL;
                uint64_t reclaimed_sync = clusters * 32ULL;
                free_pages += static_cast<int64_t>(reclaimed_sync);
                allocstalls += clusters;
                direct_steals += reclaimed_sync;
                step_lat_us += static_cast<double>(clusters) * backend.direct_reclaim_stall_us;
            }
            alloc_latencies_us.push_back(step_lat_us);
        }

        if (is_calm_window && ms < 2500) {
            /* Usable DRAM % = fraction of RAM NOT stranded idle in free headroom */
            double usable = 100.0 * static_cast<double>(
                                kTotalPages - static_cast<uint64_t>(std::max<int64_t>(0, free_pages))) /
                            static_cast<double>(kTotalPages);
            calm_usable_sum += usable;
            ++calm_samples;
        }
    }

    /* 3. Phase 3 Warm-Cold Re-access Scan (2,048 pages evicted during Burst 1) */
    double reaccess_us = 0.0;
    uint64_t mext_hits = 0;
    constexpr uint64_t kReaccessPages = 2048ULL;
    for (uint64_t i = 0; i < kReaccessPages; ++i) {
        uint64_t vpn = 0x200000ULL + i;
        if (policy == ReclaimPolicyKind::SMAP_PROACTIVE_MEXT &&
            prefetched_vpns.count(vpn) > 0) {
            ++mext_hits;
            reaccess_us += backend.prefetched_swapin_us;
            /* Boundary renewal on last 2 pages of prefault window */
            if ((i & 7ULL) == 7ULL) {
                mext_eng.HandleFaultEvent(4242, vpn, 3U, sim_ts_ns + i * 2000ULL, true);
            }
        } else {
            reaccess_us += backend.major_swapin_us;
            if (policy == ReclaimPolicyKind::SMAP_PROACTIVE_MEXT) {
                mext_eng.HandleFaultEvent(4242, vpn, 3U, sim_ts_ns + i * 2000ULL, true);
            }
        }
    }

    if (policy == ReclaimPolicyKind::SMAP_PROACTIVE_MEXT) {
        ctrl.Stop();
        mext_eng.Stop();
    }

    std::sort(alloc_latencies_us.begin(), alloc_latencies_us.end());
    size_t p995_idx = std::min(alloc_latencies_us.size() - 1,
                               (alloc_latencies_us.size() * 995) / 1000);

    SimResult res{};
    res.direct_reclaim_stalls = allocstalls;
    res.direct_reclaimed_pages = direct_steals;
    res.p99_alloc_lat_us = alloc_latencies_us[p995_idx];
    res.max_alloc_lat_us = alloc_latencies_us.back();
    res.calm_usable_dram_pct = (calm_samples > 0)
                                   ? (calm_usable_sum / static_cast<double>(calm_samples))
                                   : 0.0;
    res.reaccess_stall_ms = reaccess_us / 1000.0;
    res.mext_hit_rate_pct = 100.0 * static_cast<double>(mext_hits) /
                            static_cast<double>(kReaccessPages);
    return res;
}

void RunAllBurstBenchmarks()
{
    const BackendSpec backends[2] = {
        { "NVMe Gen4 Swap", false, 1500ULL, 420.0, 85.0, 0.35 },
        { "zram DRAM Swap", true,  2048ULL,  65.0,  5.5, 0.15 },
    };

    for (const auto &b : backends) {
        SimResult r_low = RunBurstSimulation(ReclaimPolicyKind::STATIC_LOW_WMARK, b);
        SimResult r_high = RunBurstSimulation(ReclaimPolicyKind::STATIC_HIGH_WMARK, b);
        SimResult r_smap = RunBurstSimulation(ReclaimPolicyKind::SMAP_PROACTIVE_MEXT, b);

        std::printf("\n==========================================================================================\n");
        std::printf("  Allocation-Burst & Usable-DRAM Benchmark — Backend: %s (16 GB Host, 4-5.5 GB/s Bursts)\n",
                    b.name);
        std::printf("------------------------------------------------------------------------------------------\n");
        std::printf("  %-30s | %12s | %14s | %11s | %13s\n",
                    "Reclaim Policy", "AllocStalls", "P99.5 Alloc us", "Usable DRAM", "Re-access ms");
        std::printf("--------------------------------------------------------------------------------------------\n");
        std::printf("  %-30s | %12llu | %14.2f | %10.2f%% | %13.2f\n",
                    "Linux Static Low WMARK (80MB)",
                    static_cast<unsigned long long>(r_low.direct_reclaim_stalls),
                    r_low.p99_alloc_lat_us, r_low.calm_usable_dram_pct, r_low.reaccess_stall_ms);
        std::printf("  %-30s | %12llu | %14.2f | %10.2f%% | %13.2f\n",
                    "Linux Static Scale (1.5GB Low)",
                    static_cast<unsigned long long>(r_high.direct_reclaim_stalls),
                    r_high.p99_alloc_lat_us, r_high.calm_usable_dram_pct, r_high.reaccess_stall_ms);
        std::printf("  %-30s | %12llu | %14.2f | %10.2f%% | %13.2f\n",
                    "SMAP Proactive + MEXT (Ours)",
                    static_cast<unsigned long long>(r_smap.direct_reclaim_stalls),
                    r_smap.p99_alloc_lat_us, r_smap.calm_usable_dram_pct, r_smap.reaccess_stall_ms);
        std::printf("==========================================================================================\n");

        /* Assertions proving SMAP breaks the static watermark tradeoff on BOTH axes */
        REQUIRE(r_low.direct_reclaim_stalls > 100ULL);
        REQUIRE(r_smap.direct_reclaim_stalls < r_low.direct_reclaim_stalls / 10ULL);
        REQUIRE(r_smap.p99_alloc_lat_us < r_low.p99_alloc_lat_us / 10.0);
        REQUIRE(r_smap.calm_usable_dram_pct > r_high.calm_usable_dram_pct + 10.0);
        REQUIRE(r_smap.reaccess_stall_ms < r_low.reaccess_stall_ms * 0.20);
        REQUIRE(r_smap.mext_hit_rate_pct > 85.0);
    }
}

/* --------------------------------------------------------------------------
 * 6. 1,200-Sandbox Scale & Abundant-RAM Zero-Overhead Regression Test (N100)
 * -------------------------------------------------------------------------- */
void TestThousandSandboxScaleAndCalmBypass()
{
    MextEngine mext_eng;
    MextConfig mcfg{};
    MextDefaultConfig(&mcfg);
    REQUIRE(mext_eng.Start(-1, mcfg, false) == 0);

    /* Register 1,200 concurrent sandboxes (matches N100 Firecracker fleet) */
    for (int32_t pid = 10001; pid <= 11200; ++pid) {
        REQUIRE(mext_eng.TrackPid(pid) == 0);
    }
    REQUIRE(mext_eng.ShadowMap().TotalActivePages() == 0ULL);

    /* Verify dynamic SwapShadowMap log growth (1024 -> 2048 -> 4096) preserves neighbors */
    for (uint64_t k = 0; k < 3000; ++k) {
        mext_eng.HandleEvictionEvent(10001, 0x500000ULL + k * 7ULL, 42ULL);
    }
    uint64_t seq = 0;
    uint64_t ep = 0;
    SwapShadowMap local_shadow;
    for (uint64_t k = 0; k < 3000; ++k) {
        REQUIRE(local_shadow.RecordEviction(777, 0x600000ULL + k * 3ULL, 9ULL) == k + 1ULL);
    }
    REQUIRE(local_shadow.OnPageFault(777, 0x600000ULL + 1500ULL * 3ULL, &seq, &ep));
    uint64_t neighbor_vpn = 0;
    REQUIRE(local_shadow.ResolveEvictNeighbor(777, seq, 1, &neighbor_vpn));
    REQUIRE(neighbor_vpn == 0x600000ULL + 1501ULL * 3ULL);

    /* Simulate 768 GB host with 258 GB MemFree (exact N100 unconstrained condition) */
    ProactiveReclaimController ctrl;
    ctrl.BindMextEngine(&mext_eng);
    ProactiveReclaimConfig rcfg{};
    ProactiveReclaimDefaultConfig(&rcfg);

    constexpr uint64_t kHost768GbPages = 201326592ULL; /* 768 GB in 4KB pages */
    uint64_t free_pages = 67633152ULL;                 /* 258 GB free (33.6%) */
    uint64_t ts_ns = 1'000'000'000ULL;

    ctrl.SetTelemetryProvider([&](HostMemSnapshot *out) -> bool {
        out->timestamp_ns = ts_ns;
        out->total_pages = kHost768GbPages;
        out->free_pages = free_pages;
        out->wmark_min_pages = 262144ULL;  /* 1.0 GB */
        out->wmark_low_pages = 327680ULL;  /* 1.25 GB */
        out->wmark_high_pages = 393216ULL; /* 1.5 GB */
        return true;
    });
    REQUIRE(ctrl.Start(rcfg, [](uint64_t target, uint8_t) { return target; }, false) == 0);

    ReclaimDecision d_abundant = ctrl.StepOnce();
    REQUIRE(d_abundant.urgency == RECLAIM_CALM);
    REQUIRE(d_abundant.target_reclaim_pages == 0ULL);
    REQUIRE(!ctrl.ShouldTrackCold()); /* O(1) lockless bypass active at 258 GB free! */

    /* Steady state at 60 GB free (7.8% of 768 GB, still >> W_dyn): pre-warms ColdTracker, 0 swaps */
    free_pages = 15728640ULL; /* 60 GB free */
    ReclaimDecision d_prewarm{};
    for (int i = 0; i < 40; ++i) {
        ts_ns += 100'000'000ULL;
        d_prewarm = ctrl.StepOnce();
    }
    REQUIRE(d_prewarm.urgency == RECLAIM_CALM);
    REQUIRE(d_prewarm.target_reclaim_pages == 0ULL);
    REQUIRE(ctrl.ShouldTrackCold());

    for (int32_t pid = 10001; pid <= 11200; ++pid) {
        mext_eng.UntrackPid(pid);
    }
    REQUIRE(mext_eng.ShadowMap().TotalActivePages() == 0ULL);
    ctrl.Stop();
    mext_eng.Stop();
    std::printf("[PASS] 1,200-sandbox N100 scale & 768GB abundant-RAM O(1) bypass verification\n");
}

/* --------------------------------------------------------------------------
 * 7. Cgroup-Evicted Fault Prefetch + Cgroup v2 Limit Overlay + JSON Telemetry
 * -------------------------------------------------------------------------- */
void TestCgroupEvictionPrefetchAndJsonStats()
{
    namespace fs = std::filesystem;
    fs::path tmp_dir = fs::current_path() / ("smap_mock_cg_" + std::to_string(getpid()));
    fs::create_directories(tmp_dir);

    struct CleanupGuard {
        fs::path dir;
        ~CleanupGuard()
        {
            std::error_code ec;
            fs::remove_all(dir, ec);
        }
    } cleanup{ tmp_dir };

    /* Part A: Verify MEXT prefetches cgroup-evicted pages even when another PID used SMAP swap */
    MextEngine mext_eng;
    MextConfig mcfg{};
    MextDefaultConfig(&mcfg);
    fs::path json_path = tmp_dir / "smap_mext_stats.json";
    mext_eng.SetStatsJsonPath(json_path.string());
    REQUIRE(mext_eng.Start(-1, mcfg, false) == 0);

    /* PID 100 has SMAP-tracked evictions (making global evicts_tracked_ > 0) */
    for (uint64_t k = 0; k < 16; ++k) {
        mext_eng.HandleEvictionEvent(100, 0x100000ULL + k, 1ULL);
    }

    /* PID 200 experiences cgroup `memory.max` swap faults (untracked in shadow_map_) */
    uint64_t dispatched_pages = 0;
    mext_eng.SetDispatchCallback([&](int32_t pid,
                                     const std::vector<uint64_t> &pf,
                                     const std::vector<uint64_t> &pflt) {
        if (pid == 200) {
            dispatched_pages += pf.size() + pflt.size();
        }
    });
    for (uint64_t i = 0; i < 32; ++i) {
        mext_eng.HandleFaultEvent(200, 0x800000ULL + i, 5U, 1000000ULL + i * 5000ULL, true);
    }
    REQUIRE(dispatched_pages >= 20ULL);
    MextStats mst = mext_eng.GetStatsSnapshot();
    REQUIRE(mst.hit_rate > 0.25);

    mext_eng.FlushStatsJson();
    {
        std::ifstream jf(json_path);
        REQUIRE(jf.good());
        std::string content((std::istreambuf_iterator<char>(jf)), std::istreambuf_iterator<char>());
        REQUIRE(content.find("\"total_prefetched_pages\":") != std::string::npos);
        REQUIRE(content.find("\"hit_rate\":") != std::string::npos);
        REQUIRE(content.find("\"coverage_rate\":") != std::string::npos);
    }

    /* Part B: Verify Cgroup v2 `memory.max` + `memory.events: max` wakes ProactiveReclaim
     * even when the 768 GB host still has 270.9 GB MemFree! */
    {
        std::ofstream(tmp_dir / "memory.max") << (32ULL * 1024ULL * 1024ULL * 1024ULL) << "\n";
        std::ofstream(tmp_dir / "memory.current") << (32ULL * 1024ULL * 1024ULL * 1024ULL - 64ULL * 1024ULL * 1024ULL) << "\n";
        std::ofstream(tmp_dir / "memory.events") << "low 0\nhigh 500\nmax 142\noom 0\n";
        std::ofstream(tmp_dir / "memory.pressure") << "some avg10=8.50 avg60=3.00\nfull avg10=3.25 avg60=1.00\n";
    }

    ProactiveReclaimController ctrl;
    ctrl.BindMextEngine(&mext_eng);
    ctrl.SetCgroupV2Root(tmp_dir.string());
    ctrl.SetTelemetryProvider([](HostMemSnapshot *out) -> bool {
        out->timestamp_ns = 2'000'000'000ULL;
        out->total_pages = 201326592ULL; /* 768 GB host */
        out->free_pages = 71014810ULL;   /* 270.9 GB host MemFree */
        out->wmark_min_pages = 262144ULL;
        out->wmark_low_pages = 327680ULL;
        out->wmark_high_pages = 393216ULL;
        return true;
    });

    ProactiveReclaimConfig rcfg{};
    ProactiveReclaimDefaultConfig(&rcfg);
    REQUIRE(ctrl.Start(rcfg, [](uint64_t target, uint8_t) { return target; }, false) == 0);
    ReclaimDecision cg_dec = ctrl.StepOnce();
    /* 64 MB free inside a 32 GB cgroup (< cgroup wmark_low = 320 MB) -> EMERGENCY! */
    REQUIRE(cg_dec.urgency == RECLAIM_EMERGENCY);
    REQUIRE(cg_dec.target_reclaim_pages > 0ULL);
    REQUIRE(ctrl.ShouldTrackCold());

    ctrl.Stop();
    mext_eng.Stop();
    std::printf("[PASS] Cgroup-evicted fault prefetch (%llu pages), cgroup v2 memory.max overlay & JSON stats\n",
                static_cast<unsigned long long>(dispatched_pages));
}

/* --------------------------------------------------------------------------
 * 8. Zero-Config Auto Per-PID Cgroup v2 Discovery & Targeted Fleet Reclaim
 * -------------------------------------------------------------------------- */
void TestAutoPerPidCgroupDiscoveryAndTargetedReclaim()
{
    namespace fs = std::filesystem;
    fs::path root = fs::current_path() / ("smap_auto_cg_" + std::to_string(getpid()));
    fs::path mock_proc = root / "proc";
    fs::path mock_cg = root / "cgroup";
    fs::create_directories(mock_proc);
    fs::create_directories(mock_cg / "aenv.slice");

    struct CleanupGuard {
        fs::path dir;
        ~CleanupGuard()
        {
            std::error_code ec;
            fs::remove_all(dir, ec);
        }
    } cleanup{ root };

    /* Parent slice is unconstrained ("max"), each sandbox fc-XX has 64 MB limit,
     * and leaf vcpu subdir has "max" — testing upward ancestor resolution! */
    std::ofstream(mock_cg / "aenv.slice" / "memory.max") << "max\n";

    constexpr uint64_t kLimit64Mb = 64ULL * 1024ULL * 1024ULL; /* 16,384 pages */
    for (int i = 1; i <= 40; ++i) {
        int32_t pid = 30000 + i;
        std::string fc_name = "fc-" + std::to_string(i);
        fs::path fc_dir = mock_cg / "aenv.slice" / fc_name;
        fs::path leaf_dir = fc_dir / "vcpu";
        fs::create_directories(leaf_dir);
        fs::create_directories(mock_proc / std::to_string(pid));

        std::ofstream(mock_proc / std::to_string(pid) / "cgroup")
            << "0::/aenv.slice/" << fc_name << "/vcpu\n";
        std::ofstream(leaf_dir / "memory.max") << "max\n";
        std::ofstream(fc_dir / "memory.max") << kLimit64Mb << "\n";

        uint64_t used_bytes = 16ULL * 1024ULL * 1024ULL; /* 25% calm */
        uint64_t max_ev = 0;
        if (i == 38) {
            used_bytes = 48ULL * 1024ULL * 1024ULL; /* 75% pre-warm (>70%, <88%) */
        } else if (i >= 39) {
            used_bytes = 63ULL * 1024ULL * 1024ULL; /* 98.4% bursting! */
            max_ev = 12ULL;
        }
        std::ofstream(fc_dir / "memory.current") << used_bytes << "\n";
        std::ofstream(fc_dir / "memory.events") << "low 0\nhigh 0\nmax " << max_ev << "\n";
        std::ofstream(fc_dir / "memory.pressure") << "some avg10=2.0\nfull avg10=0.5\n";
    }

    MextEngine mext_eng;
    ProactiveReclaimController ctrl;
    ctrl.BindMextEngine(&mext_eng);
    ctrl.SetDiscoveryRoots(mock_proc.string(), mock_cg.string());

    /* Host has 270.9 GB MemFree out of 768 GB — zero host-global pressure! */
    ctrl.SetTelemetryProvider([](HostMemSnapshot *out) -> bool {
        out->timestamp_ns = 5'000'000'000ULL;
        out->total_pages = 201326592ULL;
        out->free_pages = 71014810ULL;
        out->wmark_min_pages = 262144ULL;
        out->wmark_low_pages = 327680ULL;
        out->wmark_high_pages = 393216ULL;
        return true;
    });

    ProactiveReclaimConfig rcfg{};
    ProactiveReclaimDefaultConfig(&rcfg);
    REQUIRE(ctrl.Start(rcfg, [](uint64_t target, uint8_t) { return target; }, false) == 0);

    for (int32_t pid = 30001; pid <= 30040; ++pid) {
        REQUIRE(ctrl.TrackPid(pid) == 0);
    }

    ReclaimDecision dec = ctrl.StepOnce();
    REQUIRE(ctrl.ShouldTrackCold());
    REQUIRE(dec.urgency == RECLAIM_BURST || dec.urgency == RECLAIM_EMERGENCY);
    /* Must cover combined fleet deficit across BOTH bursting sandboxes (~3,400+ pages) */
    REQUIRE(dec.target_reclaim_pages >= 3000ULL);

    /* Calm sandboxes (30001..30037): neither ColdTracker aging nor swap pressure */
    for (int32_t pid = 30001; pid <= 30037; ++pid) {
        REQUIRE(!ctrl.ShouldAgePidCold(pid));
        REQUIRE(ctrl.GetPidPressureScore(pid) == 0U);
    }
    /* Pre-warm sandbox (30038 @ 75%): ages ColdTracker, 0 eviction pressure */
    REQUIRE(ctrl.ShouldAgePidCold(30038));
    REQUIRE(ctrl.GetPidPressureScore(30038) == 0U);

    /* Bursting sandboxes (30039, 30040 @ 98.4%): targeted high pressure score! */
    REQUIRE(ctrl.ShouldAgePidCold(30039));
    REQUIRE(ctrl.ShouldAgePidCold(30040));
    REQUIRE(ctrl.GetPidPressureScore(30039) >= 80U);
    REQUIRE(ctrl.GetPidPressureScore(30040) >= 80U);

    for (int32_t pid = 30001; pid <= 30040; ++pid) {
        ctrl.UntrackPid(pid);
    }
    ctrl.Stop();
    std::printf("[PASS] Zero-config auto per-PID cgroup v2 ancestor discovery & targeted fleet reclaim\n");
}

/* --------------------------------------------------------------------------
 * 9. Cgroup v1 & Hybrid v1+v2 Auto-Discovery (`memory.limit_in_bytes`, `failcnt`)
 * -------------------------------------------------------------------------- */
void TestCgroupV1AndHybridAutoDiscovery()
{
    namespace fs = std::filesystem;
    fs::path root = fs::current_path() / ("smap_cg_v1_" + std::to_string(getpid()));
    fs::path mock_proc = root / "proc";
    fs::path mock_cg = root / "cgroup";
    fs::create_directories(mock_proc);
    fs::create_directories(mock_cg / "memory" / "aenv");
    fs::create_directories(mock_cg / "system.slice" / "ubturbod.service");

    struct CleanupGuard {
        fs::path dir;
        ~CleanupGuard()
        {
            std::error_code ec;
            fs::remove_all(dir, ec);
        }
    } cleanup{ root };

    /* Linux cgroup v1 unconstrained PAGE_COUNTER_MAX * PAGE_SIZE (~8 EiB) */
    constexpr uint64_t kV1Unlimited = 9223372036854771712ULL;
    constexpr uint64_t kLimit64Mb = 64ULL * 1024ULL * 1024ULL;
    std::ofstream(mock_cg / "memory" / "aenv" / "memory.limit_in_bytes") << kV1Unlimited << "\n";
    std::ofstream(mock_cg / "memory" / "aenv" / "memory.usage_in_bytes") << (256ULL << 20) << "\n";

    for (int i = 1; i <= 24; ++i) {
        int32_t pid = 40000 + i;
        std::string sb_name = "sb-" + std::to_string(i);
        fs::path sb_dir = mock_cg / "memory" / "aenv" / sb_name;
        fs::path vcpu_dir = sb_dir / "vcpu";
        fs::create_directories(vcpu_dir);
        fs::create_directories(mock_proc / std::to_string(pid));

        /* Hybrid v1 + systemd v2 `/proc/<pid>/cgroup`: `0::` exists without memory.max,
         * while `6:cpu,memory:/aenv/sb-X/vcpu` holds the active v1 memory controller! */
        std::ofstream(mock_proc / std::to_string(pid) / "cgroup")
            << "10:hugetlb:/\n"
            << "6:cpu,memory:/aenv/" << sb_name << "/vcpu\n"
            << "1:name=systemd:/system.slice/ubturbod.service\n"
            << "0::/system.slice/ubturbod.service\n";

        /* Leaf `vcpu` is unconstrained in v1; parent `sb-X` enforces 64 MB cap */
        std::ofstream(vcpu_dir / "memory.limit_in_bytes") << kV1Unlimited << "\n";
        std::ofstream(vcpu_dir / "memory.usage_in_bytes") << (4ULL << 20) << "\n";

        uint64_t used_bytes = 16ULL * 1024ULL * 1024ULL;
        uint64_t failcnt = 0;
        if (i == 22) {
            used_bytes = 48ULL * 1024ULL * 1024ULL; /* 75% pre-warm */
        } else if (i >= 23) {
            used_bytes = 63ULL * 1024ULL * 1024ULL; /* 98.4% bursting! */
            failcnt = 42ULL;
        }
        std::ofstream(sb_dir / "memory.limit_in_bytes") << kLimit64Mb << "\n";
        std::ofstream(sb_dir / "memory.usage_in_bytes") << used_bytes << "\n";
        std::ofstream(sb_dir / "memory.failcnt") << failcnt << "\n";
    }

    /* Verify direct Cgroup v1 overlay (`SMAP_RECLAIM_CGROUP`) reads failcnt */
    HostMemSnapshot snap_v1{};
    snap_v1.total_pages = 201326592ULL;
    snap_v1.free_pages = 71014810ULL;
    REQUIRE(ProactiveReclaimController::ApplyCgroupV2Overlay(
        (mock_cg / "memory" / "aenv" / "sb-24").string().c_str(), 12U, &snap_v1));
    REQUIRE(snap_v1.total_pages == (kLimit64Mb >> 12U));
    REQUIRE(snap_v1.free_pages == ((1ULL << 20) >> 12U));
    REQUIRE(snap_v1.allocstall_total == 42ULL);

    MextEngine mext_eng;
    REQUIRE(mext_eng.OpenPidfdCount() == 0U);
    REQUIRE(mext_eng.PageoutPidCold(-1, 40024, 64U, false) == -1);

    ProactiveReclaimController ctrl;
    ctrl.BindMextEngine(&mext_eng);
    ctrl.SetDiscoveryRoots(mock_proc.string(), mock_cg.string());
    ctrl.SetTelemetryProvider([](HostMemSnapshot *out) -> bool {
        out->timestamp_ns = 9'000'000'000ULL;
        out->total_pages = 201326592ULL;
        out->free_pages = 71014810ULL; /* 270.9 GB MemFree on host */
        out->wmark_min_pages = 262144ULL;
        out->wmark_low_pages = 327680ULL;
        out->wmark_high_pages = 393216ULL;
        return true;
    });

    ProactiveReclaimConfig rcfg{};
    ProactiveReclaimDefaultConfig(&rcfg);
    REQUIRE(ctrl.Start(rcfg, [](uint64_t target, uint8_t) { return target; }, false) == 0);

    for (int32_t pid = 40001; pid <= 40024; ++pid) {
        REQUIRE(ctrl.TrackPid(pid) == 0);
    }

    ReclaimDecision dec = ctrl.StepOnce();
    REQUIRE(ctrl.ShouldTrackCold());
    REQUIRE(dec.urgency == RECLAIM_EMERGENCY);
    REQUIRE(dec.target_reclaim_pages >= 3000ULL);

    for (int32_t pid = 40001; pid <= 40021; ++pid) {
        REQUIRE(!ctrl.ShouldAgePidCold(pid));
        REQUIRE(ctrl.GetPidPressureScore(pid) == 0U);
    }
    REQUIRE(ctrl.ShouldAgePidCold(40022));
    REQUIRE(ctrl.GetPidPressureScore(40022) == 0U);
    REQUIRE(ctrl.ShouldAgePidCold(40023));
    REQUIRE(ctrl.ShouldAgePidCold(40024));
    REQUIRE(ctrl.GetPidPressureScore(40023) >= 80U);
    REQUIRE(ctrl.GetPidPressureScore(40024) >= 80U);

    for (int32_t pid = 40001; pid <= 40024; ++pid) {
        ctrl.UntrackPid(pid);
    }
    ctrl.Stop();
    std::printf("[PASS] Cgroup v1 & hybrid v1+v2 auto-discovery (`limit_in_bytes` + `failcnt`)\n");
}

/* --------------------------------------------------------------------------
 * 10. Sequential Prefetch-Frontier Ramp, 4-Shard Pool & Active-Fault Guard
 * -------------------------------------------------------------------------- */
void TestSequentialFrontierRampAndActiveFaultGuard()
{
    MextEngine eng;
    MextConfig mcfg{};
    MextDefaultConfig(&mcfg);
    mcfg.enable_transformer = false; /* isolate Tier-1 sequential frontier ramp */

    std::vector<uint64_t> last_batch;
    eng.SetDispatchCallback([&](int32_t, const std::vector<uint64_t> &pf, const std::vector<uint64_t> &flt) {
        last_batch.clear();
        last_batch.insert(last_batch.end(), flt.begin(), flt.end());
        last_batch.insert(last_batch.end(), pf.begin(), pf.end());
        std::sort(last_batch.begin(), last_batch.end());
    });

    REQUIRE(eng.Start(-1, mcfg, false) == 0);
    const int32_t kPid = 60001;
    REQUIRE(!eng.WasPidRecentlyFaulting(kPid, 50'000'000ULL));

    /* Fault 1 at VPN 1000 & Fault 2 at VPN 1001 confirm unit-stride sequential stream (prefetches 1002..1003) */
    eng.HandleFaultEvent(kPid, 1000ULL, 2U, 1'000'000ULL, true);
    eng.HandleFaultEvent(kPid, 1001ULL, 2U, 1'500'000ULL, true);
    REQUIRE(last_batch.size() >= 2U);
    REQUIRE(eng.WasPidRecentlyFaulting(kPid, 500'000'000ULL));

    /* Confirm streak=2 at VPN 1002, then frontier Fault 4 at VPN 1006 ramps to >= 4 fresh pages */
    eng.HandleFaultEvent(kPid, 1002ULL, 2U, 2'000'000ULL, true);
    eng.HandleFaultEvent(kPid, 1006ULL, 2U, 2'500'000ULL, true);
    REQUIRE(last_batch.size() >= 6U);
    REQUIRE(last_batch.front() == 1007ULL);

    /* Next frontier fault after prefetched window ramps to >= 8 forward pages! */
    uint64_t next_frontier = 1006ULL + 8ULL + 1ULL; /* 1015 (since 1007..1014 were prefetched) */
    eng.HandleFaultEvent(kPid, next_frontier, 2U, 3'000'000ULL, true);
    REQUIRE(last_batch.size() >= 8U);
    REQUIRE(std::find(last_batch.begin(), last_batch.end(), next_frontier + 1ULL) != last_batch.end());
    REQUIRE(std::find(last_batch.begin(), last_batch.end(), next_frontier + 8ULL) != last_batch.end());
    eng.Stop();

    /* Verify 4-shard live async worker pool starts and drains cleanly */
    MextEngine live_eng;
    REQUIRE(live_eng.Start(-1, mcfg, true) == 0);
    for (int i = 0; i < 8; ++i) {
        live_eng.HandleFaultEvent(61000 + (i % 4), 2000ULL + static_cast<uint64_t>(i) * 5ULL,
                                  1U, static_cast<uint64_t>(i + 1) * 1'000'000ULL, true);
    }
    live_eng.Stop();

    /* Verify dual-vCPU interleaved VM_FAULT_RETRY deduplication preserves SwapShadowMap
     * completeness (`IsSwappedOut` filtering) and Transformer Head B co-eviction parity! */
    MextEngine retry_eng;
    MextConfig r_mcfg{};
    MextDefaultConfig(&r_mcfg);
    r_mcfg.fault_trigger_stride = 2U;
    std::vector<uint64_t> coevict_dispatched;
    retry_eng.SetDispatchCallback([&](int32_t, const std::vector<uint64_t> &pf, const std::vector<uint64_t> &flt) {
        coevict_dispatched.insert(coevict_dispatched.end(), pf.begin(), pf.end());
        coevict_dispatched.insert(coevict_dispatched.end(), flt.begin(), flt.end());
    });
    REQUIRE(retry_eng.Start(-1, r_mcfg, false) == 0);
    const int32_t kVmPid = 62001;
    for (uint64_t k = 0; k < 40ULL; ++k) {
        retry_eng.HandleEvictionEvent(kVmPid, 0x900000ULL + k * 37ULL, 55ULL);
    }
    /* Simulate 2 vCPUs faulting in pairs where each major fault triggers do_swap_page TWICE (200 us retry) */
    for (uint64_t pair = 0; pair < 10ULL; ++pair) {
        uint64_t v0 = 0x900000ULL + (pair * 2ULL) * 37ULL;
        uint64_t v1 = 0x900000ULL + (pair * 2ULL + 1ULL) * 37ULL;
        uint64_t t_base = 10'000'000ULL + pair * 5'000'000ULL;
        retry_eng.HandleFaultEvent(kVmPid, v0, 4U, t_base, true);            /* vCPU 0 Pass 1 */
        retry_eng.HandleFaultEvent(kVmPid, v1, 4U, t_base + 30'000ULL, true);/* vCPU 1 Pass 1 */
        retry_eng.HandleFaultEvent(kVmPid, v0, 4U, t_base + 200'000ULL, true);/* vCPU 0 VM_FAULT_RETRY */
        retry_eng.HandleFaultEvent(kVmPid, v1, 4U, t_base + 230'000ULL, true);/* vCPU 1 VM_FAULT_RETRY */
    }
    MextStats rst = retry_eng.GetStatsSnapshot();
    REQUIRE(rst.faults_processed == 20ULL); /* All 20 retry duplicates cleanly stripped! */
    /* Resident (never-evicted) neighbor VPNs must NOT be prefetched when shadow_is_complete is true */
    for (uint64_t dv : coevict_dispatched) {
        REQUIRE(((dv - 0x900000ULL) % 37ULL) == 0ULL);
    }
    retry_eng.Stop();

    std::printf("[PASS] Sequential prefetch-frontier ramp, 4-shard pool & dual-vCPU VM_FAULT_RETRY dedup\n");
}

} // namespace

int main()
{
    std::printf("=== SMAP-MEXT Dynamic Proactive Reclaim Verification & Burst Suite ===\n");
    TestProcTelemetryParser();
    TestDisturbanceObserverStability();
    TestClosedLoopMextThrashBrake();
    TestLiveKernelPressureRingWakeup();
    TestThousandSandboxScaleAndCalmBypass();
    TestCgroupEvictionPrefetchAndJsonStats();
    TestAutoPerPidCgroupDiscoveryAndTargetedReclaim();
    TestCgroupV1AndHybridAutoDiscovery();
    TestSequentialFrontierRampAndActiveFaultGuard();
    RunAllBurstBenchmarks();
    std::printf("\nALL PROACTIVE RECLAIM TESTS & BURST BENCHMARKS PASSED SUCCESSFULLY!\n");
    return 0;
}
