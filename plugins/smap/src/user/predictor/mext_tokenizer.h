/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Multi-space delta tokenizer for MEXT Predictive Memory
 *              (VPN delta + Swap-Eviction-Order delta + VMA + inter-fault dt)
 */
#ifndef SMAP_USER_PREDICTOR_MEXT_TOKENIZER_H
#define SMAP_USER_PREDICTOR_MEXT_TOKENIZER_H

#include <cstdint>

namespace smap {
namespace mext {

constexpr int32_t MEXT_MAX_EXACT_DELTA = 64;
constexpr uint32_t MEXT_EXACT_COUNT = 2 * MEXT_MAX_EXACT_DELTA + 1; /* 129: [-64..+64] */
constexpr uint32_t MEXT_LOG_BUCKETS = 32;                           /* 32 neg + 32 pos */
constexpr uint32_t MEXT_TOKEN_UNKNOWN = MEXT_EXACT_COUNT + 2 * MEXT_LOG_BUCKETS; /* 193 */
constexpr uint32_t MEXT_DELTA_VOCAB = MEXT_TOKEN_UNKNOWN + 1;       /* 194 */

constexpr uint32_t MEXT_VMA_VOCAB = 32;
constexpr uint32_t MEXT_DT_VOCAB = 16;

/* Actionable non-zero offsets: 128 for dVPN [-64..-1, +1..+64] + 128 for dEvictSeq */
constexpr uint32_t MEXT_ACTION_OFFSETS = 128;
constexpr uint32_t MEXT_OUT_DIM = 2 * MEXT_ACTION_OFFSETS; /* 256 */

inline uint32_t FloorLog2U64(uint64_t v)
{
    if (v <= 1) {
        return 0;
    }
#if defined(__GNUC__) || defined(__clang__)
    return 63U - static_cast<uint32_t>(__builtin_clzll(v));
#else
    uint32_t r = 0;
    while (v >>= 1) {
        ++r;
    }
    return r;
#endif
}

inline uint8_t EncodeDeltaToken(int64_t delta, bool valid)
{
    if (!valid) {
        return static_cast<uint8_t>(MEXT_TOKEN_UNKNOWN);
    }
    if (delta >= -MEXT_MAX_EXACT_DELTA && delta <= MEXT_MAX_EXACT_DELTA) {
        return static_cast<uint8_t>(delta + MEXT_MAX_EXACT_DELTA);
    }
    uint64_t mag = (delta < 0) ? static_cast<uint64_t>(-(delta + 1)) + 1ULL
                               : static_cast<uint64_t>(delta);
    uint32_t lg = FloorLog2U64(mag);
    uint32_t bucket = (lg > 6U) ? (lg - 6U) : 0U;
    if (bucket >= MEXT_LOG_BUCKETS) {
        bucket = MEXT_LOG_BUCKETS - 1U;
    }
    uint32_t base = MEXT_EXACT_COUNT + ((delta < 0) ? 0U : MEXT_LOG_BUCKETS);
    return static_cast<uint8_t>(base + bucket);
}

inline uint8_t EncodeDtToken(uint64_t dt_ns)
{
    uint64_t us = dt_ns / 1000ULL;
    if (us == 0) {
        return 0;
    }
    uint32_t b = FloorLog2U64(us) + 1U;
    return static_cast<uint8_t>(b >= MEXT_DT_VOCAB ? (MEXT_DT_VOCAB - 1U) : b);
}

/*
 * Map index k in [0..127] <-> non-zero offset in [-64..-1, +1..+64].
 */
inline int32_t ActionIndexToOffset(uint32_t idx)
{
    if (idx < static_cast<uint32_t>(MEXT_MAX_EXACT_DELTA)) {
        return static_cast<int32_t>(idx) - MEXT_MAX_EXACT_DELTA; /* -64 .. -1 */
    }
    return static_cast<int32_t>(idx - MEXT_MAX_EXACT_DELTA) + 1; /* +1 .. +64 */
}

inline int32_t OffsetToActionIndex(int64_t delta)
{
    if (delta == 0 || delta < -MEXT_MAX_EXACT_DELTA || delta > MEXT_MAX_EXACT_DELTA) {
        return -1;
    }
    if (delta < 0) {
        return static_cast<int32_t>(delta + MEXT_MAX_EXACT_DELTA);
    }
    return static_cast<int32_t>(delta - 1 + MEXT_MAX_EXACT_DELTA);
}

} // namespace mext
} // namespace smap

#endif /* SMAP_USER_PREDICTOR_MEXT_TOKENIZER_H */
