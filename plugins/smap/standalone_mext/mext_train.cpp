/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: Standalone Offline Real-Trace Weight Trainer (`mext_train`) for
 *              MEXT's 2-Layer Causal INT8 TinyTransformer. Consumes raw 32-byte
 *              `mext_ring_event` trace files captured by `mextd --dump-trace`
 *              (`--record-only`) and exports converged `mext_tiny_tf.bin` weights.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <unistd.h>

#include "../src/tiering/mext_ring_abi.h"
#include "../src/user/predictor/mext_engine.h"
#include "../src/user/predictor/mext_tokenizer.h"
#include "../src/user/predictor/swap_shadow_map.h"
#include "../src/user/predictor/tiny_transformer.h"

namespace {

using smap::mext::ActionIndexToOffset;
using smap::mext::EncodeDeltaToken;
using smap::mext::EncodeDtToken;
using smap::mext::MEXT_ACTION_OFFSETS;
using smap::mext::MEXT_OUT_DIM;
using smap::mext::MEXT_TOKEN_UNKNOWN;
using smap::mext::MEXT_VMA_VOCAB;
using smap::mext::MextEngine;
using smap::mext::OffsetToActionIndex;
using smap::mext::SwapShadowMap;
using smap::mext::TF_CTX_LEN;
using smap::mext::TinyTransformer;
using smap::mext::TinyTransformerTrainer;
using smap::mext::TokenEvent;
using smap::mext::TrainingStepSample;

struct TrainOptions {
    std::vector<std::string> trace_paths;
    std::string out_weights = "mext_tiny_tf.bin";
    std::string init_weights;
    uint32_t epochs = 8U;
    uint32_t horizon = 12U;
    uint32_t batch_size = 16U;
    size_t max_windows = 50000U;
    float base_lr = 0.008f;
    bool self_test = false;
};

struct RawPidFault {
    uint64_t vpn = 0;
    uint64_t evict_seq = 0;
    bool has_evict = false;
    TokenEvent tok{};
};

void MarkLabel(uint8_t mask[MEXT_OUT_DIM], bool is_evict_space, int64_t delta)
{
    int32_t idx = OffsetToActionIndex(delta);
    if (idx >= 0 && idx < static_cast<int32_t>(MEXT_ACTION_OFFSETS)) {
        uint32_t pos = is_evict_space
                           ? (MEXT_ACTION_OFFSETS + static_cast<uint32_t>(idx))
                           : static_cast<uint32_t>(idx);
        mask[pos] = 1U;
    }
}

using WindowSample = std::array<TrainingStepSample, TF_CTX_LEN>;

bool BuildWindowsFromTraceFile(const std::string &path,
                               uint32_t horizon,
                               std::vector<WindowSample> *out_windows,
                               uint64_t *fault_cnt_out,
                               uint64_t *retry_dup_cnt_out,
                               uint64_t *evict_cnt_out,
                               size_t *pid_cnt_out)
{
    FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        std::perror(("Failed to open trace file " + path).c_str());
        return false;
    }

    constexpr uint32_t kRecentFaultSlots = 16U;
    constexpr uint64_t kRetryDedupWindowNs = 50'000'000ULL; /* 50 ms */

    SwapShadowMap shadow;
    struct RecentFault {
        uint64_t vpn = 0;
        uint64_t ts_ns = 0;
    };
    struct PidStream {
        uint64_t last_vpn = 0;
        uint64_t last_evict_seq = 0;
        uint64_t last_ts = 0;
        bool has_vpn = false;
        bool has_evict = false;
        RecentFault recent[kRecentFaultSlots] = {};
        uint32_t recent_cursor = 0;
        std::vector<RawPidFault> faults;
    };
    std::unordered_map<uint32_t, PidStream> streams;

    uint64_t n_faults = 0;
    uint64_t n_retry_dups = 0;
    uint64_t n_evicts = 0;
    std::vector<mext_ring_event> buf(4096);

