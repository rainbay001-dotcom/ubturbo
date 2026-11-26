/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 */

#include "tier1_table.h"

#include <algorithm>
#include <cstdlib>

namespace smap {
namespace mext {

void Tier1Table::UpdateMarkov(MarkovEntry *e, int32_t next_offset)
{
    if (next_offset == 0) {
        return;
    }
    /* Periodic halving decay to adapt to phase changes */
    if (e->total >= 512) {
        e->count[0] >>= 1U;
        e->count[1] >>= 1U;
        e->total = static_cast<uint16_t>((e->total >> 1U) + 1U);
    } else {
        ++e->total;
    }

    for (int i = 0; i < 2; ++i) {
        if (e->count[i] > 0 && e->offset[i] == next_offset) {
            ++e->count[i];
            if (i == 1 && e->count[1] > e->count[0]) {
                std::swap(e->count[0], e->count[1]);
                std::swap(e->offset[0], e->offset[1]);
            }
            return;
        }
    }

    for (int i = 0; i < 2; ++i) {
        if (e->count[i] == 0) {
            e->offset[i] = next_offset;
            e->count[i] = 1;
            return;
        }
    }

    /* Space-Saving replacement of weaker slot */
    if (e->count[1] <= 1) {
        e->offset[1] = next_offset;
        e->count[1] = 1;
    } else {
        --e->count[1];
    }
}

void Tier1Table::ObserveAndPredict(int32_t pid,
                                   uint64_t vpn,
                                   uint8_t vma_slot,
                                   bool has_evict,
                                   uint64_t evict_seq,
                                   uint64_t evict_epoch,
                                   const SwapShadowMap &shadow,
                                   std::vector<CandidatePage> *out_candidates)
{
    if (pid <= 0 || vpn == 0) {
        return;
    }

    PidTier1State &st = pids_[pid];
    uint8_t vma_idx = vma_slot & (MEXT_VMA_VOCAB - 1U);

    /* 1. Per-VMA / GPA-Region Stream & Prefetch-Frontier Detector */
    VmaStreamState &vs = st.vma_streams[vma_idx];
    if (!vs.initialized) {
        /*
         * Do not blind-spray 4 GPA pages on the very first fault of a region:
         * in Firecracker KVM VMs, Guest Physical Addresses (GPA) are non-linear
         * (~19% unit-stride locality) and full cgroups penalize false prefetches.
         */
        vs.seq_window = 0U;
    } else if (vpn != vs.last_vpn) {
        int64_t stride = static_cast<int64_t>(vpn) - static_cast<int64_t>(vs.last_vpn);
        uint64_t fwd_gap = (stride > 0) ? static_cast<uint64_t>(stride) : 0ULL;
        bool seq_frontier_hit =
            (fwd_gap == 1ULL) ||
            (vs.seq_window >= 2U && vs.streak >= 2U && fwd_gap >= 2ULL &&
             fwd_gap <= static_cast<uint64_t>(vs.seq_window + 1U) &&
             shadow.WasRecentlyPrefetched(pid, vpn - 1ULL));

        if (seq_frontier_hit) {
            vs.last_stride = 1;
            if (vs.streak < 64U) {
                ++vs.streak;
            }
            uint32_t next_win = (vs.streak == 1U)
                                    ? 2U
                                    : std::min<uint32_t>(8U, std::max<uint32_t>(4U, vs.seq_window * 2U));
            vs.seq_window = next_win;
            if (out_candidates) {
                float base_conf = std::min(0.90f, 0.48f + 0.08f * static_cast<float>(vs.streak));
                for (uint32_t step = 1U; step <= next_win; ++step) {
                    float conf = std::max(0.30f, base_conf * (1.0f - 0.06f * static_cast<float>(step - 1U)));
                    out_candidates->push_back(CandidatePage{ vpn + step, conf });
                }
            }
        } else {
            vs.seq_window = 0U;
            if (stride == vs.last_stride && std::llabs(stride) <= 256) {
                if (vs.streak < 64U) {
                    ++vs.streak;
                }
            } else {
                vs.last_stride = stride;
                vs.streak = 1U;
            }

            if (out_candidates && vs.streak >= 2U) {
                uint32_t depth = std::min<uint32_t>(8U, 2U + vs.streak);
                float base_conf = std::min(0.90f, 0.48f + 0.08f * static_cast<float>(vs.streak));
                for (uint32_t step = 1U; step <= depth; ++step) {
                    int64_t target = static_cast<int64_t>(vpn) + vs.last_stride * static_cast<int64_t>(step);
                    if (target > 0) {
                        float conf = base_conf * (1.0f - 0.08f * static_cast<float>(step - 1U));
                        out_candidates->push_back(CandidatePage{ static_cast<uint64_t>(target), conf });
                    }
                }
            }
        }
    }
    vs.last_vpn = vpn;
    vs.initialized = true;

    /* 2. First-Order Markov Delta Transition Table */
    if (st.has_prev_vpn) {
        int64_t dvpn = static_cast<int64_t>(vpn) - static_cast<int64_t>(st.prev_vpn);
        bool cur_exact = (dvpn != 0 && dvpn >= -MEXT_MAX_EXACT_DELTA && dvpn <= MEXT_MAX_EXACT_DELTA);
        if (st.has_prev_exact && cur_exact) {
            uint32_t prev_slot = static_cast<uint32_t>(st.prev_exact_dvpn + MEXT_MAX_EXACT_DELTA);
            UpdateMarkov(&st.markov[prev_slot], static_cast<int32_t>(dvpn));
        }

        if (cur_exact) {
            uint32_t cur_slot = static_cast<uint32_t>(dvpn + MEXT_MAX_EXACT_DELTA);
            const MarkovEntry &me = st.markov[cur_slot];
            if (out_candidates && me.total >= 3) {
                for (int i = 0; i < 2; ++i) {
                    if (me.count[i] >= 2 && me.offset[i] != 0) {
                        float share = static_cast<float>(me.count[i]) / static_cast<float>(me.total);
                        if (share >= 0.25f) {
                            int64_t target = static_cast<int64_t>(vpn) + me.offset[i];
                            if (target > 0) {
                                out_candidates->push_back(
                                    CandidatePage{ static_cast<uint64_t>(target), std::min(0.82f, share * 0.90f) });
                            }
                        }
                    }
                }
            }
            st.prev_exact_dvpn = static_cast<int32_t>(dvpn);
            st.has_prev_exact = true;
        } else {
            st.has_prev_exact = false;
        }
    }
    st.prev_vpn = vpn;
    st.has_prev_vpn = true;

    /* 3. Co-Eviction Epoch Run-Length Detector (US 2026/0154153 A1) */
    if (has_evict && evict_seq > 0) {
        int64_t dseq = static_cast<int64_t>(evict_seq) - static_cast<int64_t>(st.last_evict_seq);
        if (st.last_evict_seq > 0 &&
            (evict_epoch == st.last_evict_epoch || std::llabs(dseq) <= 16)) {
            if (st.epoch_burst < 32) {
                ++st.epoch_burst;
            }
        } else {
            st.epoch_burst = 1;
        }
        st.last_evict_seq = evict_seq;
        st.last_evict_epoch = evict_epoch;

        if (out_candidates) {
            uint32_t lookahead = (st.epoch_burst >= 2) ? std::min<uint32_t>(10U, 3U + st.epoch_burst) : 4U;
            float base_prob = (st.epoch_burst >= 2)
                                  ? std::min(0.88f, 0.50f + 0.08f * static_cast<float>(st.epoch_burst))
                                  : 0.42f;
            for (uint32_t k = 1; k <= lookahead; ++k) {
                uint64_t nbr_vpn = 0;
                uint64_t nbr_epoch = 0;
                if (shadow.ResolveEvictNeighbor(pid, evict_seq, static_cast<int32_t>(k), &nbr_vpn, &nbr_epoch)) {
                    float p = base_prob * (1.0f - 0.08f * static_cast<float>(k - 1));
                    if (nbr_epoch == evict_epoch) {
                        p = std::min(0.88f, p + 0.08f);
                    }
                    out_candidates->push_back(CandidatePage{ nbr_vpn, p });
                }
            }
        }
    }
}

void Tier1Table::RemovePid(int32_t pid)
{
    pids_.erase(pid);
}

void Tier1Table::Clear()
{
    pids_.clear();
}

} // namespace mext
} // namespace smap
