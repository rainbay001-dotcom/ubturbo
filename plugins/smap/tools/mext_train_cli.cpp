/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Native C++17 Offline Trainer CLI for MEXT TinyTransformer.
 *              Reads recorded mext_ring_event binary traces (or generates
 *              synthetic multi-workload traces) and exports MXTF weights.
 */

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../src/tiering/mext_ring_abi.h"
#include "../src/user/predictor/mext_engine.h"
#include "../src/user/predictor/mext_tokenizer.h"
#include "../src/user/predictor/swap_shadow_map.h"
#include "../src/user/predictor/tiny_transformer.h"

using namespace smap::mext;

namespace {

void MarkLabel(uint8_t mask[MEXT_OUT_DIM], bool evict_space, int64_t delta)
{
    int32_t idx = OffsetToActionIndex(delta);
    if (idx >= 0 && idx < static_cast<int32_t>(MEXT_ACTION_OFFSETS)) {
        uint32_t slot = evict_space ? (MEXT_ACTION_OFFSETS + static_cast<uint32_t>(idx))
                                    : static_cast<uint32_t>(idx);
        mask[slot] = 1;
    }
}

struct RawFaultRecord {
    uint64_t vpn;
    uint64_t evict_seq;
    bool has_evict;
    uint8_t vma_slot;
    uint64_t ts_ns;
};

std::vector<std::array<TrainingStepSample, TF_CTX_LEN>>
BuildWindowsFromEvents(const std::vector<mext_ring_event> &events, uint32_t horizon = 12)
{
    SwapShadowMap shadow;
    std::unordered_map<uint32_t, std::vector<RawFaultRecord>> per_pid;

    for (const auto &ev : events) {
        if (ev.type == MEXT_EVT_EVICTED) {
            shadow.RecordEviction(static_cast<int32_t>(ev.pid), ev.vpn, ev.aux);
        } else if (ev.type == MEXT_EVT_SWAP_FAULT) {
            uint64_t seq = 0;
            uint64_t epoch = 0;
            bool has_ev = shadow.OnPageFault(static_cast<int32_t>(ev.pid), ev.vpn, &seq, &epoch);
            per_pid[ev.pid].push_back(
                RawFaultRecord{ ev.vpn, seq, has_ev, static_cast<uint8_t>(ev.aux & 31ULL), ev.timestamp });
        }
    }

    std::vector<std::array<TrainingStepSample, TF_CTX_LEN>> windows;
    for (const auto &kv : per_pid) {
        const auto &stream = kv.second;
        if (stream.size() < TF_CTX_LEN + horizon) {
            continue;
        }
        for (size_t start = 0; start + TF_CTX_LEN + horizon <= stream.size(); start += TF_CTX_LEN / 2) {
            std::array<TrainingStepSample, TF_CTX_LEN> win{};
            for (uint32_t t = 0; t < TF_CTX_LEN; ++t) {
                size_t idx = start + t;
                const auto &cur = stream[idx];
                bool has_prev = (idx > 0);
                int64_t dvpn = has_prev
                                   ? (static_cast<int64_t>(cur.vpn) - static_cast<int64_t>(stream[idx - 1].vpn))
                                   : 0;
                bool both_ev = has_prev && cur.has_evict && stream[idx - 1].has_evict;
                int64_t dseq = both_ev ? (static_cast<int64_t>(cur.evict_seq) -
                                          static_cast<int64_t>(stream[idx - 1].evict_seq))
                                       : 0;
                uint64_t dt = (has_prev && cur.ts_ns >= stream[idx - 1].ts_ns)
                                  ? (cur.ts_ns - stream[idx - 1].ts_ns)
                                  : 10000ULL;

                win[t].token.dvpn_tok = EncodeDeltaToken(dvpn, has_prev);
                win[t].token.devict_tok = EncodeDeltaToken(dseq, both_ev);
                win[t].token.vma_tok = cur.vma_slot;
                win[t].token.dt_tok = EncodeDtToken(dt);
                win[t].supervised = (t >= 4);

                for (uint32_t h = 1; h <= horizon; ++h) {
                    const auto &fut = stream[idx + h];
                    MarkLabel(win[t].target_mask, false,
                              static_cast<int64_t>(fut.vpn) - static_cast<int64_t>(cur.vpn));
                    if (cur.has_evict && fut.has_evict) {
                        MarkLabel(win[t].target_mask, true,
                                  static_cast<int64_t>(fut.evict_seq) - static_cast<int64_t>(cur.evict_seq));
                    }
                }
            }
            windows.push_back(win);
        }
    }
    return windows;
}

std::vector<mext_ring_event> GenerateSyntheticTrace()
{
    std::vector<mext_ring_event> evs;
    uint64_t ts = 1000000ULL;

    /* Workload 1 (pid 101): Pointer-chasing EDA graph co-evicted in batches */
    std::vector<uint64_t> graph_vpns;
    uint64_t lfsr = 0xACE1BEEFULL;
    for (int i = 0; i < 512; ++i) {
        lfsr = (lfsr >> 1U) ^ (-(lfsr & 1ULL) & 0xD0000001ULL);
        uint64_t vpn = 0x100000ULL + ((lfsr & 0xFFFFFULL) * 17ULL);
        graph_vpns.push_back(vpn);
        evs.push_back(mext_ring_event{ MEXT_EVT_EVICTED, 101, vpn, static_cast<uint64_t>(1 + i / 64), ts });
        ts += 500;
    }
    for (size_t i = 0; i < graph_vpns.size(); ++i) {
        evs.push_back(mext_ring_event{ MEXT_EVT_SWAP_FAULT, 101, graph_vpns[i], 3, ts });
        ts += 25000;
    }

    /* Workload 2 (pid 202): Interleaved two-arena strided + KV block streams */
    uint64_t arena_a = 0x40000ULL;
    uint64_t arena_b = 0x900000ULL;
    for (int i = 0; i < 512; ++i) {
        uint64_t vpn = ((i & 1) == 0) ? (arena_a + static_cast<uint64_t>(i / 2) * 2ULL)
                                      : (arena_b + static_cast<uint64_t>(i / 2) * 4ULL);
        uint64_t vma = ((i & 1) == 0) ? 1 : 9;
        evs.push_back(mext_ring_event{ MEXT_EVT_SWAP_FAULT, 202, vpn, vma, ts });
        ts += 18000;
    }

    return evs;
}

} // namespace