    while (true) {
        size_t got = std::fread(buf.data(), sizeof(mext_ring_event), buf.size(), fp);
        if (got == 0) {
            break;
        }
        for (size_t i = 0; i < got; ++i) {
            const auto &ev = buf[i];
            if (ev.pid == 0 || ev.vpn == 0) {
                continue;
            }
            if (ev.type == MEXT_EVT_EVICTED) {
                (void)shadow.RecordEviction(static_cast<int32_t>(ev.pid), ev.vpn, ev.aux);
                auto sit = streams.find(ev.pid);
                if (sit != streams.end()) {
                    for (uint32_t s = 0; s < kRecentFaultSlots; ++s) {
                        if (sit->second.recent[s].vpn == ev.vpn) {
                            sit->second.recent[s].vpn = 0;
                        }
                    }
                }
                ++n_evicts;
            } else if (ev.type == MEXT_EVT_SWAP_FAULT) {
                PidStream &ps = streams[ev.pid];
                /*
                 * Strip Linux `do_swap_page` VM_FAULT_RETRY second-pass duplicates
                 * (including interleaved dual-vCPU retries within 50 ms where the
                 * page was not re-evicted in between) so 32-step windows and Head B
                 * co-eviction labels reflect genuine page-fault transitions only.
                 */
                if (!shadow.IsSwappedOut(static_cast<int32_t>(ev.pid), ev.vpn) &&
                    ev.timestamp > 0) {
                    bool is_retry_dup = false;
                    for (uint32_t s = 0; s < kRecentFaultSlots; ++s) {
                        if (ps.recent[s].vpn == ev.vpn &&
                            ev.timestamp >= ps.recent[s].ts_ns &&
                            (ev.timestamp - ps.recent[s].ts_ns) <= kRetryDedupWindowNs) {
                            is_retry_dup = true;
                            break;
                        }
                    }
                    if (is_retry_dup) {
                        ++n_retry_dups;
                        continue;
                    }
                }
                ps.recent[ps.recent_cursor & (kRecentFaultSlots - 1U)] = { ev.vpn, ev.timestamp };
                ++ps.recent_cursor;

                ++n_faults;
                uint64_t eseq = 0;
                uint64_t eepoch = 0;
                bool has_ev = shadow.OnPageFault(static_cast<int32_t>(ev.pid),
                                                 ev.vpn, &eseq, &eepoch);

                int64_t dvpn = ps.has_vpn
                                   ? (static_cast<int64_t>(ev.vpn) - static_cast<int64_t>(ps.last_vpn))
                                   : 0;
                int64_t dseq = (ps.has_evict && has_ev)
                                   ? (static_cast<int64_t>(eseq) - static_cast<int64_t>(ps.last_evict_seq))
                                   : 0;
                uint64_t dt_ns = (ps.last_ts > 0 && ev.timestamp >= ps.last_ts)
                                     ? (ev.timestamp - ps.last_ts)
                                     : 0ULL;

                RawPidFault rf{};
                rf.vpn = ev.vpn;
                rf.evict_seq = eseq;
                rf.has_evict = has_ev;
                rf.tok.dvpn_tok = EncodeDeltaToken(dvpn, ps.has_vpn);
                rf.tok.devict_tok = EncodeDeltaToken(dseq, ps.has_evict && has_ev);
                rf.tok.vma_tok = static_cast<uint8_t>(ev.aux & (MEXT_VMA_VOCAB - 1U));
                rf.tok.dt_tok = EncodeDtToken(dt_ns);

                ps.last_vpn = ev.vpn;
                ps.has_vpn = true;
                ps.last_ts = ev.timestamp;
                if (has_ev) {
                    ps.last_evict_seq = eseq;
                    ps.has_evict = true;
                }
                ps.faults.push_back(rf);
            }
        }
    }
    std::fclose(fp);

