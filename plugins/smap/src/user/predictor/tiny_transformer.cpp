/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * smap is licensed under the Mulan PSL v2.
 */

#include "tiny_transformer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define MEXT_HAS_NEON 1
#else
#define MEXT_HAS_NEON 0
#endif

namespace smap {
namespace mext {

namespace {

constexpr float kRmsEps = 1e-5f;
constexpr float kPosWeight = 6.0f;
constexpr double kTwoPi = 6.28318530717958647692;

struct SplitMix32 {
    uint32_t state;
    explicit SplitMix32(uint32_t seed) : state(seed) {}
    uint32_t NextU32()
    {
        uint32_t z = (state += 0x9e3779b9U);
        z = (z ^ (z >> 16)) * 0x85ebca6bU;
        z = (z ^ (z >> 13)) * 0xc2b2ae35U;
        return z ^ (z >> 16);
    }
    float UniformSym(float scale)
    {
        float u = static_cast<float>(NextU32() & 0xFFFFFFU) / static_cast<float>(0x800000U) - 1.0f;
        return u * scale;
    }
};

void InitRoPEFreqs(double inv_freq[TF_D_HEAD / 2])
{
    for (uint32_t i = 0; i < TF_D_HEAD / 2; ++i) {
        double exponent = static_cast<double>(2 * i) / static_cast<double>(TF_D_HEAD);
        inv_freq[i] = 1.0 / std::pow(1000.0, exponent);
    }
}

inline float FastSigmoid(float z)
{
    if (z >= 16.0f) {
        return 0.9999999f;
    }
    if (z <= -16.0f) {
        return 0.0000001f;
    }
    return 1.0f / (1.0f + std::exp(-z));
}

void RmsNormForward(const float *x, const float *gamma, uint32_t dim, float *y, float *inv_rms_out = nullptr)
{
    float sum_sq = 0.0f;
#if MEXT_HAS_NEON
    float32x4_t vsum = vdupq_n_f32(0.0f);
    for (uint32_t i = 0; i < dim; i += 4) {
        float32x4_t vx = vld1q_f32(x + i);
        vsum = vfmaq_f32(vsum, vx, vx);
    }
    sum_sq = vaddvq_f32(vsum);
#else
    for (uint32_t i = 0; i < dim; ++i) {
        sum_sq += x[i] * x[i];
    }
#endif
    float inv_rms = 1.0f / std::sqrt(sum_sq / static_cast<float>(dim) + kRmsEps);
    if (inv_rms_out) {
        *inv_rms_out = inv_rms;
    }
#if MEXT_HAS_NEON
    float32x4_t vinv = vdupq_n_f32(inv_rms);
    for (uint32_t i = 0; i < dim; i += 4) {
        float32x4_t vx = vld1q_f32(x + i);
        float32x4_t vg = vld1q_f32(gamma + i);
        vst1q_f32(y + i, vmulq_f32(vmulq_f32(vx, vinv), vg));
    }
#else
    for (uint32_t i = 0; i < dim; ++i) {
        y[i] = x[i] * inv_rms * gamma[i];
    }
#endif
}

void RmsNormBackward(const float *x, const float *gamma, float inv_rms,
                     const float *dy, uint32_t dim,
                     float *dx_accum, float *dgamma_accum)
{
    float dot = 0.0f;
    for (uint32_t i = 0; i < dim; ++i) {
        float x_hat = x[i] * inv_rms;
        dgamma_accum[i] += dy[i] * x_hat;
        dot += (dy[i] * gamma[i]) * x_hat;
    }
    float mean_dot = dot / static_cast<float>(dim);
    for (uint32_t i = 0; i < dim; ++i) {
        float x_hat = x[i] * inv_rms;
        float u = dy[i] * gamma[i];
        dx_accum[i] += inv_rms * (u - x_hat * mean_dot);
    }
}

void QuantizeRows(const float *src, uint32_t rows, uint32_t cols, int8_t *dst_q, float *dst_scale)
{
    for (uint32_t r = 0; r < rows; ++r) {
        const float *row = src + r * cols;
        int8_t *qrow = dst_q + r * cols;
        float max_abs = 0.0f;
        for (uint32_t c = 0; c < cols; ++c) {
            float a = std::fabs(row[c]);
            if (a > max_abs) {
                max_abs = a;
            }
        }
        if (max_abs < 1e-8f) {
            dst_scale[r] = 0.0f;
            std::memset(qrow, 0, cols);
            continue;
        }
        float scale = max_abs / 127.0f;
        float inv = 127.0f / max_abs;
        dst_scale[r] = scale;
        for (uint32_t c = 0; c < cols; ++c) {
            int32_t q = static_cast<int32_t>(std::lrintf(row[c] * inv));
            q = std::max(-127, std::min(127, q));
            qrow[c] = static_cast<int8_t>(q);
        }
    }
}

/*
 * Quantize input vector x[cols] (cols is multiple of 16) to int8 once,
 * allowing reuse across Q/K/V projections on the same normalized input.
 */
float QuantizeActivation(const float *x, uint32_t cols, int8_t *x_q)
{
    float max_abs = 0.0f;
#if MEXT_HAS_NEON
    float32x4_t vmax = vdupq_n_f32(0.0f);
    for (uint32_t i = 0; i < cols; i += 4) {
        vmax = vmaxq_f32(vmax, vabsq_f32(vld1q_f32(x + i)));
    }
    max_abs = vmaxvq_f32(vmax);
#else
    for (uint32_t i = 0; i < cols; ++i) {
        max_abs = std::max(max_abs, std::fabs(x[i]));
    }
#endif
    if (max_abs < 1e-8f) {
        std::memset(x_q, 0, cols);
        return 0.0f;
    }
    float scale = max_abs / 127.0f;
    float inv = 127.0f / max_abs;
    for (uint32_t i = 0; i < cols; ++i) {
        int32_t q = static_cast<int32_t>(std::lrintf(x[i] * inv));
        x_q[i] = static_cast<int8_t>(std::max(-127, std::min(127, q)));
    }
    return scale;
}

void MatVecInt8PreQ(const int8_t *w_q, const float *w_scale,
                    const int8_t *x_q, float x_scale,
                    uint32_t rows, uint32_t cols, float *y)
{
    if (x_scale == 0.0f) {
        std::memset(y, 0, sizeof(float) * rows);
        return;
    }
    for (uint32_t r = 0; r < rows; ++r) {
        const int8_t *wrow = w_q + r * cols;
#if MEXT_HAS_NEON
        int32x4_t acc32 = vdupq_n_s32(0);
        for (uint32_t c = 0; c < cols; c += 16) {
            int8x16_t vw = vld1q_s8(wrow + c);
            int8x16_t vx = vld1q_s8(x_q + c);
            int16x8_t prod_lo = vmull_s8(vget_low_s8(vw), vget_low_s8(vx));
            int16x8_t prod_hi = vmull_high_s8(vw, vx);
            int16x8_t sum16 = vaddq_s16(prod_lo, prod_hi);
            acc32 = vpadalq_s16(acc32, sum16);
        }
        int32_t dot = vaddvq_s32(acc32);
#else
        int32_t dot = 0;
        for (uint32_t c = 0; c < cols; ++c) {
            dot += static_cast<int32_t>(wrow[c]) * static_cast<int32_t>(x_q[c]);
        }
#endif
        y[r] = static_cast<float>(dot) * (w_scale[r] * x_scale);
    }
}

void InitDefaultWeights(TinyTransformerFp32Params *p, uint32_t seed)
{
    SplitMix32 rng(seed);
    float emb_scale = 0.08f;
    for (uint32_t v = 0; v < MEXT_DELTA_VOCAB; ++v) {
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            p->emb_dvpn[v][d] = rng.UniformSym(emb_scale);
            p->emb_devict[v][d] = rng.UniformSym(emb_scale);
        }
    }
    for (uint32_t v = 0; v < MEXT_VMA_VOCAB; ++v) {
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            p->emb_vma[v][d] = rng.UniformSym(emb_scale);
        }
    }
    for (uint32_t v = 0; v < MEXT_DT_VOCAB; ++v) {
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            p->emb_dt[v][d] = rng.UniformSym(emb_scale);
        }
    }

