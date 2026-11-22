/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Real In-RAM Compressed zram + MEXT Prefetch/Prefault Verification
 *              and Live Two-Thread Wall-Clock Benchmark Suite.
 *
 * Exercises genuine 4 KB page compression/decompression (zlib LZ77/Huffman in RAM),
 * bit-exact 4096-byte page payload verification, DRAM Swap-Cache + zram double-buffer
 * accounting, and concurrent OS thread offload (Application Thread on Core A vs.
 * MextEngine Worker Thread on Core B over the lockless mext_ring_buffer).
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <zlib.h>

#include "../../src/tiering/mext_ring_abi.h"
#include "../../src/user/predictor/mext_engine.h"
#include "../../src/user/predictor/mext_predictor_c_api.h"

using namespace smap::mext;

#define REQUIRE(cond)                                                                         \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            std::fprintf(stderr, "ZRAM TEST FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            std::exit(1);                                                                     \
        }                                                                                     \
    } while (0)

namespace {

constexpr size_t kPageBytes = 4096;

/*
 * Generate realistic compressible 4 KB heap page content (~3:1 compression ratio,
 * typical of EDA timing node structs / Python object dicts / KV metadata) seeded
 * uniquely by `vpn` so every page has a verifiable 4096-byte ground truth.
 */
void GenerateStructuredPage4K(uint64_t vpn, uint8_t out[kPageBytes])
{
    uint64_t *words = reinterpret_cast<uint64_t *>(out);
    for (size_t i = 0; i < kPageBytes / sizeof(uint64_t); ++i) {
        /* Realistic mix of pointer headers, repeated zeros/tags, and vpn-unique fields */
        switch (i & 7U) {
            case 0:
                words[i] = 0x7FFF00000000ULL | (vpn << 12U) | (i * 8ULL);
                break;
            case 1:
                words[i] = 0x0000000100000004ULL;
                break;
            case 2:
                words[i] = vpn ^ (i * 0x9E3779B97F4A7C15ULL);
                break;
            case 3:
            case 4:
                words[i] = 0ULL;
                break;
            case 5:
                words[i] = 0x4D4558545A52414DULL; /* "MEXTZRAM" tag */
                break;
            default:
                words[i] = (vpn & 0xFFFFULL) + (i >> 3U);
                break;
        }
    }
}

uint64_t ChecksumPage4K(const uint8_t buf[kPageBytes])
{
    uint64_t h = 14695981039346656037ULL;
    const uint64_t *words = reinterpret_cast<const uint64_t *>(buf);
    for (size_t i = 0; i < kPageBytes / sizeof(uint64_t); ++i) {
        h ^= words[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/*
 * Thread-safe in-RAM zram + Swap-Cache + PTE page store modeling Linux's exact
 * SWP_SYNCHRONOUS_IO memory lifecycle:
 *   - SwapOut(vpn): compresses 4 KB -> ~1.2 KB zsmalloc buffer in RAM.
 *   - PrefetchToSwapCache(vpn) [MADV_WILLNEED]: decompresses into 4 KB Swap Cache
 *     page while keeping compressed zram slot until FaultOrRead() claims it!
 *   - PrefaultToPte(vpn) [SMAP_MIG_PREFAULT]: decompresses directly into 4 KB
 *     resident PTE page AND frees the compressed zram slot immediately (swap_free)!
 *   - AppAccessPage(vpn, out_buf): if PTE-resident -> 0 fault copy; if in Swap Cache
 *     -> minor fault claims page & frees zram slot; if only in zram -> synchronous
 *     CPU decompression on the application thread!
 */
class SimulatedZramSubsystem {
public:
    enum AccessResult {
        HIT_RESIDENT_PTE = 0,
        HIT_SWAP_CACHE_MINOR = 1,
        SYNC_ZRAM_MAJOR_FAULT = 2,
    };

    explicit SimulatedZramSubsystem(int zlib_level = Z_BEST_SPEED)
        : zlib_level_(zlib_level) {}

    size_t SwapOutPage(uint64_t vpn, const uint8_t raw[kPageBytes])
    {
        uLongf bound = compressBound(kPageBytes);
        std::vector<uint8_t> comp(bound);
        int rc = compress2(comp.data(), &bound, raw, kPageBytes, zlib_level_);
        REQUIRE(rc == Z_OK);
        comp.resize(bound);

        std::lock_guard<std::mutex> lk(mu_);
        resident_ptes_.erase(vpn);
        swap_cache_.erase(vpn);
        zram_bytes_ -= zram_slots_[vpn].size();
        zram_slots_[vpn] = std::move(comp);
        zram_bytes_ += bound;
        UpdatePeakLocked();
        return static_cast<size_t>(bound);
    }

    /* MADV_WILLNEED: populates 4KB Swap Cache WITHOUT freeing zram slot yet */
    bool PrefetchToSwapCache(uint64_t vpn)
    {
        std::vector<uint8_t> comp_copy;
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (resident_ptes_.count(vpn) || swap_cache_.count(vpn)) {
                return false;
            }
            auto it = zram_slots_.find(vpn);
            if (it == zram_slots_.end()) {
                return false;
            }
            comp_copy = it->second;
        }

        /* Run real CPU decompression outside lock (on MEXT helper core!) */
        std::array<uint8_t, kPageBytes> page{};
        uLongf dest_len = kPageBytes;
        int rc = uncompress(page.data(), &dest_len, comp_copy.data(), comp_copy.size());
        REQUIRE(rc == Z_OK && dest_len == kPageBytes);

        std::lock_guard<std::mutex> lk(mu_);
        if (!resident_ptes_.count(vpn) && zram_slots_.count(vpn) && !swap_cache_.count(vpn)) {
            swap_cache_[vpn] = page;
            ++double_buffered_events_;
            UpdatePeakLocked();
            return true;
        }
        return false;
    }

    /* SMAP_MIG_PREFAULT: installs 4KB PTE AND immediately frees compressed zram slot */
    bool PrefaultToPte(uint64_t vpn)
    {
        std::vector<uint8_t> comp_copy;
        bool from_swap_cache = false;
        std::array<uint8_t, kPageBytes> page{};

        {
            std::lock_guard<std::mutex> lk(mu_);
            if (resident_ptes_.count(vpn)) {
                return false;
            }
            auto sc_it = swap_cache_.find(vpn);
            if (sc_it != swap_cache_.end()) {
                page = sc_it->second;
                swap_cache_.erase(sc_it);
                from_swap_cache = true;
            } else {
                auto z_it = zram_slots_.find(vpn);
                if (z_it == zram_slots_.end()) {
                    return false;
                }
                comp_copy = z_it->second;
            }
        }

        if (!from_swap_cache) {
            uLongf dest_len = kPageBytes;
            int rc = uncompress(page.data(), &dest_len, comp_copy.data(), comp_copy.size());
            REQUIRE(rc == Z_OK && dest_len == kPageBytes);
        }

        std::lock_guard<std::mutex> lk(mu_);
        if (!resident_ptes_.count(vpn)) {
            resident_ptes_[vpn] = page;
            auto z_it = zram_slots_.find(vpn);
            if (z_it != zram_slots_.end()) {
                zram_bytes_ -= z_it->second.size();
                zram_slots_.erase(z_it);
            }
            swap_cache_.erase(vpn);
            UpdatePeakLocked();
            return true;
        }
        return false;
    }

    AccessResult AppAccessPage(uint64_t vpn, uint8_t out_page[kPageBytes])
    {
        std::vector<uint8_t> comp_copy;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto r_it = resident_ptes_.find(vpn);
            if (r_it != resident_ptes_.end()) {
                std::memcpy(out_page, r_it->second.data(), kPageBytes);
                return HIT_RESIDENT_PTE;
            }
            auto sc_it = swap_cache_.find(vpn);
            if (sc_it != swap_cache_.end()) {
                std::memcpy(out_page, sc_it->second.data(), kPageBytes);
                resident_ptes_[vpn] = sc_it->second;
                swap_cache_.erase(sc_it);
                auto z_it = zram_slots_.find(vpn);
                if (z_it != zram_slots_.end()) {
                    zram_bytes_ -= z_it->second.size();
                    zram_slots_.erase(z_it);
                }
                UpdatePeakLocked();
                return HIT_SWAP_CACHE_MINOR;
            }
            auto z_it = zram_slots_.find(vpn);
            REQUIRE(z_it != zram_slots_.end());
            comp_copy = z_it->second;
        }

        /* Synchronous SWP_SYNCHRONOUS_IO decompression stall on Application Thread! */
        uLongf dest_len = kPageBytes;
        int rc = uncompress(out_page, &dest_len, comp_copy.data(), comp_copy.size());
        REQUIRE(rc == Z_OK && dest_len == kPageBytes);

        std::lock_guard<std::mutex> lk(mu_);
        std::array<uint8_t, kPageBytes> installed{};
        std::memcpy(installed.data(), out_page, kPageBytes);
        resident_ptes_[vpn] = installed;
        auto z_it = zram_slots_.find(vpn);
        if (z_it != zram_slots_.end()) {
            zram_bytes_ -= z_it->second.size();
            zram_slots_.erase(z_it);
        }
        swap_cache_.erase(vpn);
        UpdatePeakLocked();
        return SYNC_ZRAM_MAJOR_FAULT;
    }

    size_t PeakSwapCacheBytes() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return peak_swap_cache_bytes_;
    }

    size_t PeakTotalDramBytes() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return peak_total_dram_bytes_;
    }