    if (fault_cnt_out) {
        *fault_cnt_out += n_faults;
    }
    if (retry_dup_cnt_out) {
        *retry_dup_cnt_out += n_retry_dups;
    }
    if (evict_cnt_out) {
        *evict_cnt_out += n_evicts;
    }
    if (pid_cnt_out) {
        *pid_cnt_out += streams.size();
    }

    /* Slice each PID's fault timeline into overlapping stride-16 context windows */
    constexpr size_t kWindowStride = 16U;
    for (const auto &kv : streams) {
        const auto &flts = kv.second.faults;
        if (flts.size() < TF_CTX_LEN + 2U) {
            continue;
        }
        for (size_t start = 0; start + TF_CTX_LEN < flts.size(); start += kWindowStride) {
            WindowSample win{};
            bool any_positive = false;
            for (size_t t = 0; t < TF_CTX_LEN; ++t) {
                const RawPidFault &anchor = flts[start + t];
                win[t].token = anchor.tok;
                win[t].supervised = (t >= 3U);
                if (!win[t].supervised) {
                    continue;
                }
                size_t max_h = std::min<size_t>(horizon, flts.size() - (start + t + 1U));
                for (size_t h = 1U; h <= max_h; ++h) {
                    const RawPidFault &fut = flts[start + t + h];
                    int64_t dv = static_cast<int64_t>(fut.vpn) - static_cast<int64_t>(anchor.vpn);
                    MarkLabel(win[t].target_mask, false, dv);
                    if (anchor.has_evict && fut.has_evict) {
                        int64_t ds = static_cast<int64_t>(fut.evict_seq) -
                                     static_cast<int64_t>(anchor.evict_seq);
                        MarkLabel(win[t].target_mask, true, ds);
                    }
                }
                for (uint32_t d = 0; d < MEXT_OUT_DIM; ++d) {
                    if (win[t].target_mask[d]) {
                        any_positive = true;
                        break;
                    }
                }
            }
            if (any_positive) {
                out_windows->push_back(win);
            }
        }
    }
    return true;
}

struct EvalMetrics {
    double top8_step_hit_rate = 0.0; /* % of supervised steps where Top-8 hits >=1 future fault */
    double top8_precision = 0.0;     /* % of Top-8 predicted actions matching ground truth */
};

EvalMetrics EvaluateOnWindows(TinyTransformer *tf,
                              const std::vector<WindowSample> &val_windows,
                              size_t start_idx,
                              size_t end_idx)
{
    EvalMetrics m{};
    if (!tf || end_idx <= start_idx) {
        return m;
    }

    uint64_t steps_eval = 0;
    uint64_t steps_hit = 0;
    uint64_t preds_total = 0;
    uint64_t preds_correct = 0;

    for (size_t w = start_idx; w < end_idx; ++w) {
        const int32_t eval_pid = 900000;
        tf->ResetPid(eval_pid);
        for (uint32_t t = 0; t < TF_CTX_LEN; ++t) {
            float probs[MEXT_OUT_DIM] = {};
            bool sup = val_windows[w][t].supervised;
            tf->ForwardStep(eval_pid, val_windows[w][t].token, sup, sup ? probs : nullptr);
            if (!sup) {
                continue;
            }

            bool has_gt = false;
            for (uint32_t d = 0; d < MEXT_OUT_DIM; ++d) {
                if (val_windows[w][t].target_mask[d]) {
                    has_gt = true;
                    break;
                }
            }
            if (!has_gt) {
                continue;
            }

            std::array<uint32_t, MEXT_OUT_DIM> order{};
            std::iota(order.begin(), order.end(), 0U);
            std::partial_sort(order.begin(), order.begin() + 8, order.end(),
                              [&](uint32_t a, uint32_t b) { return probs[a] > probs[b]; });

            bool step_matched = false;
            for (uint32_t k = 0; k < 8U; ++k) {
                uint32_t act = order[k];
                ++preds_total;
                if (val_windows[w][t].target_mask[act]) {
                    ++preds_correct;
                    step_matched = true;
                }
            }
            ++steps_eval;
            if (step_matched) {
                ++steps_hit;
            }
        }
    }
    tf->ClearState();

    if (steps_eval > 0) {
        m.top8_step_hit_rate = static_cast<double>(steps_hit) / static_cast<double>(steps_eval);
    }
    if (preds_total > 0) {
        m.top8_precision = static_cast<double>(preds_correct) / static_cast<double>(preds_total);
    }
    return m;
}