    float attn_scale = 1.0f / std::sqrt(static_cast<float>(TF_D_MODEL));
    float ffn1_scale = 1.0f / std::sqrt(static_cast<float>(TF_D_MODEL));
    float ffn2_scale = 0.5f / std::sqrt(static_cast<float>(TF_D_FF));

    for (uint32_t l = 0; l < TF_N_LAYERS; ++l) {
        Fp32LayerWeights &lw = p->layers[l];
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            lw.rms_attn[d] = 1.0f;
            lw.rms_ffn[d] = 1.0f;
        }
        for (uint32_t r = 0; r < TF_D_MODEL; ++r) {
            for (uint32_t c = 0; c < TF_D_MODEL; ++c) {
                lw.wq[r][c] = rng.UniformSym(attn_scale);
                lw.wk[r][c] = rng.UniformSym(attn_scale);
                lw.wv[r][c] = rng.UniformSym(attn_scale);
                lw.wo[r][c] = rng.UniformSym(attn_scale * 0.5f);
            }
        }
        for (uint32_t r = 0; r < TF_D_FF; ++r) {
            for (uint32_t c = 0; c < TF_D_MODEL; ++c) {
                lw.w1[r][c] = rng.UniformSym(ffn1_scale);
            }
        }
        for (uint32_t r = 0; r < TF_D_MODEL; ++r) {
            for (uint32_t c = 0; c < TF_D_FF; ++c) {
                lw.w2[r][c] = rng.UniformSym(ffn2_scale);
            }
        }
    }

    for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
        p->rms_final[d] = 1.0f;
    }

    float head_scale = 0.5f / std::sqrt(static_cast<float>(TF_D_MODEL));
    for (uint32_t o = 0; o < MEXT_OUT_DIM; ++o) {
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            p->w_head[o][d] = rng.UniformSym(head_scale);
        }
        /* Prior bias ~ -2.8 so initial sigmoid ~ 0.058 (sparse positives) */
        p->b_head[o] = -2.8f;
    }
}

