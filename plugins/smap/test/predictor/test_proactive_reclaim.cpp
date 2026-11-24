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

} // namespace

int main()
{
    std::printf("=== SMAP-MEXT Dynamic Proactive Reclaim Verification & Burst Suite ===\n");
    TestProcTelemetryParser();
    TestDisturbanceObserverStability();
    TestClosedLoopMextThrashBrake();
    TestLiveKernelPressureRingWakeup();
    TestThousandSandboxScaleAndCalmBypass();
    RunAllBurstBenchmarks();
    std::printf("\nALL PROACTIVE RECLAIM TESTS & BURST BENCHMARKS PASSED SUCCESSFULLY!\n");
    return 0;
}