int ExecuteTraining(const TrainOptions &opt)
{
    std::vector<WindowSample> all_windows;
    uint64_t total_faults = 0;
    uint64_t total_retry_dups = 0;
    uint64_t total_evicts = 0;
    size_t total_pids = 0;

    for (const auto &p : opt.trace_paths) {
        if (!BuildWindowsFromTraceFile(p, opt.horizon, &all_windows,
                                       &total_faults, &total_retry_dups,
                                       &total_evicts, &total_pids)) {
            return 1;
        }
    }

    if (all_windows.size() < 4U) {
        std::fprintf(stderr,
                     "Insufficient supervised trace windows (%zu found; need >= 4). "
                     "Collect a longer workload trace via `mextd --dump-trace trace.bin --record-only`.\n",
                     all_windows.size());
        return 1;
    }

    if (all_windows.size() > opt.max_windows) {
        /* Uniformly subsample across the full timeline to preserve early/mid/late phases */
        std::vector<WindowSample> sampled;
        sampled.reserve(opt.max_windows);
        double step = static_cast<double>(all_windows.size()) / static_cast<double>(opt.max_windows);
        for (size_t i = 0; i < opt.max_windows; ++i) {
            size_t idx = std::min(all_windows.size() - 1U, static_cast<size_t>(i * step));
            sampled.push_back(all_windows[idx]);
        }
        all_windows.swap(sampled);
    }

    const size_t split_idx = std::max<size_t>(1U, (all_windows.size() * 4U) / 5U);
    const size_t val_begin = (split_idx < all_windows.size()) ? split_idx : 0U;
    const size_t val_end = all_windows.size();

    TinyTransformer eval_tf;
    if (!opt.init_weights.empty()) {
        if (!eval_tf.LoadWeights(opt.init_weights)) {
            std::fprintf(stderr, "Failed to load initial weights from %s\n", opt.init_weights.c_str());
            return 1;
        }
    } else {
        MextEngine::BootstrapPretrain(&eval_tf, 4U);
    }

    EvalMetrics before = EvaluateOnWindows(&eval_tf, all_windows, val_begin, val_end);

    TinyTransformerTrainer trainer(20261126U);
    trainer.Params() = eval_tf.ExportFp32();

    std::vector<size_t> train_indices(split_idx);
    std::iota(train_indices.begin(), train_indices.end(), 0U);
    std::mt19937 rng(20261126U);

    std::printf("=== MEXT Offline Real-Trace TinyTransformer Trainer ===\n"
                "Traces loaded  : %zu file(s) | unique_faults=%llu (stripped %llu VM_FAULT_RETRY dups) | "
                "evictions=%llu | pids=%zu\n"
                "Context windows: %zu total (%zu train / %zu held-out validation, L=%u, H=%u)\n"
                "Pre-train Val  : Top-8 Step Recall = %.2f%% | Top-8 Action Precision = %.2f%%\n",
                opt.trace_paths.size(),
                static_cast<unsigned long long>(total_faults),
                static_cast<unsigned long long>(total_retry_dups),
                static_cast<unsigned long long>(total_evicts),
                total_pids,
                all_windows.size(), split_idx, val_end - val_begin,
                TF_CTX_LEN, opt.horizon,
                before.top8_step_hit_rate * 100.0,
                before.top8_precision * 100.0);

    const size_t bsz = std::max<size_t>(1U, opt.batch_size);
    for (uint32_t ep = 0; ep < opt.epochs; ++ep) {
        std::shuffle(train_indices.begin(), train_indices.end(), rng);
        double cos_decay = 0.5 * (1.0 + std::cos(3.14159265358979323846 *
                                                 static_cast<double>(ep) /
                                                 static_cast<double>(std::max(1U, opt.epochs))));
        float lr = static_cast<float>(opt.base_lr * (0.15 + 0.85 * cos_decay));

        double epoch_loss = 0.0;
        size_t batches = 0;
        for (size_t i = 0; i < train_indices.size(); i += bsz) {
            size_t end = std::min(train_indices.size(), i + bsz);
            trainer.ZeroGrad();
            double b_loss = 0.0;
            for (size_t j = i; j < end; ++j) {
                b_loss += trainer.AccumulateGradients(
                    all_windows[train_indices[j]].data(), TF_CTX_LEN);
            }
            trainer.StepAdamW(lr, 1e-4f, 1.0f / static_cast<float>(end - i));
            epoch_loss += b_loss / static_cast<double>(end - i);
            ++batches;
        }
        std::printf("  [Epoch %2u/%2u] lr=%.5f | mean_bce_loss=%.5f\n",
                    ep + 1U, opt.epochs, lr,
                    batches > 0 ? (epoch_loss / static_cast<double>(batches)) : 0.0);
    }

    eval_tf.LoadFromFp32(trainer.Params());
    EvalMetrics after = EvaluateOnWindows(&eval_tf, all_windows, val_begin, val_end);

    if (!eval_tf.SaveWeights(opt.out_weights)) {
        std::fprintf(stderr, "Failed to write trained weights to %s\n", opt.out_weights.c_str());
        return 1;
    }

    std::printf("Post-train Val : Top-8 Step Recall = %.2f%% | Top-8 Action Precision = %.2f%%\n"
                "Saved INT8+FP32 weights -> %s\n"
                "Deploy with    : ./mextd --track-all --weights %s --no-online-adapt\n",
                after.top8_step_hit_rate * 100.0,
                after.top8_precision * 100.0,
                opt.out_weights.c_str(),
                opt.out_weights.c_str());
    return 0;
}