void MatVecFp32(const float *w, const float *x, uint32_t rows, uint32_t cols, float *y)
{
    for (uint32_t r = 0; r < rows; ++r) {
        const float *wrow = w + r * cols;
#if MEXT_HAS_NEON
        float32x4_t acc = vdupq_n_f32(0.0f);
        for (uint32_t c = 0; c < cols; c += 4) {
            acc = vfmaq_f32(acc, vld1q_f32(wrow + c), vld1q_f32(x + c));
        }
        y[r] = vaddvq_f32(acc);
#else
        float sum = 0.0f;
        for (uint32_t c = 0; c < cols; ++c) {
            sum += wrow[c] * x[c];
        }
        y[r] = sum;
#endif
    }
}

} // namespace

TinyTransformer::TinyTransformer()
{
    InitRoPEFreqs(rope_inv_freq_);
    InitRandom(42U);
}

void TinyTransformer::InitRandom(uint32_t seed)
{
    InitDefaultWeights(&fp32_, seed);
    QuantizeBackbone();
}

void TinyTransformer::LoadFromFp32(const TinyTransformerFp32Params &fp)
{
    fp32_ = fp;
    QuantizeBackbone();
}

void TinyTransformer::QuantizeBackbone()
{
    for (uint32_t l = 0; l < TF_N_LAYERS; ++l) {
        const Fp32LayerWeights &src = fp32_.layers[l];
        QuantLayer &dst = qlayers_[l];
        std::memcpy(dst.rms_attn, src.rms_attn, sizeof(src.rms_attn));
        std::memcpy(dst.rms_ffn, src.rms_ffn, sizeof(src.rms_ffn));

        QuantizeRows(&src.wq[0][0], TF_D_MODEL, TF_D_MODEL, &dst.wq.w[0][0], dst.wq.scale);
        QuantizeRows(&src.wk[0][0], TF_D_MODEL, TF_D_MODEL, &dst.wk.w[0][0], dst.wk.scale);
        QuantizeRows(&src.wv[0][0], TF_D_MODEL, TF_D_MODEL, &dst.wv.w[0][0], dst.wv.scale);
        QuantizeRows(&src.wo[0][0], TF_D_MODEL, TF_D_MODEL, &dst.wo.w[0][0], dst.wo.scale);

        QuantizeRows(&src.w1[0][0], TF_D_FF, TF_D_MODEL, &dst.w1.w[0][0], dst.w1.scale);
        QuantizeRows(&src.w2[0][0], TF_D_MODEL, TF_D_FF, &dst.w2.w[0][0], dst.w2.scale);
    }
}

void TinyTransformer::ApplyRoPE(float *vec_head, uint64_t pos, bool inverse) const
{
    double sign = inverse ? -1.0 : 1.0;
    for (uint32_t i = 0; i < TF_D_HEAD / 2; ++i) {
        double angle = std::fmod(static_cast<double>(pos) * rope_inv_freq_[i], kTwoPi) * sign;
        float cs = static_cast<float>(std::cos(angle));
        float sn = static_cast<float>(std::sin(angle));
        float x0 = vec_head[2 * i];
        float x1 = vec_head[2 * i + 1];
        vec_head[2 * i] = x0 * cs - x1 * sn;
        vec_head[2 * i + 1] = x0 * sn + x1 * cs;
    }
}