int main(int argc, char **argv)
{
    std::string trace_path;
    std::string out_path = "mext_tiny_tf.bin";
    uint32_t epochs = 15;
    float lr = 0.01f;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--trace" && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        } else if (arg == "--epochs" && i + 1 < argc) {
            epochs = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (arg == "--lr" && i + 1 < argc) {
            lr = std::strtof(argv[++i], nullptr);
        }
    }

    std::vector<mext_ring_event> events;
    if (!trace_path.empty()) {
        std::ifstream ifs(trace_path, std::ios::binary | std::ios::ate);
        if (!ifs) {
            std::cerr << "Failed to open trace file: " << trace_path << "\n";
            return 1;
        }
        auto bytes = ifs.tellg();
        size_t n = static_cast<size_t>(bytes) / sizeof(mext_ring_event);
        events.resize(n);
        ifs.seekg(0, std::ios::beg);
        ifs.read(reinterpret_cast<char *>(events.data()),
                 static_cast<std::streamsize>(n * sizeof(mext_ring_event)));
    } else {
        events = GenerateSyntheticTrace();
    }

    auto windows = BuildWindowsFromEvents(events, 12);
    if (windows.empty()) {
        std::cerr << "Not enough events in trace to construct training windows.\n";
        return 1;
    }

    TinyTransformerTrainer trainer(42U);
    auto t0 = std::chrono::steady_clock::now();
    float first_loss = 0.0f;
    float last_loss = 0.0f;
    const size_t batch_size = 8;

    for (uint32_t ep = 0; ep < epochs; ++ep) {
        float ep_loss = 0.0f;
        float cur_lr = (ep >= (epochs * 2) / 3) ? (lr * 0.4f) : lr;
        for (size_t i = 0; i < windows.size(); i += batch_size) {
            size_t end = std::min(windows.size(), i + batch_size);
            trainer.ZeroGrad();
            for (size_t j = i; j < end; ++j) {
                ep_loss += trainer.AccumulateGradients(windows[j].data(), TF_CTX_LEN);
            }
            trainer.StepAdamW(cur_lr, 1e-4f, 1.0f / static_cast<float>(end - i));
        }
        ep_loss /= static_cast<float>(windows.size());
        if (ep == 0) {
            first_loss = ep_loss;
        }
        last_loss = ep_loss;
    }

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    TinyTransformer model;
    model.LoadFromFp32(trainer.Params());
    if (!model.SaveWeights(out_path)) {
        std::cerr << "Failed to write weights to " << out_path << "\n";
        return 1;
    }

    std::printf("[mext_train_cli] windows=%zu epochs=%u loss: %.4f -> %.4f (%.1f ms) saved=%s\n",
                windows.size(), epochs, first_loss, last_loss, ms, out_path.c_str());
    return 0;
}