int RunSelfTest()
{
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / ("mext_train_test_" + std::to_string(getpid()));
    fs::create_directories(dir);
    struct Guard {
        fs::path p;
        ~Guard()
        {
            std::error_code ec;
            fs::remove_all(p, ec);
        }
    } cleanup{ dir };

    fs::path trace_file = dir / "sample_trace.bin";
    fs::path weights_file = dir / "trained_tf.bin";

    /* Synthesize Firecracker-like fragmented GPA co-eviction + stride trace */
    FILE *fp = std::fopen(trace_file.c_str(), "wb");
    if (!fp) {
        return 1;
    }
    const uint32_t pid = 77001U;
    std::vector<uint64_t> coevicted_gpans;
    for (uint64_t i = 0; i < 320ULL; ++i) {
        /* Scattered GPAs (stride +37 / -19) evicted in temporal order */
        uint64_t gpa_vpn = 0xfffef939dULL + ((i * 37ULL) % 8192ULL) + i * 3ULL;
        coevicted_gpans.push_back(gpa_vpn);
        mext_ring_event ev{};
        ev.type = MEXT_EVT_EVICTED;
        ev.pid = pid;
        ev.vpn = gpa_vpn;
        ev.aux = 100ULL + (i / 16ULL);
        ev.timestamp = (i + 1ULL) * 10000ULL;
        (void)std::fwrite(&ev, sizeof(ev), 1, fp);
    }
    /* Replay faults following co-eviction order (+3 step in eviction sequence),
     * injecting realistic dual-vCPU VM_FAULT_RETRY second-pass duplicates! */
    for (size_t step = 0; step < 260U; ++step) {
        size_t idx = (step * 3U) % coevicted_gpans.size();
        mext_ring_event ev{};
        ev.type = MEXT_EVT_SWAP_FAULT;
        ev.pid = pid;
        ev.vpn = coevicted_gpans[idx];
        ev.aux = static_cast<uint64_t>((ev.vpn >> 14) & 0x1fULL);
        ev.timestamp = 5'000'000ULL + step * 50000ULL;
        (void)std::fwrite(&ev, sizeof(ev), 1, fp);

        /* Simulated VM_FAULT_RETRY duplicate 180 ns later on the same swap PTE */
        ev.timestamp += 180ULL;
        (void)std::fwrite(&ev, sizeof(ev), 1, fp);
    }
    std::fclose(fp);

    TrainOptions opt{};
    opt.trace_paths.push_back(trace_file.string());
    opt.out_weights = weights_file.string();
    opt.epochs = 6U;
    opt.horizon = 8U;
    opt.batch_size = 4U;

    if (ExecuteTraining(opt) != 0) {
        return 1;
    }

    TinyTransformer loaded;
    if (!loaded.LoadWeights(weights_file.string())) {
        std::fprintf(stderr, "[FAIL] Could not reload trained weights\n");
        return 1;
    }
    std::printf("[PASS] mext_train real-trace trainer self-test succeeded\n");
    return 0;
}

