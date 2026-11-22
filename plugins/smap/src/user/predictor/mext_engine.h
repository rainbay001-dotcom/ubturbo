/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: End-to-End MEXT Predictive Memory Engine combining:
 *              - Lockless kernel fault/eviction mmap ring consumer
 *              - SwapShadowMap (eviction-order spatial delta representation)
 *              - Tier-1 sub-microsecond stride/Markov/co-eviction table
 *              - Tier-2 INT8 TinyTransformer (d=64, H=2, L=32) + online head SGD
 *              - Confidence-gated process_madvise(MADV_WILLNEED) + SMAP_MIG_PREFAULT
 */
#ifndef SMAP_USER_PREDICTOR_MEXT_ENGINE_H
#define SMAP_USER_PREDICTOR_MEXT_ENGINE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../../tiering/mext_ring_abi.h"
#include "../manage/swap_types.h"
#include "swap_shadow_map.h"
#include "tier1_table.h"
#include "tiny_transformer.h"

namespace smap {
namespace mext {

class MextEngine {
public:
    using DispatchCallback = std::function<void(int32_t pid,
                                                const std::vector<uint64_t> &prefetch_vpns,
                                                const std::vector<uint64_t> &prefault_vpns)>;

    MextEngine();
    ~MextEngine();

    int Start(int mig_fd, const MextConfig &cfg, bool spawn_worker = true);
    void Stop();
    bool IsRunning() const { return running_.load(std::memory_order_acquire); }

    int UpdateConfig(const MextConfig &cfg);
    int TrackPid(int32_t pid);
    void UntrackPid(int32_t pid);

    /* Attach an in-memory ring buffer or dispatch callback (for tests/simulation) */
    void AttachExternalRing(mext_ring_buffer *ring);
    void SetDispatchCallback(DispatchCallback cb);

    /* Drain up to `max_events` from the shared ring buffer; returns events processed */
    size_t ProcessRingOnce(size_t max_events = 512);

    /* Direct event injection entrypoints (used by ring consumer & trace harness) */
    void HandleEvictionEvent(int32_t pid, uint64_t vpn, uint64_t epoch);
    void HandleFaultEvent(int32_t pid, uint64_t vpn, uint8_t vma_slot,
                          uint64_t timestamp_ns, bool emit_predictions = true);

    /* Bootstrap pre-training on canonical multi-stream + co-eviction patterns */
    static void BootstrapPretrain(TinyTransformer *tf, uint32_t epochs = 6);

    MextStats GetStatsSnapshot();
    const TinyTransformer &Transformer() const { return transformer_; }
    TinyTransformer &MutableTransformer() { return transformer_; }
    const SwapShadowMap &ShadowMap() const { return shadow_map_; }

private:
    struct PendingSupervision {
        uint64_t anchor_vpn = 0;
        uint64_t anchor_evict_seq = 0;
        bool has_evict_seq = false;
        uint32_t remaining_horizon = 16;
        float hidden[TF_D_MODEL] = {};
        uint8_t target_mask[MEXT_OUT_DIM] = {};
    };

    struct PidRuntimeState {
        uint64_t last_vpn = 0;
        uint64_t last_evict_seq = 0;
        uint64_t last_ts_ns = 0;
        uint64_t fault_count = 0;
        bool has_last_vpn = false;
        bool has_last_evict = false;
        int pidfd = -1;

        std::deque<PendingSupervision> pending;
        /* Ring of recently predicted VPNs to measure online prediction precision */
        uint64_t predicted_ring[512] = { 0 };
        uint32_t predicted_cursor = 0;
    };

    void WorkerLoop();
    bool PopRingEvent(mext_ring_event *out);
    void DispatchCandidates(int32_t pid,
                            PidRuntimeState *pst,
                            const std::vector<CandidatePage> &merged);
    void RefreshPsiIfNeeded(uint64_t now_ns);
    int EnsurePidfd(int32_t pid, PidRuntimeState *pst);

    mutable std::mutex mu_;
    MextConfig cfg_{};
    int mig_fd_ = -1;
    uint32_t page_shift_ = 12;

    mext_ring_buffer *mmap_ring_ = nullptr;
    size_t mmap_bytes_ = 0;
    bool owns_mmap_ = false;

    SwapShadowMap shadow_map_;
    Tier1Table tier1_;
    TinyTransformer transformer_;
    std::unordered_map<int32_t, PidRuntimeState> pid_states_;
    DispatchCallback dispatch_cb_;

    std::atomic<bool> running_{ false };
    std::thread worker_;

    /* Telemetry counters */
    uint64_t faults_processed_ = 0;
    uint64_t evicts_tracked_ = 0;
    uint64_t tier1_preds_ = 0;
    uint64_t tf_preds_ = 0;
    uint64_t prefetch_issued_ = 0;
    uint64_t prefault_issued_ = 0;
    uint64_t prediction_hits_ = 0;
    uint64_t predictions_evaluated_ = 0;
    uint64_t online_train_steps_ = 0;
    double avg_infer_us_ = 0.0;
    double psi_some_avg10_ = 0.0;
    uint64_t last_psi_check_ns_ = 0;
};

} // namespace mext
} // namespace smap

#endif /* SMAP_USER_PREDICTOR_MEXT_ENGINE_H */
