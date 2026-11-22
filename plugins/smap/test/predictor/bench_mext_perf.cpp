/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Event-driven NVMe Queue + Real CPU Inference Timing Benchmark.
 *              Drives the compiled MextEngine (including live ARM64 NEON INT8
 *              inference latency) against an NVMe SSD I/O completion model:
 *                - DRAM hit latency:               0.10 us (100 ns)
 *                - Minor fault (swap-cache hit):   0.85 us
 *                - NVMe 4KB read latency:         45.00 us (PCIe Gen4/5 SSD)
 *                - NVMe serialization / BW:        0.80 us/IO (~1.25M IOPS)
 */

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../../src/user/predictor/mext_engine.h"
#include "../../src/user/predictor/mext_predictor_c_api.h"

using namespace smap::mext;

namespace {

constexpr double kDramHitUs = 0.10;
constexpr double kMinorFaultUs = 0.85;
constexpr double kNvmeReadLatencyUs = 45.0;
constexpr double kNvmePerIoChannelSerializeUs = 0.80;
constexpr double kComputeBetweenMemOpsUs = 6.0;

struct AccessOp {
    uint64_t vpn;
    uint8_t vma;
    bool starts_on_nvme;
};

struct SimResult {
    double all_dram_ms = 0.0;
    double reactive_swap_ms = 0.0;
    double linux_ra_ms = 0.0;
    double mext_ms = 0.0;
    uint64_t major_stalls_reactive = 0;
    uint64_t major_stalls_mext = 0;
    uint64_t partial_stalls_mext = 0;
    uint64_t minor_hits_mext = 0;
    uint64_t zero_fault_hits_mext = 0;
    uint64_t wasted_ios_mext = 0;
    double total_cpu_infer_ms = 0.0;
};

struct XorShift64 {
    uint64_t s;
    explicit XorShift64(uint64_t seed) : s(seed) {}
    uint64_t Next()
    {
        uint64_t x = s;
        x ^= x << 13U;
        x ^= x >> 7U;
        x ^= x << 17U;
        s = x;
        return x;
    }
};

SimResult RunTimingSimulation(const std::vector<AccessOp> &ops,
                              const std::vector<std::pair<uint64_t, uint64_t>> &evictions)
{
    SimResult res{};
    const int32_t pid = 9001;

    /* 1. All-DRAM baseline (100% physical DRAM, zero swap) */
    res.all_dram_ms = (static_cast<double>(ops.size()) * (kComputeBetweenMemOpsUs + kDramHitUs)) / 1000.0;

    /* 2. Reactive Linux swap (vm.page-cluster=0, synchronous do_swap_page) */
    {
        std::unordered_set<uint64_t> on_nvme;
        for (const auto &ev : evictions) {
            on_nvme.insert(ev.first);
        }
        double now_us = 0.0;
        for (const auto &op : ops) {
            now_us += kComputeBetweenMemOpsUs;
            if (on_nvme.erase(op.vpn) > 0) {
                now_us += kNvmeReadLatencyUs + kMinorFaultUs;
                ++res.major_stalls_reactive;
            } else {
                now_us += kDramHitUs;
            }
        }
        res.reactive_swap_ms = now_us / 1000.0;
    }

    /* 3. Standard Linux VMA/cluster readahead (prefetches vpn+1..vpn+3 on fault) */
    {
        std::unordered_set<uint64_t> on_nvme;
        for (const auto &ev : evictions) {
            on_nvme.insert(ev.first);
        }
        std::unordered_map<uint64_t, double> ready_time_us;
        double nvme_bus_free_us = 0.0;
        double now_us = 0.0;

        for (const auto &op : ops) {
            now_us += kComputeBetweenMemOpsUs;
            auto it_ready = ready_time_us.find(op.vpn);
            if (it_ready != ready_time_us.end()) {
                if (it_ready->second > now_us) {
                    now_us = it_ready->second;
                }
                now_us += kMinorFaultUs;
                ready_time_us.erase(it_ready);
            } else if (on_nvme.erase(op.vpn) > 0) {
                double start_io = std::max(now_us, nvme_bus_free_us);
                nvme_bus_free_us = start_io + kNvmePerIoChannelSerializeUs;
                now_us = start_io + kNvmeReadLatencyUs + kMinorFaultUs;
                for (uint64_t k = 1; k <= 3; ++k) {
                    if (on_nvme.erase(op.vpn + k) > 0) {
                        double s_ra = std::max(now_us - kNvmeReadLatencyUs, nvme_bus_free_us);
                        nvme_bus_free_us = s_ra + kNvmePerIoChannelSerializeUs;
                        ready_time_us[op.vpn + k] = s_ra + kNvmeReadLatencyUs;
                    }
                }
            } else {
                now_us += kDramHitUs;
            }
        }
        res.linux_ra_ms = now_us / 1000.0;
    }

    /* 4. UBTurbo SMAP-MEXT Engine (real C++ execution + NVMe queue physics) */
    {
        MextEngine engine;
        MextConfig cfg{};
        MextDefaultConfig(&cfg);
        cfg.fault_trigger_stride = 2;
        cfg.top_k = 8;
        cfg.prefetch_threshold = 0.28f;
        cfg.prefault_threshold = 0.72f;
        engine.Start(-1, cfg, false);

        std::unordered_set<uint64_t> on_nvme;
        for (const auto &ev : evictions) {
            on_nvme.insert(ev.first);
            engine.HandleEvictionEvent(pid, ev.first, ev.second);
        }

        struct InflightPage {
            double ready_us;
            bool pte_prefaulted;
            bool used;
        };
        std::unordered_map<uint64_t, InflightPage> swap_cache;
        double nvme_bus_free_us = 0.0;
        double now_us = 0.0;

        engine.SetDispatchCallback([&](int32_t,
                                       const std::vector<uint64_t> &prefetch_vpns,
                                       const std::vector<uint64_t> &prefault_vpns) {
            for (uint64_t v : prefetch_vpns) {
                if (on_nvme.erase(v) > 0) {
                    double s_io = std::max(now_us, nvme_bus_free_us);
                    nvme_bus_free_us = s_io + kNvmePerIoChannelSerializeUs;
                    swap_cache[v] = InflightPage{ s_io + kNvmeReadLatencyUs, false, false };
                }
            }
            for (uint64_t v : prefault_vpns) {
                if (on_nvme.erase(v) > 0) {
                    double s_io = std::max(now_us, nvme_bus_free_us);
                    nvme_bus_free_us = s_io + kNvmePerIoChannelSerializeUs;
                    swap_cache[v] = InflightPage{ s_io + kNvmeReadLatencyUs + 0.35, true, false };
                }
            }
        });

        for (const auto &op : ops) {
            now_us += kComputeBetweenMemOpsUs;
            auto it = swap_cache.find(op.vpn);
            if (it != swap_cache.end()) {
                it->second.used = true;
                if (it->second.ready_us > now_us) {
                    now_us = it->second.ready_us;
                    ++res.partial_stalls_mext;
                }
                if (it->second.pte_prefaulted) {
                    now_us += kDramHitUs;
                    ++res.zero_fault_hits_mext;
                } else {
                    now_us += kMinorFaultUs;
                    ++res.minor_hits_mext;
                }
                auto t0 = std::chrono::steady_clock::now();
                engine.HandleFaultEvent(pid, op.vpn, op.vma, static_cast<uint64_t>(now_us * 1000.0));
                auto t1 = std::chrono::steady_clock::now();
                res.total_cpu_infer_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
                swap_cache.erase(it);
            } else if (on_nvme.erase(op.vpn) > 0) {
                double s_io = std::max(now_us, nvme_bus_free_us);
                nvme_bus_free_us = s_io + kNvmePerIoChannelSerializeUs;
                now_us = s_io + kNvmeReadLatencyUs + kMinorFaultUs;
                ++res.major_stalls_mext;

                auto t0 = std::chrono::steady_clock::now();
                engine.HandleFaultEvent(pid, op.vpn, op.vma, static_cast<uint64_t>(now_us * 1000.0));
                auto t1 = std::chrono::steady_clock::now();
                res.total_cpu_infer_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
            } else {
                now_us += kDramHitUs;
            }
        }

        for (const auto &kv : swap_cache) {
            if (!kv.second.used) {
                ++res.wasted_ios_mext;
            }
        }
        res.mext_ms = now_us / 1000.0;
        engine.Stop();
    }

    return res;
}

} // namespace