void PrintUsage(const char *prog)
{
    std::printf(
        "Usage: %s --trace <trace.bin> [--trace <trace2.bin> ...] [options]\n\n"
        "Offline Real-Trace Weight Trainer for MEXT TinyTransformer\n"
        "(Consumes raw 32-byte mext_ring_event dumps from `mextd --dump-trace`)\n\n"
        "Options:\n"
        "  --trace <path>          Input raw trace file (repeatable)\n"
        "  --out <weights.bin>     Output weights path (default: mext_tiny_tf.bin)\n"
        "  --init-weights <path>   Optional starting weights file (default: bootstrap init)\n"
        "  --epochs <N>            Training epochs (default: 8)\n"
        "  --horizon <H>           Supervised future fault horizon per step (default: 12)\n"
        "  --batch-size <B>        Mini-batch size in 32-step windows (default: 16)\n"
        "  --max-windows <M>       Max sampled windows across trace (default: 50000)\n"
        "  --lr <float>            Initial AdamW learning rate (default: 0.008)\n"
        "  --self-test             Run synthetic Firecracker co-eviction training test\n"
        "  -h, --help              Show this help\n",
        prog);
}

} // namespace

int main(int argc, char **argv)
{
    TrainOptions opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            PrintUsage(argv[0]);
            return 0;
        } else if (a == "--self-test") {
            opt.self_test = true;
        } else if (a == "--trace" && i + 1 < argc) {
            opt.trace_paths.push_back(argv[++i]);
        } else if (a == "--out" && i + 1 < argc) {
            opt.out_weights = argv[++i];
        } else if (a == "--init-weights" && i + 1 < argc) {
            opt.init_weights = argv[++i];
        } else if (a == "--epochs" && i + 1 < argc) {
            opt.epochs = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (a == "--horizon" && i + 1 < argc) {
            opt.horizon = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (a == "--batch-size" && i + 1 < argc) {
            opt.batch_size = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (a == "--max-windows" && i + 1 < argc) {
            opt.max_windows = static_cast<size_t>(std::strtoull(argv[++i], nullptr, 10));
        } else if (a == "--lr" && i + 1 < argc) {
            opt.base_lr = std::strtof(argv[++i], nullptr);
        } else {
            std::fprintf(stderr, "Unknown or incomplete argument: %s\n", a.c_str());
            PrintUsage(argv[0]);
            return 1;
        }
    }

    if (opt.self_test) {
        return RunSelfTest();
    }
    if (opt.trace_paths.empty()) {
        PrintUsage(argv[0]);
        return 1;
    }
    return ExecuteTraining(opt);
}
