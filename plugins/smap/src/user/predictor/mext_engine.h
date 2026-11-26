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
#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
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
    using PressureCallback = std::function<void(uint32_t nid,
                                                uint64_t free_pages,
                                                uint64_t wmark_low_pages,
                                                uint16_t order,
                                                uint64_t timestamp_ns)>;

    MextEngine();
    ~MextEngine();

    int Start(int mig_fd, const MextConfig &cfg, bool spawn_worker = true);
    void Stop();
    bool IsRunning() const { return running_.load(std::memory_order_acquire); }

    int UpdateConfig(const MextConfig &cfg);
    int TrackPid(int32_t pid);
    void UntrackPid(int32_t pid);
    int PageoutPidCold(int mig_fd, int32_t pid, uint32_t nr_pages, bool emergency);
    bool WasPidRecentlyFaulting(int32_t pid, uint64_t within_ns) const;
    uint32_t OpenPidfdCount() const;

    /* Attach an in-memory ring buffer, telemetry file, or callbacks */
    void AttachExternalRing(mext_ring_buffer *ring);
    void SetDispatchCallback(DispatchCallback cb);
    void SetPressureCallback(PressureCallback cb);
    void SetStatsJsonPath(const std::string &path);
    void FlushStatsJson();

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
        uint64_t last_steady_ns = 0;
        uint64_t fault_count = 0;
        uint64_t untracked_faults = 0;
        bool has_last_vpn = false;
        bool has_last_evict = false;

        std::deque<PendingSupervision> pending;
        /* Ring of recently predicted VPNs to measure online prediction precision */
        uint64_t predicted_ring[512] = { 0 };
        uint32_t predicted_cursor = 0;
    };

    static constexpr uint32_t kPrefetchShards = 4U;
    static constexpr uint32_t kPidfdsPerShard = 16U; /* 4 * 16 = 64 max open pidfds */
    static constexpr size_t kMaxShardQueue = 128U;

    struct PrefetchWorkItem {
        int32_t pid = 0;
        bool sync_zram_mode = false;
        std::vector<uint64_t> prefetch_vpns;
        std::vector<uint64_t> prefault_vpns;
    };

    struct PidfdCacheEntry {
        int32_t pid = 0;
        int fd = -1;
    };

    struct PrefetchShard {
        mutable std::mutex mu;
        std::condition_variable cv;
        std::deque<PrefetchWorkItem> queue;
        std::vector<int32_t> pending_close_pids;
        PidfdCacheEntry pidfd_ring[kPidfdsPerShard] = {};
        uint32_t pidfd_cursor = 0;
        std::thread worker;
    };

    void WorkerLoop();
    void PrefetchShardLoop(uint32_t shard_idx);
    void ExecutePrefetchItem(PrefetchShard *shard, int mig_fd, uint32_t page_shift,
                             const PrefetchWorkItem &item);
    int EnsureShardPidfd(PrefetchShard *shard, int32_t pid);
    void CloseShardPidfd(PrefetchShard *shard, int32_t pid);
    void CloseAllShardPidfds(PrefetchShard *shard);
    bool PopRingEvent(mext_ring_event *out);
    void DispatchCandidates(int32_t pid,
                            PidRuntimeState *pst,
                            const std::vector<CandidatePage> &merged);
    void RefreshPsiIfNeeded(uint64_t now_ns);
    void FlushStatsJsonLocked(uint64_t now_ns, bool force);

    mutable std::mutex mu_;
    MextConfig cfg_{};
    int mig_fd_ = -1;
    uint32_t page_shift_ = 12;
    bool async_pool_active_ = false;
    std::atomic<uint32_t> total_open_pidfds_{ 0 };
    PrefetchShard prefetch_shards_[kPrefetchShards];

    mext_ring_buffer *mmap_ring_ = nullptr;
    size_t mmap_bytes_ = 0;
    bool owns_mmap_ = false;

    SwapShadowMap shadow_map_;
    Tier1Table tier1_;
    TinyTransformer transformer_;
    std::unordered_map<int32_t, PidRuntimeState> pid_states_;
    DispatchCallback dispatch_cb_;
    PressureCallback pressure_cb_;

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
    uint64_t last_stats_flush_ns_ = 0;
    std::string stats_json_path_;
};

MextEngine &GlobalMextEngine();

} // namespace mext
} // namespace smap

#endif /* SMAP_USER_PREDICTOR_MEXT_ENGINE_H */