void TinyTransformer::ForwardStep(int32_t pid,
                                  const TokenEvent &ev,
                                  bool compute_head,
                                  float *out_probs,
                                  float *out_hidden)
{
    PidKvCache &cache = kv_caches_[pid];
    uint64_t step = cache.step;
    uint32_t slot = static_cast<uint32_t>(step & TF_CTX_MASK);
    if (cache.valid_len < TF_CTX_LEN) {
        ++cache.valid_len;
    }
    cache.step = step + 1;

    uint32_t dvpn = std::min<uint32_t>(ev.dvpn_tok, MEXT_DELTA_VOCAB - 1U);
    uint32_t devict = std::min<uint32_t>(ev.devict_tok, MEXT_DELTA_VOCAB - 1U);
    uint32_t vma = ev.vma_tok & (MEXT_VMA_VOCAB - 1U);
    uint32_t dt = std::min<uint32_t>(ev.dt_tok, MEXT_DT_VOCAB - 1U);

    alignas(16) float x[TF_D_MODEL];
    for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
        x[d] = fp32_.emb_dvpn[dvpn][d] + fp32_.emb_devict[devict][d] +
               fp32_.emb_vma[vma][d] + fp32_.emb_dt[dt][d];
    }

    const float inv_sqrt_d = 1.0f / std::sqrt(static_cast<float>(TF_D_HEAD));
    alignas(16) float u[TF_D_MODEL];
    alignas(16) int8_t u_q[TF_D_FF];
    alignas(16) float q[TF_D_MODEL];
    alignas(16) float k_cur[TF_D_MODEL];
    alignas(16) float v_cur[TF_D_MODEL];
    alignas(16) float attn_ctx[TF_D_MODEL];
    alignas(16) float attn_out[TF_D_MODEL];
    alignas(16) float ffn_mid[TF_D_FF];
    alignas(16) float ffn_out[TF_D_MODEL];
    float scores[TF_CTX_LEN];

    for (uint32_t l = 0; l < TF_N_LAYERS; ++l) {
        const QuantLayer &ql = qlayers_[l];

        /* 1. Pre-RMSNorm + INT8 QKV projections */
        RmsNormForward(x, ql.rms_attn, TF_D_MODEL, u);
        float u_scale = QuantizeActivation(u, TF_D_MODEL, u_q);
        MatVecInt8PreQ(&ql.wq.w[0][0], ql.wq.scale, u_q, u_scale, TF_D_MODEL, TF_D_MODEL, q);
        MatVecInt8PreQ(&ql.wk.w[0][0], ql.wk.scale, u_q, u_scale, TF_D_MODEL, TF_D_MODEL, k_cur);
        MatVecInt8PreQ(&ql.wv.w[0][0], ql.wv.scale, u_q, u_scale, TF_D_MODEL, TF_D_MODEL, v_cur);

        for (uint32_t h = 0; h < TF_N_HEADS; ++h) {
            ApplyRoPE(q + h * TF_D_HEAD, step, false);
            ApplyRoPE(k_cur + h * TF_D_HEAD, step, false);
        }

        std::memcpy(cache.k[l][slot], k_cur, sizeof(k_cur));
        std::memcpy(cache.v[l][slot], v_cur, sizeof(v_cur));

        /* Causal multi-head attention against circular KV cache */
        std::memset(attn_ctx, 0, sizeof(attn_ctx));
        uint32_t len = cache.valid_len;
        for (uint32_t h = 0; h < TF_N_HEADS; ++h) {
            const float *qh = q + h * TF_D_HEAD;
            float max_s = -1e30f;
            for (uint32_t idx = 0; idx < len; ++idx) {
                const float *kh = cache.k[l][idx] + h * TF_D_HEAD;
#if MEXT_HAS_NEON
                float32x4_t vdot = vdupq_n_f32(0.0f);
                for (uint32_t d = 0; d < TF_D_HEAD; d += 4) {
                    vdot = vfmaq_f32(vdot, vld1q_f32(qh + d), vld1q_f32(kh + d));
                }
                float s = vaddvq_f32(vdot) * inv_sqrt_d;
#else
                float s = 0.0f;
                for (uint32_t d = 0; d < TF_D_HEAD; ++d) {
                    s += qh[d] * kh[d];
                }
                s *= inv_sqrt_d;
#endif
                scores[idx] = s;
                if (s > max_s) {
                    max_s = s;
                }
            }

            float sum_exp = 0.0f;
            for (uint32_t idx = 0; idx < len; ++idx) {
                scores[idx] = std::exp(scores[idx] - max_s);
                sum_exp += scores[idx];
            }
            float inv_sum = 1.0f / (sum_exp + 1e-12f);

            float *ctx_h = attn_ctx + h * TF_D_HEAD;
            for (uint32_t idx = 0; idx < len; ++idx) {
                float w = scores[idx] * inv_sum;
                const float *vh = cache.v[l][idx] + h * TF_D_HEAD;
                for (uint32_t d = 0; d < TF_D_HEAD; ++d) {
                    ctx_h[d] += w * vh[d];
                }
            }
        }

        float ctx_scale = QuantizeActivation(attn_ctx, TF_D_MODEL, u_q);
        MatVecInt8PreQ(&ql.wo.w[0][0], ql.wo.scale, u_q, ctx_scale, TF_D_MODEL, TF_D_MODEL, attn_out);
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            x[d] += attn_out[d];
        }

        /* 2. Pre-RMSNorm + ReLU^2 INT8 FFN */
        RmsNormForward(x, ql.rms_ffn, TF_D_MODEL, u);
        u_scale = QuantizeActivation(u, TF_D_MODEL, u_q);
        MatVecInt8PreQ(&ql.w1.w[0][0], ql.w1.scale, u_q, u_scale, TF_D_FF, TF_D_MODEL, ffn_mid);
        for (uint32_t i = 0; i < TF_D_FF; ++i) {
            float m = ffn_mid[i] > 0.0f ? ffn_mid[i] : 0.0f;
            ffn_mid[i] = m * m;
        }
        float mid_scale = QuantizeActivation(ffn_mid, TF_D_FF, u_q);
        MatVecInt8PreQ(&ql.w2.w[0][0], ql.w2.scale, u_q, mid_scale, TF_D_MODEL, TF_D_FF, ffn_out);
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            x[d] += ffn_out[d];
        }
    }

    if (!compute_head) {
        return;
    }

    alignas(16) float h_norm[TF_D_MODEL];
    RmsNormForward(x, fp32_.rms_final, TF_D_MODEL, h_norm);
    if (out_hidden) {
        std::memcpy(out_hidden, h_norm, sizeof(h_norm));
    }

    if (out_probs) {
        for (uint32_t o = 0; o < MEXT_OUT_DIM; ++o) {
            const float *wrow = fp32_.w_head[o];
#if MEXT_HAS_NEON
            float32x4_t acc = vdupq_n_f32(0.0f);
            for (uint32_t d = 0; d < TF_D_MODEL; d += 4) {
                acc = vfmaq_f32(acc, vld1q_f32(wrow + d), vld1q_f32(h_norm + d));
            }
            float logit = vaddvq_f32(acc) + fp32_.b_head[o];
#else
            float logit = fp32_.b_head[o];
            for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
                logit += wrow[d] * h_norm[d];
            }
#endif
            out_probs[o] = FastSigmoid(logit);
        }
    }
}

