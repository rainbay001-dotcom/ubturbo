/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Swap Shadow Map tracking per-PID eviction sequence & co-eviction
 *              epochs so random VA pointer graphs collapse to small eviction deltas.
 */
#ifndef SMAP_USER_PREDICTOR_SWAP_SHADOW_MAP_H
#define SMAP_USER_PREDICTOR_SWAP_SHADOW_MAP_H

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace smap {
namespace mext {

constexpr uint32_t MEXT_SHADOW_LOG_INIT_CAP = 1024U; /* lazy 32KB start, grows to 64K */
constexpr uint32_t MEXT_SHADOW_LOG_BITS = 16U;
constexpr uint32_t MEXT_SHADOW_LOG_CAP = (1U << MEXT_SHADOW_LOG_BITS); /* 65536 */
constexpr uint32_t MEXT_SHADOW_LOG_MASK = MEXT_SHADOW_LOG_CAP - 1U;

constexpr uint32_t MEXT_PREFETCH_FILTER_BITS = 10U;
constexpr uint32_t MEXT_PREFETCH_FILTER_CAP = (1U << MEXT_PREFETCH_FILTER_BITS); /* 1024 */
constexpr uint32_t MEXT_PREFETCH_FILTER_MASK = MEXT_PREFETCH_FILTER_CAP - 1U;

struct EvictLogEntry {
    uint64_t vpn = 0;
    uint64_t epoch = 0;
    uint64_t seq = 0;
    bool active = false;
};

/*
 * Fast open-addressed power-of-2 hash table (vpn -> evict_seq) with zero
 * per-operation heap allocation on the steady-state critical path.
 */
class FlatVpnHashTable {
public:
    FlatVpnHashTable();
    explicit FlatVpnHashTable(uint32_t cap_pow2);

    void Upsert(uint64_t vpn, uint64_t seq);
    bool Lookup(uint64_t vpn, uint64_t *seq_out) const;
    bool Erase(uint64_t vpn, uint64_t *seq_out = nullptr);
    size_t Size() const { return size_; }
    void Clear();

private:
    enum SlotState : uint8_t { EMPTY = 0, OCCUPIED = 1, TOMBSTONE = 2 };
    struct Slot {
        uint64_t key = 0;
        uint64_t val = 0;
        uint8_t state = EMPTY;
    };

    void Rehash(uint32_t new_cap);
    static uint64_t Hash(uint64_t x)
    {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        return x;
    }

    std::vector<Slot> slots_;
    uint32_t mask_ = 0;
    size_t size_ = 0;
    size_t used_ = 0; /* OCCUPIED + TOMBSTONE */
};

class SwapShadowMap {
public:
    SwapShadowMap() = default;

    uint64_t RecordEviction(int32_t pid, uint64_t vpn, uint64_t epoch);

    /*
     * Called when a swap fault occurs on (pid, vpn). Returns true if the page
     * was tracked in the shadow map, writing its eviction sequence & epoch and
     * marking it resident (no longer on swap).
     */
    bool OnPageFault(int32_t pid, uint64_t vpn, uint64_t *evict_seq_out, uint64_t *epoch_out);

    /*
     * Resolve `base_seq + delta` to a VPN that is STILL swapped out (active)
     * and has not already been prefetched recently.
     */
    bool ResolveEvictNeighbor(int32_t pid, uint64_t base_seq, int32_t delta,
                              uint64_t *vpn_out, uint64_t *epoch_out = nullptr) const;

    bool IsSwappedOut(int32_t pid, uint64_t vpn) const;
    bool WasRecentlyPrefetched(int32_t pid, uint64_t vpn) const;
    void MarkPrefetched(int32_t pid, uint64_t vpn);

    void RemovePid(int32_t pid);
    void Clear();
    uint64_t TotalActivePages() const;
    uint64_t TotalEvictionsForPid(int32_t pid) const;

private:
    struct PidShadow {
        std::vector<EvictLogEntry> log;
        uint32_t log_mask = 0;
        FlatVpnHashTable vpn_to_seq;
        uint64_t next_seq = 1;
        uint64_t recent_prefetch[MEXT_PREFETCH_FILTER_CAP] = { 0 };

        PidShadow() : vpn_to_seq(256) {}
    };

    static void EnsureLogCapacity(PidShadow *ps);

    std::unordered_map<int32_t, PidShadow> pids_;
};

} // namespace mext
} // namespace smap

#endif /* SMAP_USER_PREDICTOR_SWAP_SHADOW_MAP_H */