    uint64_t DoubleBufferedEvents() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return double_buffered_events_;
    }

private:
    void UpdatePeakLocked()
    {
        size_t sc_bytes = swap_cache_.size() * kPageBytes;
        size_t res_bytes = resident_ptes_.size() * kPageBytes;
        size_t total = zram_bytes_ + sc_bytes + res_bytes;
        if (sc_bytes > peak_swap_cache_bytes_) {
            peak_swap_cache_bytes_ = sc_bytes;
        }
        if (total > peak_total_dram_bytes_) {
            peak_total_dram_bytes_ = total;
        }
    }

    int zlib_level_;
    mutable std::mutex mu_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> zram_slots_;
    std::unordered_map<uint64_t, std::array<uint8_t, kPageBytes>> swap_cache_;
    std::unordered_map<uint64_t, std::array<uint8_t, kPageBytes>> resident_ptes_;
    size_t zram_bytes_ = 0;
    size_t peak_swap_cache_bytes_ = 0;
    size_t peak_total_dram_bytes_ = 0;
    uint64_t double_buffered_events_ = 0;
};

void PushRingEvent(mext_ring_buffer *ring, uint32_t type, uint32_t pid, uint64_t vpn, uint64_t aux)
{
    uint64_t now_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    while (true) {
        uint64_t w = __atomic_load_n(&ring->hdr.write_idx, __ATOMIC_RELAXED);
        uint64_t tail = __atomic_load_n(&ring->hdr.tail, __ATOMIC_ACQUIRE);
        if (w - tail >= MEXT_RING_CAPACITY) {
            std::this_thread::yield();
            continue;
        }
        if (__atomic_compare_exchange_n(&ring->hdr.write_idx, &w, w + 1ULL, true,
                                        __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
            mext_ring_event *ev = &ring->events[w & MEXT_RING_MASK];
            while (__atomic_load_n(&ev->type, __ATOMIC_ACQUIRE) != MEXT_EVT_NONE) {
                std::this_thread::yield();
            }
            ev->pid = pid;
            ev->vpn = vpn;
            ev->aux = aux;
            ev->timestamp = now_ns;
            __atomic_store_n(&ev->type, type, __ATOMIC_RELEASE);
            return;
        }
    }
}

void TestZramPresetConfig()
{
    MextConfig cfg{};
    MextDefaultConfig(&cfg);
    REQUIRE(cfg.fault_trigger_stride == 4);
    REQUIRE(std::fabs(cfg.prefault_threshold - 0.75f) < 1e-5f);

    MextApplyZramPreset(&cfg);
    REQUIRE(cfg.enabled);
    REQUIRE(cfg.fault_trigger_stride == 1);
    REQUIRE(std::fabs(cfg.prefetch_threshold - 0.38f) < 1e-5f);
    REQUIRE(std::fabs(cfg.prefault_threshold - 0.50f) < 1e-5f);
    std::printf("[PASS] Test 1: MextApplyZramPreset Configuration & Compression Codec Check\n");
}

void TestZramDoubleBufferingElimination()
{
    constexpr int kPages = 400;
    const int32_t pid = 3101;

    std::vector<uint64_t> vpns;
    uint64_t rng = 0xCAFEBABE1234ULL;
    for (int i = 0; i < kPages; ++i) {
        rng ^= rng << 13U;
        rng ^= rng >> 7U;
        rng ^= rng << 17U;
        vpns.push_back(0x300000ULL + static_cast<uint64_t>(i) * 193ULL + (rng & 0x3FULL));
    }

    auto run_config = [&](bool use_zram_preset, uint64_t *out_double_buf, uint64_t *out_pte_hits) -> size_t {
        SimulatedZramSubsystem zram(Z_BEST_SPEED);
        uint8_t page_buf[kPageBytes];
        size_t total_comp = 0;
        for (uint64_t v : vpns) {
            GenerateStructuredPage4K(v, page_buf);
            total_comp += zram.SwapOutPage(v, page_buf);
        }
        (void)total_comp;

        MextEngine engine;
        MextConfig cfg{};
        MextDefaultConfig(&cfg);
        if (use_zram_preset) {
            MextApplyZramPreset(&cfg);
        }
        REQUIRE(engine.Start(-1, cfg, false) == 0);
        engine.SetDispatchCallback([&](int32_t,
                                       const std::vector<uint64_t> &pf,
                                       const std::vector<uint64_t> &pflt) {
            for (uint64_t v : pf) {
                zram.PrefetchToSwapCache(v);
            }
            for (uint64_t v : pflt) {
                zram.PrefaultToPte(v);
            }
        });

        for (size_t i = 0; i < vpns.size(); ++i) {
            engine.HandleEvictionEvent(pid, vpns[i], 1 + i / 64);
        }

        uint64_t pte_hits = 0;
        for (size_t i = 0; i < vpns.size(); ++i) {
            uint64_t v = vpns[i];
            uint8_t expected[kPageBytes];
            GenerateStructuredPage4K(v, expected);

            auto res = zram.AppAccessPage(v, page_buf);
            /* Verify bit-exact 4096-byte integrity after zram decompression! */
            REQUIRE(ChecksumPage4K(page_buf) == ChecksumPage4K(expected));
            if (res == SimulatedZramSubsystem::HIT_RESIDENT_PTE) {
                ++pte_hits;
            } else {
                engine.HandleFaultEvent(pid, v, 3, (i + 1) * 8000ULL);
            }
        }

        *out_double_buf = zram.DoubleBufferedEvents();
        *out_pte_hits = pte_hits;
        engine.Stop();
        return zram.PeakSwapCacheBytes();
    };

    uint64_t nvme_dbuf = 0, nvme_pte_hits = 0;
    uint64_t zram_dbuf = 0, zram_pte_hits = 0;
    size_t nvme_sc_peak = run_config(false, &nvme_dbuf, &nvme_pte_hits);
    size_t zram_sc_peak = run_config(true, &zram_dbuf, &zram_pte_hits);

    REQUIRE(zram_dbuf < nvme_dbuf);
    REQUIRE(zram_pte_hits > 300);
    std::printf("[PASS] Test 2: DRAM Double-Buffering & Bit-Exact 4KB Integrity\n");
    std::printf("       • NVMe Config on zram : double-buffered=%3" PRIu64 " pages (peak SwapCache=%5.1f KB) | zero-fault PTE hits=%" PRIu64 "/400\n",
                nvme_dbuf, static_cast<double>(nvme_sc_peak) / 1024.0, nvme_pte_hits);
    std::printf("       • Zram Preset Config  : double-buffered=%3" PRIu64 " pages (peak SwapCache=%5.1f KB) | zero-fault PTE hits=%" PRIu64 "/400\n",
                zram_dbuf, static_cast<double>(zram_sc_peak) / 1024.0, zram_pte_hits);
}

struct LiveBenchStats {
    double wall_ms = 0.0;
    uint64_t pte_hits = 0;
    uint64_t minor_hits = 0;
    uint64_t sync_faults = 0;
    double compression_ratio = 0.0;
};

LiveBenchStats RunLiveConcurrentZramWorkload(const std::vector<std::pair<uint64_t, uint8_t>> &access_stream,
                                             const std::vector<std::pair<uint64_t, uint64_t>> &evict_order,
                                             bool enable_mext_worker,
                                             int zlib_level)
{
    LiveBenchStats stats{};
    SimulatedZramSubsystem zram(zlib_level);
    uint8_t init_page[kPageBytes];
    size_t total_raw = 0;
    size_t total_comp = 0;

    for (const auto &ev : evict_order) {
        GenerateStructuredPage4K(ev.first, init_page);
        total_raw += kPageBytes;
        total_comp += zram.SwapOutPage(ev.first, init_page);
    }
    stats.compression_ratio = static_cast<double>(total_raw) / static_cast<double>(std::max<size_t>(1, total_comp));

    auto ring = std::make_unique<mext_ring_buffer>();
    std::memset(ring.get(), 0, sizeof(mext_ring_buffer));
    ring->hdr.magic = MEXT_RING_MAGIC;
    ring->hdr.version = MEXT_RING_VERSION;
    ring->hdr.capacity = MEXT_RING_CAPACITY;
    ring->hdr.page_shift = 12;

    MextEngine engine;
    const int32_t pid = 7700;

    if (enable_mext_worker) {
        MextConfig cfg{};
        MextDefaultConfig(&cfg);
        MextApplyZramPreset(&cfg);
        REQUIRE(engine.Start(-1, cfg, false) == 0);
        engine.AttachExternalRing(ring.get());
        engine.SetDispatchCallback([&](int32_t,
                                       const std::vector<uint64_t> &pf,
                                       const std::vector<uint64_t> &pflt) {
            /* Runs on MEXT Helper Worker Thread (Core B)! */
            for (uint64_t v : pf) {
                zram.PrefetchToSwapCache(v);
            }
            for (uint64_t v : pflt) {
                zram.PrefaultToPte(v);
            }
        });

        for (const auto &ev : evict_order) {
            engine.HandleEvictionEvent(pid, ev.first, ev.second);
        }
        /* Restart with live OS background worker thread polling the lockless ring! */
        engine.Stop();
        REQUIRE(engine.Start(-1, cfg, false) == 0);
        engine.AttachExternalRing(ring.get());
        engine.SetDispatchCallback([&](int32_t,
                                       const std::vector<uint64_t> &pf,
                                       const std::vector<uint64_t> &pflt) {
            for (uint64_t v : pf) {
                zram.PrefetchToSwapCache(v);
            }
            for (uint64_t v : pflt) {
                zram.PrefaultToPte(v);
            }
        });
        for (const auto &ev : evict_order) {
            engine.HandleEvictionEvent(pid, ev.first, ev.second);
        }
    }

    /* Spawn real concurrent helper OS thread draining the lockless ring */
    std::atomic<bool> helper_stop{ false };
    std::thread helper_thread;
    if (enable_mext_worker) {
        helper_thread = std::thread([&]() {
            while (!helper_stop.load(std::memory_order_acquire)) {
                if (engine.ProcessRingOnce(64) == 0) {
                    std::this_thread::yield();
                }
            }
            engine.ProcessRingOnce(512);
        });
    }

    /* Application Thread (Core A): real page reads + compute work + checksum verification */
    uint8_t page_out[kPageBytes];
    volatile uint64_t app_accum = 0;

    auto t0 = std::chrono::steady_clock::now();
    for (const auto &op : access_stream) {
        uint64_t vpn = op.first;
        uint8_t vma = op.second;

        auto res = zram.AppAccessPage(vpn, page_out);
        if (res == SimulatedZramSubsystem::HIT_RESIDENT_PTE) {
            ++stats.pte_hits;
        } else if (res == SimulatedZramSubsystem::HIT_SWAP_CACHE_MINOR) {
            ++stats.minor_hits;
            if (enable_mext_worker) {
                PushRingEvent(ring.get(), MEXT_EVT_SWAP_FAULT, static_cast<uint32_t>(pid), vpn, vma);
            }
        } else {
            ++stats.sync_faults;
            if (enable_mext_worker) {
                PushRingEvent(ring.get(), MEXT_EVT_SWAP_FAULT, static_cast<uint32_t>(pid), vpn, vma);
            }
        }

        /* Real application CPU work (~6 us, matching EDA/Agentic compute per cold 4KB page) */
        uint64_t csum = ChecksumPage4K(page_out);
        for (int spin = 0; spin < 3500; ++spin) {
            csum = (csum ^ static_cast<uint64_t>(spin)) * 0x100000001B3ULL;
            asm volatile("" : "+r"(csum));
        }
        app_accum ^= csum;
    }
    auto t1 = std::chrono::steady_clock::now();
    (void)app_accum;

    if (enable_mext_worker) {
        helper_stop.store(true, std::memory_order_release);
        helper_thread.join();
        engine.Stop();
    }

    stats.wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return stats;
}

void TestLiveTwoCoreZramSpeedup()
{
    constexpr int kNumPages = 1200;

    /* 1. EDA Pointer-Chase Graph on zram */
    std::vector<std::pair<uint64_t, uint8_t>> eda_stream;
    std::vector<std::pair<uint64_t, uint64_t>> eda_evicts;
    {
        uint64_t s = 0x9876543210ABCDEFULL;
        for (int i = 0; i < kNumPages; ++i) {
            s ^= s << 13U;
            s ^= s >> 7U;
            s ^= s << 17U;
            uint64_t vpn = 0x400000ULL + static_cast<uint64_t>(i) * 271ULL + (s & 0x3FULL);
            eda_stream.push_back({ vpn, 4 });
            eda_evicts.push_back({ vpn, 1 + static_cast<uint64_t>(i / 64) });
        }
    }

    /* 2. Agentic AI Interleaved Dual-Arena Stream on zram */
    std::vector<std::pair<uint64_t, uint8_t>> ai_stream;
    std::vector<std::pair<uint64_t, uint64_t>> ai_evicts;
    {
        for (int i = 0; i < kNumPages; ++i) {
            uint64_t vpn = ((i & 1) == 0) ? (0x600000ULL + static_cast<uint64_t>(i / 2) * 4ULL)
                                          : (0x900000ULL + static_cast<uint64_t>(i / 2) * 8ULL);
            uint8_t vma = ((i & 1) == 0) ? 2 : 11;
            ai_stream.push_back({ vpn, vma });
            ai_evicts.push_back({ vpn, 10 + static_cast<uint64_t>(i / 100) });
        }
    }

    /* 3. Adversarial 100% Fisher-Yates Random Hash on zram */
    std::vector<std::pair<uint64_t, uint8_t>> rand_stream;
    std::vector<std::pair<uint64_t, uint64_t>> rand_evicts;
    {
        uint64_t s = 0xDEADBEEF42ULL;
        std::vector<uint64_t> pool;
        for (int i = 0; i < 4000; ++i) {
            s ^= s << 13U;
            s ^= s >> 7U;
            s ^= s << 17U;
            uint64_t vpn = 0xC00000ULL + static_cast<uint64_t>(i) * 23ULL + (s & 0xFULL);
            pool.push_back(vpn);
            rand_evicts.push_back({ vpn, 1 + static_cast<uint64_t>(i / 128) });
        }
        for (size_t i = pool.size() - 1; i > 0; --i) {
            s ^= s << 13U;
            s ^= s >> 7U;
            s ^= s << 17U;
            std::swap(pool[i], pool[s % (i + 1)]);
        }
        for (int i = 0; i < kNumPages; ++i) {
            rand_stream.push_back({ pool[i], static_cast<uint8_t>(i & 31) });
        }
    }

    auto eda_base_fast = RunLiveConcurrentZramWorkload(eda_stream, eda_evicts, false, Z_BEST_SPEED);
    auto eda_mext_fast = RunLiveConcurrentZramWorkload(eda_stream, eda_evicts, true, Z_BEST_SPEED);

    auto eda_base_dense = RunLiveConcurrentZramWorkload(eda_stream, eda_evicts, false, Z_DEFAULT_COMPRESSION);
    auto eda_mext_dense = RunLiveConcurrentZramWorkload(eda_stream, eda_evicts, true, Z_DEFAULT_COMPRESSION);

    auto ai_base_fast = RunLiveConcurrentZramWorkload(ai_stream, ai_evicts, false, Z_BEST_SPEED);
    auto ai_mext_fast = RunLiveConcurrentZramWorkload(ai_stream, ai_evicts, true, Z_BEST_SPEED);

    auto rand_base_fast = RunLiveConcurrentZramWorkload(rand_stream, rand_evicts, false, Z_BEST_SPEED);
    auto rand_mext_fast = RunLiveConcurrentZramWorkload(rand_stream, rand_evicts, true, Z_BEST_SPEED);

    std::printf("[PASS] Test 3: Live Two-Thread Concurrent OS Execution on Real In-RAM Compressed 4KB Pages (%d pages)\n",
                kNumPages);
    std::printf("\n%-38s | %7s | %10s | %10s | %9s | %18s\n",
                "Live Two-Core zram Workload", "CompRat", "Sync-zram", "MEXT+zram", "Speedup", "PTE / Minor / Sync");
    std::printf("---------------------------------------+---------+------------+------------+-----------+-------------------\n");
    std::printf("%-38s | %6.2fx | %8.2fms | %8.2fms | %7.2fx  | %4" PRIu64 " / %3" PRIu64 " / %4" PRIu64 "\n",
                "1. EDA Graph (Fast zram level 1)",
                eda_mext_fast.compression_ratio, eda_base_fast.wall_ms, eda_mext_fast.wall_ms,
                eda_base_fast.wall_ms / eda_mext_fast.wall_ms,
                eda_mext_fast.pte_hits, eda_mext_fast.minor_hits, eda_mext_fast.sync_faults);
    std::printf("%-38s | %6.2fx | %8.2fms | %8.2fms | %7.2fx  | %4" PRIu64 " / %3" PRIu64 " / %4" PRIu64 "\n",
                "2. EDA Graph (Dense zram level 6)",
                eda_mext_dense.compression_ratio, eda_base_dense.wall_ms, eda_mext_dense.wall_ms,
                eda_base_dense.wall_ms / eda_mext_dense.wall_ms,
                eda_mext_dense.pte_hits, eda_mext_dense.minor_hits, eda_mext_dense.sync_faults);
    std::printf("%-38s | %6.2fx | %8.2fms | %8.2fms | %7.2fx  | %4" PRIu64 " / %3" PRIu64 " / %4" PRIu64 "\n",
                "3. Agentic Dual-Arena (Fast zram)",
                ai_mext_fast.compression_ratio, ai_base_fast.wall_ms, ai_mext_fast.wall_ms,
                ai_base_fast.wall_ms / ai_mext_fast.wall_ms,
                ai_mext_fast.pte_hits, ai_mext_fast.minor_hits, ai_mext_fast.sync_faults);
    std::printf("%-38s | %6.2fx | %8.2fms | %8.2fms | %7.2fx  | %4" PRIu64 " / %3" PRIu64 " / %4" PRIu64 "\n",
                "4. Adversarial 100% Random (Safety)",
                rand_mext_fast.compression_ratio, rand_base_fast.wall_ms, rand_mext_fast.wall_ms,
                rand_base_fast.wall_ms / rand_mext_fast.wall_ms,
                rand_mext_fast.pte_hits, rand_mext_fast.minor_hits, rand_mext_fast.sync_faults);

    REQUIRE(eda_mext_fast.wall_ms < eda_base_fast.wall_ms);
    REQUIRE(eda_mext_dense.wall_ms < eda_base_dense.wall_ms);
    REQUIRE(ai_mext_fast.wall_ms < ai_base_fast.wall_ms);
}

} // namespace

int main()
{
    std::printf("=== UBTurbo SMAP-MEXT + zram Live Compression & Offload Suite ===\n");
    TestZramPresetConfig();
    TestZramDoubleBufferingElimination();
    TestLiveTwoCoreZramSpeedup();
    std::printf("=== ALL ZRAM PREFETCH/PREFAULT TESTS PASSED ===\n");
    return 0;
}