float TinyTransformer::OnlineAdaptHead(const float hidden[TF_D_MODEL],
                                       const uint8_t target_mask[MEXT_OUT_DIM],
                                       float lr)
{
    float total_loss = 0.0f;

    for (uint32_t o = 0; o < MEXT_OUT_DIM; ++o) {
        float *wrow = fp32_.w_head[o];
        float logit = fp32_.b_head[o];
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            logit += wrow[d] * hidden[d];
        }
        float p = FastSigmoid(logit);
        bool y = (target_mask[o] != 0);
        float grad;
        if (y) {
            total_loss += -kPosWeight * std::log(std::max(p, 1e-7f));
            grad = kPosWeight * (p - 1.0f);
        } else {
            total_loss += -std::log(std::max(1.0f - p, 1e-7f));
            grad = p;
        }
        float step_g = lr * grad;
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            wrow[d] -= step_g * hidden[d] + 1e-4f * lr * wrow[d];
        }
        fp32_.b_head[o] -= step_g;
    }
    return total_loss / static_cast<float>(MEXT_OUT_DIM);
}

void TinyTransformer::ResetPid(int32_t pid)
{
    kv_caches_.erase(pid);
}

void TinyTransformer::ClearState()
{
    kv_caches_.clear();
}

namespace {
struct SerializedHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t d_model;
    uint32_t n_layers;
    uint32_t out_dim;
    uint32_t payload_bytes;
};
} // namespace

std::vector<uint8_t> TinyTransformer::SerializeWeights() const
{
    SerializedHeader hdr{};
    hdr.magic = MEXT_WEIGHTS_MAGIC;
    hdr.version = MEXT_WEIGHTS_VERSION;
    hdr.d_model = TF_D_MODEL;
    hdr.n_layers = TF_N_LAYERS;
    hdr.out_dim = MEXT_OUT_DIM;
    hdr.payload_bytes = static_cast<uint32_t>(sizeof(TinyTransformerFp32Params));

    std::vector<uint8_t> buf(sizeof(hdr) + sizeof(fp32_));
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    std::memcpy(buf.data() + sizeof(hdr), &fp32_, sizeof(fp32_));
    return buf;
}

bool TinyTransformer::LoadWeightsFromMemory(const uint8_t *data, size_t len)
{
    if (!data || len < sizeof(SerializedHeader) + sizeof(TinyTransformerFp32Params)) {
        return false;
    }
    SerializedHeader hdr{};
    std::memcpy(&hdr, data, sizeof(hdr));
    if (hdr.magic != MEXT_WEIGHTS_MAGIC || hdr.version != MEXT_WEIGHTS_VERSION ||
        hdr.d_model != TF_D_MODEL || hdr.n_layers != TF_N_LAYERS ||
        hdr.out_dim != MEXT_OUT_DIM || hdr.payload_bytes != sizeof(TinyTransformerFp32Params)) {
        return false;
    }
    std::memcpy(&fp32_, data + sizeof(hdr), sizeof(fp32_));
    QuantizeBackbone();
    return true;
}

bool TinyTransformer::SaveWeights(const std::string &path) const
{
    std::vector<uint8_t> buf = SerializeWeights();
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        return false;
    }
    ofs.write(reinterpret_cast<const char *>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return ofs.good();
}

bool TinyTransformer::LoadWeights(const std::string &path)
{
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) {
        return false;
    }
    std::streamsize sz = ifs.tellg();
    if (sz <= 0) {
        return false;
    }
    ifs.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    if (!ifs.read(reinterpret_cast<char *>(buf.data()), sz)) {
        return false;
    }
    return LoadWeightsFromMemory(buf.data(), buf.size());
}

/* =========================================================================
 * TinyTransformerTrainer: Full Sequence Analytical Backprop + AdamW
 * ========================================================================= */

TinyTransformerTrainer::TinyTransformerTrainer(uint32_t seed)
{
    InitRoPEFreqs(rope_inv_freq_);
    InitDefaultWeights(&params_, seed);
    ZeroGrad();
}

void TinyTransformerTrainer::ZeroGrad()
{
    std::memset(&grads_, 0, sizeof(grads_));
}

void TinyTransformerTrainer::ApplyRoPE(float *vec_head, uint32_t pos, bool inverse) const
{
    double sign = inverse ? -1.0 : 1.0;
    for (uint32_t i = 0; i < TF_D_HEAD / 2; ++i) {
        double angle = std::fmod(static_cast<double>(pos) * rope_inv_freq_[i], kTwoPi) * sign;
        float cs = static_cast<float>(std::cos(angle));
        float sn = static_cast<float>(std::sin(angle));
        float x0 = vec_head[2 * i];
        float x1 = vec_head[2 * i + 1];
        vec_head[2 * i] = x0 * cs - x1 * sn;
        vec_head[2 * i + 1] = x0 * sn + x1 * cs;
    }
}

