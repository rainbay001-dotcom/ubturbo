/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Comprehensive Unit, Numerical Gradient, Concurrency, Latency,
 *              and Workload Hit-Rate Test Suite for MEXT Predictive Memory.
 */

#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "../../src/tiering/mext_ring_abi.h"
#include "../../src/user/predictor/mext_engine.h"
#include "../../src/user/predictor/mext_predictor_c_api.h"
#include "../../src/user/predictor/mext_tokenizer.h"
#include "../../src/user/predictor/swap_shadow_map.h"
#include "../../src/user/predictor/tier1_table.h"
#include "../../src/user/predictor/tiny_transformer.h"

using namespace smap::mext;

#define REQUIRE(cond)                                                                         \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            std::fprintf(stderr, "TEST FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            std::exit(1);                                                                     \
        }                                                                                     \
    } while (0)

namespace {

void TestTokenizer()
{
    REQUIRE(EncodeDeltaToken(0, false) == MEXT_TOKEN_UNKNOWN);
    REQUIRE(EncodeDeltaToken(0, true) == 64);
    REQUIRE(EncodeDeltaToken(-64, true) == 0);
    REQUIRE(EncodeDeltaToken(64, true) == 128);

    /* Non-exact log-bucketed deltas */
    uint8_t pos_big = EncodeDeltaToken(4096, true);
    uint8_t neg_big = EncodeDeltaToken(-4096, true);
    REQUIRE(pos_big >= MEXT_EXACT_COUNT + MEXT_LOG_BUCKETS && pos_big < MEXT_TOKEN_UNKNOWN);
    REQUIRE(neg_big >= MEXT_EXACT_COUNT && neg_big < MEXT_EXACT_COUNT + MEXT_LOG_BUCKETS);

    /* Extreme int64 safety */
    uint8_t min_tok = EncodeDeltaToken(INT64_MIN, true);
    uint8_t max_tok = EncodeDeltaToken(INT64_MAX, true);
    REQUIRE(min_tok < MEXT_TOKEN_UNKNOWN);
    REQUIRE(max_tok < MEXT_TOKEN_UNKNOWN);

    /* Action offset bijection [-64..-1, +1..+64] <-> [0..127] */
    for (uint32_t idx = 0; idx < MEXT_ACTION_OFFSETS; ++idx) {
        int32_t off = ActionIndexToOffset(idx);
        REQUIRE(off != 0 && off >= -64 && off <= 64);
        REQUIRE(OffsetToActionIndex(off) == static_cast<int32_t>(idx));
    }
    REQUIRE(OffsetToActionIndex(0) == -1);
    REQUIRE(OffsetToActionIndex(65) == -1);
    std::printf("[PASS] Test 1: Multi-space Delta Tokenizer & Action Bijection\n");
}

void TestSwapShadowMap()
{
    SwapShadowMap shadow;
    const int32_t pid = 4242;

    uint64_t s1 = shadow.RecordEviction(pid, 0x10010ULL, 7);
    uint64_t s2 = shadow.RecordEviction(pid, 0x9F820ULL, 7);
    uint64_t s3 = shadow.RecordEviction(pid, 0x00412ULL, 7);
    REQUIRE(s1 == 1 && s2 == 2 && s3 == 3);
    REQUIRE(shadow.TotalActivePages() == 3);

    /* Fault on s1: neighbor +1 and +2 resolve to non-contiguous pointer-chase VPNs */
    uint64_t f_seq = 0;
    uint64_t f_ep = 0;
    REQUIRE(shadow.OnPageFault(pid, 0x10010ULL, &f_seq, &f_ep));
    REQUIRE(f_seq == s1 && f_ep == 7);
    REQUIRE(!shadow.IsSwappedOut(pid, 0x10010ULL));

    uint64_t nbr1 = 0;
    uint64_t nbr2 = 0;
    REQUIRE(shadow.ResolveEvictNeighbor(pid, f_seq, +1, &nbr1));
    REQUIRE(shadow.ResolveEvictNeighbor(pid, f_seq, +2, &nbr2));
    REQUIRE(nbr1 == 0x9F820ULL);
    REQUIRE(nbr2 == 0x00412ULL);

    /* Wrap-around stress > 65536 evictions */
    for (uint64_t i = 0; i < MEXT_SHADOW_LOG_CAP + 500; ++i) {
        shadow.RecordEviction(pid, 0x200000ULL + i, 9);
    }
    REQUIRE(shadow.TotalActivePages() == MEXT_SHADOW_LOG_CAP);
    shadow.RemovePid(pid);
    REQUIRE(shadow.TotalActivePages() == 0);
    std::printf("[PASS] Test 2: SwapShadowMap Co-Eviction Resolution & Ring Wrap-Around\n");
}

void TestAnalyticalGradients()
{
    TinyTransformerTrainer trainer(12345U);
    TrainingStepSample seq[6]{};
    for (int t = 0; t < 6; ++t) {
        seq[t].token.dvpn_tok = static_cast<uint8_t>(64 + t + 1);
        seq[t].token.devict_tok = static_cast<uint8_t>(65);
        seq[t].token.vma_tok = static_cast<uint8_t>(t % 4);
        seq[t].token.dt_tok = 3;
        seq[t].supervised = (t >= 2);
        seq[t].target_mask[64 + t] = 1;
        seq[t].target_mask[128 + 64] = 1;
    }

    trainer.ZeroGrad();
    trainer.AccumulateGradients(seq, 6);
    TinyTransformerFp32Params analytic = trainer.Grads();

    /* Spot-check finite-difference numerical gradients across every layer type */
    const float eps = 1e-3f;
    auto check_param = [&](float *param_ptr, float analytic_g, const char *name) {
        float orig = *param_ptr;
        *param_ptr = orig + eps;
        TinyTransformerTrainer tp = trainer;
        tp.Params() = trainer.Params();
        double loss_p = tp.AccumulateGradients(seq, 6);

        *param_ptr = orig - eps;
        TinyTransformerTrainer tm = trainer;
        tm.Params() = trainer.Params();
        double loss_m = tm.AccumulateGradients(seq, 6);
        *param_ptr = orig;

        float num_g = static_cast<float>((loss_p - loss_m) / (2.0 * static_cast<double>(eps)));
        float diff = std::fabs(analytic_g - num_g);
        float denom = std::max(1e-3f, std::fabs(analytic_g) + std::fabs(num_g));
        if (diff / denom >= 2.5e-2f) {
            std::fprintf(stderr, "Grad mismatch on %s: analytic=%g num=%g\n", name, analytic_g, num_g);
            REQUIRE(false);
        }
    };

    check_param(&trainer.Params().w_head[66][3], analytic.w_head[66][3], "w_head");
    check_param(&trainer.Params().b_head[66], analytic.b_head[66], "b_head");
    check_param(&trainer.Params().rms_final[5], analytic.rms_final[5], "rms_final");
    check_param(&trainer.Params().layers[1].w2[4][9], analytic.layers[1].w2[4][9], "L1.w2");
    check_param(&trainer.Params().layers[1].w1[7][2], analytic.layers[1].w1[7][2], "L1.w1");
    check_param(&trainer.Params().layers[1].wq[1][6], analytic.layers[1].wq[1][6], "L1.wq");
    check_param(&trainer.Params().layers[0].wk[10][3], analytic.layers[0].wk[10][3], "L0.wk");
    check_param(&trainer.Params().layers[0].wv[2][11], analytic.layers[0].wv[2][11], "L0.wv");
    check_param(&trainer.Params().emb_dvpn[67][8], analytic.emb_dvpn[67][8], "emb_dvpn");

    std::printf("[PASS] Test 3: TinyTransformer Analytical vs Finite-Difference Backprop Check\n");
}

void TestQuantizationSerializationAndOnlineAdapt()
{
    TinyTransformer tf;
    MextEngine::BootstrapPretrain(&tf, 6);

    std::vector<uint8_t> blob = tf.SerializeWeights();
    TinyTransformer tf2;
    REQUIRE(tf2.LoadWeightsFromMemory(blob.data(), blob.size()));

    TokenEvent ev{ EncodeDeltaToken(2, true), EncodeDeltaToken(1, true), 4, 5 };
    float p1[MEXT_OUT_DIM]{};
    float p2[MEXT_OUT_DIM]{};
    float hidden[TF_D_MODEL]{};
    tf.ForwardStep(99, ev, true, p1, hidden);
    tf2.ForwardStep(99, ev, true, p2, nullptr);

    for (uint32_t i = 0; i < MEXT_OUT_DIM; ++i) {
        REQUIRE(std::fabs(p1[i] - p2[i]) < 1e-6f);
    }

    /* Verify online FP32 head adaptation monotonically reduces loss on novel offset +13 */
    uint8_t novel_mask[MEXT_OUT_DIM]{};
    int32_t idx13 = OffsetToActionIndex(13);
    REQUIRE(idx13 >= 0);
    novel_mask[idx13] = 1;

    float initial_loss = tf.OnlineAdaptHead(hidden, novel_mask, 0.05f);
    float final_loss = initial_loss;
    for (int step = 0; step < 25; ++step) {
        final_loss = tf.OnlineAdaptHead(hidden, novel_mask, 0.05f);
    }
    REQUIRE(final_loss < initial_loss * 0.5f);
    std::printf("[PASS] Test 4: INT8 Quantization, Weight Serialization & Online FP32 Head SGD (loss %.4f -> %.4f)\n",
                initial_loss, final_loss);
}

void TestLocklessRingConcurrency()
{
    auto ring = std::make_unique<mext_ring_buffer>();
    std::memset(ring.get(), 0, sizeof(mext_ring_buffer));
    ring->hdr.magic = MEXT_RING_MAGIC;
    ring->hdr.version = MEXT_RING_VERSION;
    ring->hdr.capacity = MEXT_RING_CAPACITY;
    ring->hdr.page_shift = 12;

    MextEngine engine;
    MextConfig cfg{};
    MextDefaultConfig(&cfg);
    REQUIRE(engine.Start(-1, cfg, false) == 0);
    engine.AttachExternalRing(ring.get());

    constexpr int kNumProducers = 4;
    constexpr int kEventsPerProducer = 1500;
    std::atomic<bool> producers_done{ false };

    auto push_event = [&](uint32_t type, uint32_t pid, uint64_t vpn, uint64_t aux) {
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
                ev->timestamp = 1000ULL * (w + 1ULL);
                __atomic_store_n(&ev->type, type, __ATOMIC_RELEASE);
                break;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int p = 0; p < kNumProducers; ++p) {
        threads.emplace_back([&, p]() {
            uint32_t pid = static_cast<uint32_t>(500 + p);
            for (int i = 0; i < kEventsPerProducer; ++i) {
                uint64_t vpn = 0x50000ULL + static_cast<uint64_t>(p * 10000 + i);
                if (i % 2 == 0) {
                    push_event(MEXT_EVT_EVICTED, pid, vpn, 1);
                } else {
                    push_event(MEXT_EVT_SWAP_FAULT, pid, vpn - 1, 2);
                }
            }
        });
    }

    size_t consumed = 0;
    while (!producers_done.load(std::memory_order_acquire) ||
           __atomic_load_n(&ring->hdr.tail, __ATOMIC_ACQUIRE) <
               __atomic_load_n(&ring->hdr.write_idx, __ATOMIC_ACQUIRE)) {
        size_t n = engine.ProcessRingOnce(256);
        consumed += n;
        if (n == 0) {
            bool all_joined = true;
            for (auto &t : threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
            if (all_joined) {
                producers_done.store(true, std::memory_order_release);
            }
        }
    }

    REQUIRE(consumed == static_cast<size_t>(kNumProducers * kEventsPerProducer));
    MextStats st = engine.GetStatsSnapshot();
    REQUIRE(st.evicts_tracked == static_cast<uint64_t>(kNumProducers * (kEventsPerProducer / 2)));
    REQUIRE(st.faults_processed == static_cast<uint64_t>(kNumProducers * (kEventsPerProducer / 2)));
    engine.Stop();
    std::printf("[PASS] Test 5: Lockless Multi-Producer Ring Concurrency (%zu events, 0 drops)\n", consumed);
}

void TestWorkloadBenchmarksAndLatency()
{
    /*
     * Compare:
     *   Baseline: Linux next-page swap readahead (vpn + 1)
     *   MEXT:     SwapShadowMap + Tier-1 + INT8 TinyTransformer + Two-Tier Dispatch
     * on two challenging workloads:
     *   (A) EDA Pointer-Chasing Graph (non-contiguous random VAs co-evicted in batches)
     *   (B) Interleaved Dual-Arena Stride + Struct Jump Workload
     */
    MextEngine engine;
    MextConfig cfg{};
    MextDefaultConfig(&cfg);
    cfg.fault_trigger_stride = 2;
    cfg.top_k = 8;
    cfg.prefetch_threshold = 0.25f;
    cfg.prefault_threshold = 0.72f;
    REQUIRE(engine.Start(-1, cfg, false) == 0);

    std::unordered_set<uint64_t> mext_prefetched;
    std::unordered_set<uint64_t> mext_prefaulted;
    engine.SetDispatchCallback([&](int32_t, const std::vector<uint64_t> &pf, const std::vector<uint64_t> &pflt) {
        for (uint64_t v : pf) {
            mext_prefetched.insert(v);
        }
        for (uint64_t v : pflt) {
            mext_prefaulted.insert(v);
        }
    });

    /* --- Workload A: EDA Pointer-Chasing Graph (300 nodes, pseudo-random VAs) --- */
    const int32_t eda_pid = 8001;
    std::vector<uint64_t> graph_nodes;
    uint64_t state = 0x13579BDF2468ACE1ULL;
    for (int i = 0; i < 300; ++i) {
        state ^= state << 13U;
        state ^= state >> 7U;
        state ^= state << 17U;
        /* Non-adjacent 4KB pages separated by hundreds of pages */
        uint64_t vpn = 0x100000ULL + static_cast<uint64_t>(i) * 313ULL + (state & 0x7FULL);
        graph_nodes.push_back(vpn);
        engine.HandleEvictionEvent(eda_pid, vpn, 10 + static_cast<uint64_t>(i / 50));
    }

    std::unordered_set<uint64_t> naive_cache;
    uint32_t naive_eda_hits = 0;
    uint32_t mext_eda_hits = 0;
    uint32_t mext_eda_prefault_hits = 0;

    for (size_t i = 0; i < graph_nodes.size(); ++i) {
        uint64_t vpn = graph_nodes[i];
        if (naive_cache.count(vpn)) {
            ++naive_eda_hits;
        }
        if (mext_prefetched.count(vpn) || mext_prefaulted.count(vpn)) {
            ++mext_eda_hits;
            if (mext_prefaulted.count(vpn)) {
                ++mext_eda_prefault_hits;
            }
        }
        naive_cache.insert(vpn + 1);
        engine.HandleFaultEvent(eda_pid, vpn, 5, static_cast<uint64_t>((i + 1) * 30000ULL));
    }

    double naive_eda_rate = 100.0 * static_cast<double>(naive_eda_hits) / static_cast<double>(graph_nodes.size());
    double mext_eda_rate = 100.0 * static_cast<double>(mext_eda_hits) / static_cast<double>(graph_nodes.size());

    /* --- Workload B: Interleaved Two-Arena Stride (+4 in VMA 2, +8 in VMA 11) --- */
    const int32_t ai_pid = 8002;
    mext_prefetched.clear();
    mext_prefaulted.clear();
    naive_cache.clear();

    std::vector<std::pair<uint64_t, uint8_t>> ai_stream;
    for (int i = 0; i < 300; ++i) {
        uint64_t vpn = ((i & 1) == 0) ? (0x300000ULL + static_cast<uint64_t>(i / 2) * 4ULL)
                                      : (0x800000ULL + static_cast<uint64_t>(i / 2) * 8ULL);
        uint8_t vma = ((i & 1) == 0) ? 2 : 11;
        ai_stream.push_back({ vpn, vma });
        engine.HandleEvictionEvent(ai_pid, vpn, 20 + static_cast<uint64_t>(i / 60));
    }

    uint32_t naive_ai_hits = 0;
    uint32_t mext_ai_hits = 0;
    for (size_t i = 0; i < ai_stream.size(); ++i) {
        uint64_t vpn = ai_stream[i].first;
        uint8_t vma = ai_stream[i].second;
        if (naive_cache.count(vpn)) {
            ++naive_ai_hits;
        }
        if (mext_prefetched.count(vpn) || mext_prefaulted.count(vpn)) {
            ++mext_ai_hits;
        }
        naive_cache.insert(vpn + 1);
        engine.HandleFaultEvent(ai_pid, vpn, vma, static_cast<uint64_t>((i + 1) * 20000ULL));
    }

    double naive_ai_rate = 100.0 * static_cast<double>(naive_ai_hits) / static_cast<double>(ai_stream.size());
    double mext_ai_rate = 100.0 * static_cast<double>(mext_ai_hits) / static_cast<double>(ai_stream.size());

    REQUIRE(mext_eda_rate > 85.0 && naive_eda_rate < 5.0);
    REQUIRE(mext_ai_rate > 90.0 && naive_ai_rate < 5.0);

    /* --- Pure Single-Core INT8 TinyTransformer Latency Benchmark (10,000 steps) --- */
    TinyTransformer bench_tf = engine.Transformer();
    TokenEvent bench_tok{ EncodeDeltaToken(4, true), EncodeDeltaToken(1, true), 2, 4 };
    float probs[MEXT_OUT_DIM];
    constexpr int kBenchSteps = 10000;

    auto t0 = std::chrono::steady_clock::now();
    float checksum = 0.0f;
    for (int i = 0; i < kBenchSteps; ++i) {
        bench_tok.dvpn_tok = static_cast<uint8_t>(60 + (i & 7));
        bench_tf.ForwardStep(777, bench_tok, true, probs, nullptr);
        checksum += probs[i & 255];
    }
    auto t1 = std::chrono::steady_clock::now();
    double us_per_step = std::chrono::duration<double, std::micro>(t1 - t0).count() / kBenchSteps;
    REQUIRE(checksum > 0.0f);
    REQUIRE(us_per_step < 50.0); /* Typically ~2-6 us on ARM64 NEON */

    std::printf("[PASS] Test 6: Workload Hit-Rate & Latency Benchmark\n");
    std::printf("       • EDA Pointer-Chase Graph : Naive(+1) = %5.1f%% | MEXT = %5.1f%% (%u zero-fault prefaults)\n",
                naive_eda_rate, mext_eda_rate, mext_eda_prefault_hits);
    std::printf("       • Interleaved Dual-Arena  : Naive(+1) = %5.1f%% | MEXT = %5.1f%%\n",
                naive_ai_rate, mext_ai_rate);
    std::printf("       • INT8 TinyTransformer    : %.2f us / full inference step (1 CPU core, d=64, L=32)\n",
                us_per_step);

    engine.Stop();
}

} // namespace

int main()
{
    std::printf("=== UBTurbo SMAP-MEXT Predictive Memory Verification Suite ===\n");
    TestTokenizer();
    TestSwapShadowMap();
    TestAnalyticalGradients();
    TestQuantizationSerializationAndOnlineAdapt();
    TestLocklessRingConcurrency();
    TestWorkloadBenchmarksAndLatency();
    std::printf("=== ALL 6 MEXT TEST SUITES PASSED ===\n");
    return 0;
}