int main()
{
    constexpr int N = 2000;

    /* Workload 1: EDA Static Timing Graph (50% DRAM + 40% co-evicted graph + 10% unpredicted misses) */
    std::vector<AccessOp> w1_ops;
    std::vector<std::pair<uint64_t, uint64_t>> w1_evicts;
    {
        XorShift64 rng(0xEDA2026ULL);
        std::vector<uint64_t> random_pool;
        for (int b = 0; b < 10000; ++b) {
            uint64_t vpn = 0x1000000ULL + static_cast<uint64_t>(b) * 29ULL + (rng.Next() & 0xFULL);
            random_pool.push_back(vpn);
            w1_evicts.push_back({ vpn, 1 + static_cast<uint64_t>(b / 256) });
        }
        for (size_t i = random_pool.size() - 1; i > 0; --i) {
            std::swap(random_pool[i], random_pool[rng.Next() % (i + 1)]);
        }

        std::vector<uint64_t> swapped_graph;
        for (int i = 0; i < 800; ++i) {
            uint64_t vpn = 0x200000ULL + static_cast<uint64_t>(i) * 257ULL + (rng.Next() & 0x3FULL);
            swapped_graph.push_back(vpn);
            w1_evicts.push_back({ vpn, 80 + static_cast<uint64_t>(i / 64) });
        }

        int g_idx = 0, r_idx = 0;
        for (int i = 0; i < N; ++i) {
            if (i % 2 == 0) {
                w1_ops.push_back({ 0x1000ULL + static_cast<uint64_t>(i), 1, false });
            } else if (i % 10 == 9 && r_idx < 200) {
                w1_ops.push_back({ random_pool[r_idx++], 7, true });
            } else {
                w1_ops.push_back({ swapped_graph[g_idx++ % 800], 3, true });
            }
        }
    }

    /* Workload 2: Agentic AI Multi-Context KV & Tool Heap (50% DRAM + interleaved VMA strides) */
    std::vector<AccessOp> w2_ops;
    std::vector<std::pair<uint64_t, uint64_t>> w2_evicts;
    {
        for (int i = 0; i < 1000; ++i) {
            uint64_t vpn = ((i & 1) == 0) ? (0x400000ULL + static_cast<uint64_t>(i / 2) * 4ULL)
                                          : (0x700000ULL + static_cast<uint64_t>(i / 2) * 8ULL);
            uint8_t vma = ((i & 1) == 0) ? 2 : 11;
            w2_evicts.push_back({ vpn, 5 + static_cast<uint64_t>(i / 100) });
            w2_ops.push_back({ 0x2000ULL + static_cast<uint64_t>(i), 0, false });
            w2_ops.push_back({ vpn, vma, true });
        }
    }

    /* Workload 3: Unseen Novel Jump (+13 struct stride, Fisher-Yates shuffled eviction order) */
    std::vector<AccessOp> w3_ops;
    std::vector<std::pair<uint64_t, uint64_t>> w3_evicts;
    {
        XorShift64 rng(0x55AA77ULL);
        std::vector<uint64_t> shuf;
        for (int i = 0; i < 1000; ++i) {
            uint64_t vpn = 0x500000ULL + static_cast<uint64_t>(i) * 13ULL;
            shuf.push_back(vpn);
            w3_ops.push_back({ 0x3000ULL + static_cast<uint64_t>(i), 0, false });
            w3_ops.push_back({ vpn, 4, true });
        }
        for (size_t i = shuf.size() - 1; i > 0; --i) {
            std::swap(shuf[i], shuf[rng.Next() % (i + 1)]);
        }
        for (size_t i = 0; i < shuf.size(); ++i) {
            w3_evicts.push_back({ shuf[i], static_cast<uint64_t>(i) });
        }
    }

    /* Workload 4: True Adversarial Fisher-Yates Uniform Random Hash over 20,000 Cold Pages */
    std::vector<AccessOp> w4_ops;
    std::vector<std::pair<uint64_t, uint64_t>> w4_evicts;
    {
        XorShift64 rng(0xDEAD1234ULL);
        std::vector<uint64_t> pool;
        for (int i = 0; i < 20000; ++i) {
            uint64_t vpn = 0xA00000ULL + static_cast<uint64_t>(i) * 19ULL + (rng.Next() & 0xFULL);
            pool.push_back(vpn);
            w4_evicts.push_back({ vpn, 1 + static_cast<uint64_t>(i / 256) });
        }
        for (size_t i = pool.size() - 1; i > 0; --i) {
            std::swap(pool[i], pool[rng.Next() % (i + 1)]);
        }
        for (int i = 0; i < 1000; ++i) {
            w4_ops.push_back({ 0x4000ULL + static_cast<uint64_t>(i), 0, false });
            w4_ops.push_back({ pool[i], static_cast<uint8_t>(rng.Next() & 31U), true });
        }
    }

    struct NamedBench {
        const char *name;
        SimResult r;
    };
    NamedBench benches[] = {
        { "1. EDA Graph (80% graph + 20% rand)", RunTimingSimulation(w1_ops, w1_evicts) },
        { "2. Agentic AI Interleaved Arenas   ", RunTimingSimulation(w2_ops, w2_evicts) },
        { "3. Unseen +13 Stride (Shuf Evict)  ", RunTimingSimulation(w3_ops, w3_evicts) },
        { "4. Adversarial 100% Random Hash    ", RunTimingSimulation(w4_ops, w4_evicts) },
    };

    std::printf("\n%-36s | %9s | %10s | %10s | %9s | %11s | %11s | %8s\n",
                "Workload (50% DRAM + 50% NVMe)", "All-DRAM", "React-Swap", "Linux-RA", "SMAP-MEXT",
                "Boost/Swap", "% All-DRAM", "PredCPU");
    std::printf("-------------------------------------+-----------+------------+------------+-----------+-------------+-------------+---------\n");
    for (const auto &b : benches) {
        double speedup = b.r.reactive_swap_ms / b.r.mext_ms;
        double pct_dram = 100.0 * (b.r.all_dram_ms / b.r.mext_ms);
        std::printf("%-36s | %7.2fms | %8.2fms | %8.2fms | %7.2fms | %9.2fx  | %9.1f%%  | %6.2fms\n",
                    b.name, b.r.all_dram_ms, b.r.reactive_swap_ms, b.r.linux_ra_ms,
                    b.r.mext_ms, speedup, pct_dram, b.r.total_cpu_infer_ms);
    }
    std::printf("\nDetailed MEXT Breakdown (Workload 1 - Realistic Noisy EDA):\n");
    std::printf("  Major sync faults: %" PRIu64 " -> %" PRIu64 " | In-flight partial waits: %" PRIu64 " | Minor cache hits: %" PRIu64 " | Zero-fault PTE hits: %" PRIu64 " | Wasted IOs: %" PRIu64 "\n",
                benches[0].r.major_stalls_reactive, benches[0].r.major_stalls_mext,
                benches[0].r.partial_stalls_mext, benches[0].r.minor_hits_mext,
                benches[0].r.zero_fault_hits_mext, benches[0].r.wasted_ios_mext);
    return 0;
}