double TinyTransformerTrainer::AccumulateGradients(const TrainingStepSample *seq, size_t seq_len)
{
    if (!seq || seq_len == 0) {
        return 0.0;
    }
    const uint32_t T = static_cast<uint32_t>(std::min<size_t>(seq_len, TF_CTX_LEN));
    const float inv_sqrt_d = 1.0f / std::sqrt(static_cast<float>(TF_D_HEAD));

    struct LayerTape {
        float x_in[TF_CTX_LEN][TF_D_MODEL];
        float inv_rms_attn[TF_CTX_LEN];
        float u_attn[TF_CTX_LEN][TF_D_MODEL];
        float q_rot[TF_CTX_LEN][TF_D_MODEL];
        float k_rot[TF_CTX_LEN][TF_D_MODEL];
        float v[TF_CTX_LEN][TF_D_MODEL];
        float attn_w[TF_CTX_LEN][TF_N_HEADS][TF_CTX_LEN];
        float ctx[TF_CTX_LEN][TF_D_MODEL];

        float x_mid[TF_CTX_LEN][TF_D_MODEL];
        float inv_rms_ffn[TF_CTX_LEN];
        float u_ffn[TF_CTX_LEN][TF_D_MODEL];
        float ffn_pre[TF_CTX_LEN][TF_D_FF];
        float ffn_act[TF_CTX_LEN][TF_D_FF];
    };

    LayerTape tape[TF_N_LAYERS]{};
    float x_cur[TF_CTX_LEN][TF_D_MODEL]{};
    float inv_rms_final[TF_CTX_LEN]{};
    float h_final[TF_CTX_LEN][TF_D_MODEL]{};

    /* ---- Forward pass ---- */
    for (uint32_t t = 0; t < T; ++t) {
        const TokenEvent &ev = seq[t].token;
        uint32_t dvpn = std::min<uint32_t>(ev.dvpn_tok, MEXT_DELTA_VOCAB - 1U);
        uint32_t devict = std::min<uint32_t>(ev.devict_tok, MEXT_DELTA_VOCAB - 1U);
        uint32_t vma = ev.vma_tok & (MEXT_VMA_VOCAB - 1U);
        uint32_t dt = std::min<uint32_t>(ev.dt_tok, MEXT_DT_VOCAB - 1U);
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            x_cur[t][d] = params_.emb_dvpn[dvpn][d] + params_.emb_devict[devict][d] +
                          params_.emb_vma[vma][d] + params_.emb_dt[dt][d];
        }
    }

    for (uint32_t l = 0; l < TF_N_LAYERS; ++l) {
        const Fp32LayerWeights &lw = params_.layers[l];
        LayerTape &lt = tape[l];

        for (uint32_t t = 0; t < T; ++t) {
            std::memcpy(lt.x_in[t], x_cur[t], sizeof(x_cur[t]));
            RmsNormForward(lt.x_in[t], lw.rms_attn, TF_D_MODEL, lt.u_attn[t], &lt.inv_rms_attn[t]);
            MatVecFp32(&lw.wq[0][0], lt.u_attn[t], TF_D_MODEL, TF_D_MODEL, lt.q_rot[t]);
            MatVecFp32(&lw.wk[0][0], lt.u_attn[t], TF_D_MODEL, TF_D_MODEL, lt.k_rot[t]);
            MatVecFp32(&lw.wv[0][0], lt.u_attn[t], TF_D_MODEL, TF_D_MODEL, lt.v[t]);
            for (uint32_t h = 0; h < TF_N_HEADS; ++h) {
                ApplyRoPE(lt.q_rot[t] + h * TF_D_HEAD, t, false);
                ApplyRoPE(lt.k_rot[t] + h * TF_D_HEAD, t, false);
            }
        }

        for (uint32_t t = 0; t < T; ++t) {
            for (uint32_t h = 0; h < TF_N_HEADS; ++h) {
                const float *qh = lt.q_rot[t] + h * TF_D_HEAD;
                float max_s = -1e30f;
                for (uint32_t j = 0; j <= t; ++j) {
                    const float *kh = lt.k_rot[j] + h * TF_D_HEAD;
                    float s = 0.0f;
                    for (uint32_t d = 0; d < TF_D_HEAD; ++d) {
                        s += qh[d] * kh[d];
                    }
                    s *= inv_sqrt_d;
                    lt.attn_w[t][h][j] = s;
                    if (s > max_s) {
                        max_s = s;
                    }
                }
                float sum_e = 0.0f;
                for (uint32_t j = 0; j <= t; ++j) {
                    lt.attn_w[t][h][j] = std::exp(lt.attn_w[t][h][j] - max_s);
                    sum_e += lt.attn_w[t][h][j];
                }
                float inv_sum = 1.0f / (sum_e + 1e-12f);
                float *ctx_h = lt.ctx[t] + h * TF_D_HEAD;
                for (uint32_t j = 0; j <= t; ++j) {
                    lt.attn_w[t][h][j] *= inv_sum;
                    const float *vh = lt.v[j] + h * TF_D_HEAD;
                    for (uint32_t d = 0; d < TF_D_HEAD; ++d) {
                        ctx_h[d] += lt.attn_w[t][h][j] * vh[d];
                    }
                }
            }
            float attn_out[TF_D_MODEL];
            MatVecFp32(&lw.wo[0][0], lt.ctx[t], TF_D_MODEL, TF_D_MODEL, attn_out);
            for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
                lt.x_mid[t][d] = lt.x_in[t][d] + attn_out[d];
            }

            RmsNormForward(lt.x_mid[t], lw.rms_ffn, TF_D_MODEL, lt.u_ffn[t], &lt.inv_rms_ffn[t]);
            MatVecFp32(&lw.w1[0][0], lt.u_ffn[t], TF_D_FF, TF_D_MODEL, lt.ffn_pre[t]);
            for (uint32_t i = 0; i < TF_D_FF; ++i) {
                float m = lt.ffn_pre[t][i] > 0.0f ? lt.ffn_pre[t][i] : 0.0f;
                lt.ffn_act[t][i] = m * m;
            }
            float ffn_out[TF_D_MODEL];
            MatVecFp32(&lw.w2[0][0], lt.ffn_act[t], TF_D_MODEL, TF_D_FF, ffn_out);
            for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
                x_cur[t][d] = lt.x_mid[t][d] + ffn_out[d];
            }
        }
    }

    /* ---- Loss & Head Backward ---- */
    float dx[TF_CTX_LEN][TF_D_MODEL]{};
    uint32_t sup_steps = 0;
    for (uint32_t t = 0; t < T; ++t) {
        if (seq[t].supervised) {
            ++sup_steps;
        }
    }
    if (sup_steps == 0) {
        return 0.0;
    }

    const float loss_norm = 1.0f / static_cast<float>(sup_steps * MEXT_OUT_DIM);
    double total_loss = 0.0;

    for (uint32_t t = 0; t < T; ++t) {
        RmsNormForward(x_cur[t], params_.rms_final, TF_D_MODEL, h_final[t], &inv_rms_final[t]);
        if (!seq[t].supervised) {
            continue;
        }

        float dh[TF_D_MODEL]{};
        for (uint32_t o = 0; o < MEXT_OUT_DIM; ++o) {
            float logit = params_.b_head[o];
            for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
                logit += params_.w_head[o][d] * h_final[t][d];
            }
            float p = FastSigmoid(logit);
            bool y = (seq[t].target_mask[o] != 0);
            float dlogit;
            if (y) {
                total_loss += -static_cast<double>(kPosWeight) * std::log(std::max<double>(p, 1e-7)) * loss_norm;
                dlogit = kPosWeight * (p - 1.0f) * loss_norm;
            } else {
                total_loss += -std::log(std::max<double>(1.0 - p, 1e-7)) * loss_norm;
                dlogit = p * loss_norm;
            }
            grads_.b_head[o] += dlogit;
            for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
                grads_.w_head[o][d] += dlogit * h_final[t][d];
                dh[d] += dlogit * params_.w_head[o][d];
            }
        }
        RmsNormBackward(x_cur[t], params_.rms_final, inv_rms_final[t],
                        dh, TF_D_MODEL, dx[t], grads_.rms_final);
    }

    /* ---- Transformer Layers Backward ---- */
    for (int l = static_cast<int>(TF_N_LAYERS) - 1; l >= 0; --l) {
        const Fp32LayerWeights &lw = params_.layers[l];
        Fp32LayerWeights &gw = grads_.layers[l];
        const LayerTape &lt = tape[l];

        float dx_mid[TF_CTX_LEN][TF_D_MODEL]{};
        float dq_rot[TF_CTX_LEN][TF_D_MODEL]{};
        float dk_rot[TF_CTX_LEN][TF_D_MODEL]{};
        float dv[TF_CTX_LEN][TF_D_MODEL]{};

        /* FFN backward */
        for (uint32_t t = 0; t < T; ++t) {
            std::memcpy(dx_mid[t], dx[t], sizeof(dx[t]));

            float da[TF_D_FF]{};
            for (uint32_t r = 0; r < TF_D_MODEL; ++r) {
                float g = dx[t][r];
                for (uint32_t c = 0; c < TF_D_FF; ++c) {
                    gw.w2[r][c] += g * lt.ffn_act[t][c];
                    da[c] += g * lw.w2[r][c];
                }
            }

            float du_ffn[TF_D_MODEL]{};
            for (uint32_t i = 0; i < TF_D_FF; ++i) {
                float pre = lt.ffn_pre[t][i];
                float dp = (pre > 0.0f) ? (2.0f * pre * da[i]) : 0.0f;
                for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
                    gw.w1[i][d] += dp * lt.u_ffn[t][d];
                    du_ffn[d] += dp * lw.w1[i][d];
                }
            }

            RmsNormBackward(lt.x_mid[t], lw.rms_ffn, lt.inv_rms_ffn[t],
                            du_ffn, TF_D_MODEL, dx_mid[t], gw.rms_ffn);
        }

        /* Attention backward */
        float dx_prev[TF_CTX_LEN][TF_D_MODEL]{};
        for (uint32_t t = 0; t < T; ++t) {
            std::memcpy(dx_prev[t], dx_mid[t], sizeof(dx_mid[t]));

            float dctx[TF_D_MODEL]{};
            for (uint32_t r = 0; r < TF_D_MODEL; ++r) {
                float g = dx_mid[t][r];
                for (uint32_t c = 0; c < TF_D_MODEL; ++c) {
                    gw.wo[r][c] += g * lt.ctx[t][c];
                    dctx[c] += g * lw.wo[r][c];
                }
            }

            for (uint32_t h = 0; h < TF_N_HEADS; ++h) {
                const float *dctx_h = dctx + h * TF_D_HEAD;
                float dw[TF_CTX_LEN]{};
                float dot_aw = 0.0f;
                for (uint32_t j = 0; j <= t; ++j) {
                    float w = lt.attn_w[t][h][j];
                    const float *vh = lt.v[j] + h * TF_D_HEAD;
                    float *dvh = dv[j] + h * TF_D_HEAD;
                    float dalpha = 0.0f;
                    for (uint32_t d = 0; d < TF_D_HEAD; ++d) {
                        dvh[d] += w * dctx_h[d];
                        dalpha += dctx_h[d] * vh[d];
                    }
                    dw[j] = dalpha;
                    dot_aw += w * dalpha;
                }

                const float *qh = lt.q_rot[t] + h * TF_D_HEAD;
                float *dqh = dq_rot[t] + h * TF_D_HEAD;
                for (uint32_t j = 0; j <= t; ++j) {
                    float ds = inv_sqrt_d * lt.attn_w[t][h][j] * (dw[j] - dot_aw);
                    const float *kh = lt.k_rot[j] + h * TF_D_HEAD;
                    float *dkh = dk_rot[j] + h * TF_D_HEAD;
                    for (uint32_t d = 0; d < TF_D_HEAD; ++d) {
                        dqh[d] += ds * kh[d];
                        dkh[d] += ds * qh[d];
                    }
                }
            }
        }

        /* Inverse RoPE + QKV linear backward + RMSNorm backward */
        for (uint32_t t = 0; t < T; ++t) {
            for (uint32_t h = 0; h < TF_N_HEADS; ++h) {
                ApplyRoPE(dq_rot[t] + h * TF_D_HEAD, t, true);
                ApplyRoPE(dk_rot[t] + h * TF_D_HEAD, t, true);
            }
            float du_attn[TF_D_MODEL]{};
            for (uint32_t r = 0; r < TF_D_MODEL; ++r) {
                float gq = dq_rot[t][r];
                float gk = dk_rot[t][r];
                float gv = dv[t][r];
                for (uint32_t c = 0; c < TF_D_MODEL; ++c) {
                    float uval = lt.u_attn[t][c];
                    gw.wq[r][c] += gq * uval;
                    gw.wk[r][c] += gk * uval;
                    gw.wv[r][c] += gv * uval;
                    du_attn[c] += gq * lw.wq[r][c] + gk * lw.wk[r][c] + gv * lw.wv[r][c];
                }
            }
            RmsNormBackward(lt.x_in[t], lw.rms_attn, lt.inv_rms_attn[t],
                            du_attn, TF_D_MODEL, dx_prev[t], gw.rms_attn);
        }

        std::memcpy(dx, dx_prev, sizeof(dx));
    }

    /* ---- Embedding Backward ---- */
    for (uint32_t t = 0; t < T; ++t) {
        const TokenEvent &ev = seq[t].token;
        uint32_t dvpn = std::min<uint32_t>(ev.dvpn_tok, MEXT_DELTA_VOCAB - 1U);
        uint32_t devict = std::min<uint32_t>(ev.devict_tok, MEXT_DELTA_VOCAB - 1U);
        uint32_t vma = ev.vma_tok & (MEXT_VMA_VOCAB - 1U);
        uint32_t dt = std::min<uint32_t>(ev.dt_tok, MEXT_DT_VOCAB - 1U);
        for (uint32_t d = 0; d < TF_D_MODEL; ++d) {
            float g = dx[t][d];
            grads_.emb_dvpn[dvpn][d] += g;
            grads_.emb_devict[devict][d] += g;
            grads_.emb_vma[vma][d] += g;
            grads_.emb_dt[dt][d] += g;
        }
    }

    return total_loss;
}

