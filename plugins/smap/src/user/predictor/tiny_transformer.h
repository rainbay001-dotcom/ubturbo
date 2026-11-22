/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 * Description: 2-Layer Causal INT8 Tiny Transformer (d_model=64, H=2, L=32,
 *              FFN=128 ReLU^2, RoPE circular KV cache, ARM64 NEON SIMD) with
 *              online FP32 output-head adaptation and full analytical AdamW
 *              offline sequence trainer.
 */
#ifndef SMAP_USER_PREDICTOR_TINY_TRANSFORMER_H
#define SMAP_USER_PREDICTOR_TINY_TRANSFORMER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "mext_tokenizer.h"

namespace smap {
namespace mext {

constexpr uint32_t TF_D_MODEL = 64;
constexpr uint32_t TF_N_HEADS = 2;
constexpr uint32_t TF_D_HEAD = TF_D_MODEL / TF_N_HEADS; /* 32 */
constexpr uint32_t TF_D_FF = 128;
constexpr uint32_t TF_N_LAYERS = 2;
constexpr uint32_t TF_CTX_LEN = 32;
constexpr uint32_t TF_CTX_MASK = TF_CTX_LEN - 1U;

constexpr uint32_t MEXT_WEIGHTS_MAGIC = 0x4D585446U; /* "MXTF" */
constexpr uint32_t MEXT_WEIGHTS_VERSION = 1U;

struct TokenEvent {
    uint8_t dvpn_tok = static_cast<uint8_t>(MEXT_TOKEN_UNKNOWN);
    uint8_t devict_tok = static_cast<uint8_t>(MEXT_TOKEN_UNKNOWN);
    uint8_t vma_tok = 0;
    uint8_t dt_tok = 0;
};

struct PidKvCache {
    float k[TF_N_LAYERS][TF_CTX_LEN][TF_D_MODEL] = {};
    float v[TF_N_LAYERS][TF_CTX_LEN][TF_D_MODEL] = {};
    uint64_t step = 0;
    uint32_t valid_len = 0;
};

struct QuantLinear64x64 {
    alignas(16) int8_t w[TF_D_MODEL][TF_D_MODEL];
    float scale[TF_D_MODEL];
};

struct QuantLinear128x64 {
    alignas(16) int8_t w[TF_D_FF][TF_D_MODEL];
    float scale[TF_D_FF];
};

struct QuantLinear64x128 {
    alignas(16) int8_t w[TF_D_MODEL][TF_D_FF];
    float scale[TF_D_MODEL];
};

struct Fp32LayerWeights {
    float rms_attn[TF_D_MODEL];
    float wq[TF_D_MODEL][TF_D_MODEL];
    float wk[TF_D_MODEL][TF_D_MODEL];
    float wv[TF_D_MODEL][TF_D_MODEL];
    float wo[TF_D_MODEL][TF_D_MODEL];

    float rms_ffn[TF_D_MODEL];
    float w1[TF_D_FF][TF_D_MODEL];
    float w2[TF_D_MODEL][TF_D_FF];
};

struct TinyTransformerFp32Params {
    float emb_dvpn[MEXT_DELTA_VOCAB][TF_D_MODEL];
    float emb_devict[MEXT_DELTA_VOCAB][TF_D_MODEL];
    float emb_vma[MEXT_VMA_VOCAB][TF_D_MODEL];
    float emb_dt[MEXT_DT_VOCAB][TF_D_MODEL];

    Fp32LayerWeights layers[TF_N_LAYERS];
    float rms_final[TF_D_MODEL];

    alignas(16) float w_head[MEXT_OUT_DIM][TF_D_MODEL];
    alignas(16) float b_head[MEXT_OUT_DIM];
};

class TinyTransformer {
public:
    TinyTransformer();

    void InitRandom(uint32_t seed = 42U);
    void LoadFromFp32(const TinyTransformerFp32Params &fp);
    const TinyTransformerFp32Params &ExportFp32() const { return fp32_; }

    bool SaveWeights(const std::string &path) const;
    bool LoadWeights(const std::string &path);
    bool LoadWeightsFromMemory(const uint8_t *data, size_t len);
    std::vector<uint8_t> SerializeWeights() const;

    /*
     * Fast INT8 incremental step: updates per-PID circular RoPE KV cache.
     * If `compute_head` is true, also runs final RMSNorm + FP32 output head
     * to populate `out_probs[MEXT_OUT_DIM]` and `out_hidden[TF_D_MODEL]`.
     */
    void ForwardStep(int32_t pid,
                     const TokenEvent &ev,
                     bool compute_head,
                     float *out_probs,
                     float *out_hidden = nullptr);

    /*
     * Lightweight live adaptation of only the FP32 output head (64 x 256)
     * from delayed self-supervised ground truth labels y in {0, 1}^256.
     */
    float OnlineAdaptHead(const float hidden[TF_D_MODEL],
                          const uint8_t target_mask[MEXT_OUT_DIM],
                          float lr = 0.015f);

    void ResetPid(int32_t pid);
    void ClearState();

private:
    struct QuantLayer {
        float rms_attn[TF_D_MODEL];
        QuantLinear64x64 wq;
        QuantLinear64x64 wk;
        QuantLinear64x64 wv;
        QuantLinear64x64 wo;

        float rms_ffn[TF_D_MODEL];
        QuantLinear128x64 w1;
        QuantLinear64x128 w2;
    };

    void QuantizeBackbone();
    void ApplyRoPE(float *vec_head, uint64_t pos, bool inverse = false) const;

    TinyTransformerFp32Params fp32_{};
    QuantLayer qlayers_[TF_N_LAYERS]{};
    double rope_inv_freq_[TF_D_HEAD / 2]{};
    std::unordered_map<int32_t, PidKvCache> kv_caches_;
};

/*
 * Offline full-sequence FP32 trainer with exact analytical backpropagation
 * and AdamW optimizer (enables C++ pre-training without external ML libraries).
 */
struct TrainingStepSample {
    TokenEvent token;
    uint8_t target_mask[MEXT_OUT_DIM] = { 0 };
    bool supervised = false;
};

class TinyTransformerTrainer {
public:
    explicit TinyTransformerTrainer(uint32_t seed = 42U);

    TinyTransformerFp32Params &Params() { return params_; }
    const TinyTransformerFp32Params &Params() const { return params_; }

    /*
     * Forward + analytical backward over a sequence window (1 <= T <= TF_CTX_LEN),
     * accumulating parameter gradients and returning mean BCE loss.
     */
    double AccumulateGradients(const TrainingStepSample *seq, size_t seq_len);

    void ZeroGrad();
    void StepAdamW(float lr, float weight_decay = 1e-4f, float batch_scale = 1.0f);

    const TinyTransformerFp32Params &Grads() const { return grads_; }

private:
    void ApplyRoPE(float *vec_head, uint32_t pos, bool inverse) const;

    TinyTransformerFp32Params params_{};
    TinyTransformerFp32Params grads_{};
    TinyTransformerFp32Params adam_m_{};
    TinyTransformerFp32Params adam_v_{};
    double rope_inv_freq_[TF_D_HEAD / 2]{};
    uint64_t step_count_ = 0;
};

} // namespace mext
} // namespace smap

#endif /* SMAP_USER_PREDICTOR_TINY_TRANSFORMER_H */
