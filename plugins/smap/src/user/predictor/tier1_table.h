/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Sub-microsecond Tier-1 Heuristic Predictor (Per-VMA Stride,
 *              Markov Delta Table, and Co-Eviction Epoch Run-Length Detector).
 */
#ifndef SMAP_USER_PREDICTOR_TIER1_TABLE_H
#define SMAP_USER_PREDICTOR_TIER1_TABLE_H

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "mext_tokenizer.h"
#include "swap_shadow_map.h"

namespace smap {
namespace mext {

struct CandidatePage {
    uint64_t vpn = 0;
    float prob = 0.0f;
};

class Tier1Table {
public:
    Tier1Table() = default;

    /*
     * Update state on a fault event and append fast Tier-1 candidate pages.
     */
    void ObserveAndPredict(int32_t pid,
                           uint64_t vpn,
                           uint8_t vma_slot,
                           bool has_evict,
                           uint64_t evict_seq,
                           uint64_t evict_epoch,
                           const SwapShadowMap &shadow,
                           std::vector<CandidatePage> *out_candidates,
                           uint32_t kernel_ra_pages = 0U);

    void RemovePid(int32_t pid);
    void Clear();

private:
    struct VmaStreamState {
        uint64_t last_vpn = 0;
        int64_t last_stride = 0;
        uint32_t streak = 0;
        uint32_t seq_window = 0;
        bool initialized = false;
    };

    struct MarkovEntry {
        int32_t offset[2] = { 0, 0 };
        uint16_t count[2] = { 0, 0 };
        uint16_t total = 0;
    };

    struct PidTier1State {
        VmaStreamState vma_streams[MEXT_VMA_VOCAB];
        MarkovEntry markov[MEXT_EXACT_COUNT];
        uint64_t prev_vpn = 0;
        int32_t prev_exact_dvpn = 0;
        bool has_prev_vpn = false;
        bool has_prev_exact = false;

        uint64_t last_evict_seq = 0;
        uint64_t last_evict_epoch = 0;
        uint32_t epoch_burst = 0;
    };

    static void UpdateMarkov(MarkovEntry *entry, int32_t next_offset);

    std::unordered_map<int32_t, PidTier1State> pids_;
};

} // namespace mext
} // namespace smap

#endif /* SMAP_USER_PREDICTOR_TIER1_TABLE_H */