void TinyTransformerTrainer::StepAdamW(float lr, float weight_decay, float batch_scale)
{
    ++step_count_;
    const float beta1 = 0.9f;
    const float beta2 = 0.999f;
    const float eps = 1e-8f;
    const float bc1 = 1.0f - std::pow(beta1, static_cast<float>(step_count_));
    const float bc2 = 1.0f - std::pow(beta2, static_cast<float>(step_count_));

    auto *p = reinterpret_cast<float *>(&params_);
    auto *g = reinterpret_cast<float *>(&grads_);
    auto *m = reinterpret_cast<float *>(&adam_m_);
    auto *v = reinterpret_cast<float *>(&adam_v_);
    constexpr size_t n_floats = sizeof(TinyTransformerFp32Params) / sizeof(float);

    for (size_t i = 0; i < n_floats; ++i) {
        float gi = std::max(-5.0f, std::min(5.0f, g[i] * batch_scale));
        m[i] = beta1 * m[i] + (1.0f - beta1) * gi;
        v[i] = beta2 * v[i] + (1.0f - beta2) * gi * gi;
        float m_hat = m[i] / bc1;
        float v_hat = v[i] / bc2;
        p[i] -= lr * (m_hat / (std::sqrt(v_hat) + eps) + weight_decay * p[i]);
    }
    ZeroGrad();
}

} // namespace mext
} // namespace smap
