#include "mamba_layer.h"
#include <algorithm>
#include <cstring>

#if defined(__AVX__) || defined(__AVX2__)
#include <immintrin.h>
#endif

MambaLayer::MambaLayer(int64_t d_model, int64_t d_state, int64_t d_conv, int64_t expand)
    : d_model_(d_model), d_state_(d_state), d_conv_(d_conv), expand_(expand), conv_pos_(0) {

    d_inner_ = expand_ * d_model_;

    // Pre-allocate all internal memory at construction.
    // No allocations must occur inside processSample() or processBlock().
    // Initialize weights with small random values to avoid dead-zero state.
    // In production, these will be overwritten by loadLayerWeights().
    auto rand_float = []() -> float {
        return (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 0.02f; // [-0.01, 0.01]
    };

    weight_in_proj_.resize(d_model_ * 2 * d_inner_);
    weight_out_proj_.resize(d_model_ * d_inner_);  // [d_model, d_inner] row-major
    weight_conv1d_.resize(d_inner_ * d_conv_);
    dA_.resize(d_inner_ * d_state_);
    dB_.resize(d_inner_ * d_state_);
    C_.resize(d_inner_ * d_state_);
    D_.resize(d_inner_);

    for (auto& v : weight_in_proj_)  v = rand_float();
    for (auto& v : weight_out_proj_) v = rand_float();
    for (auto& v : weight_conv1d_)  v = rand_float();
    for (auto& v : dA_)             v = rand_float();
    for (auto& v : dB_)             v = rand_float();
    for (auto& v : C_)              v = rand_float();
    for (auto& v : D_)              v = rand_float();

    state_h_.assign(d_inner_ * d_state_, 0.0f);
    conv_buffer_.assign(d_inner_ * d_conv_, 0.0f);

    tmp_proj_out_.resize(2 * d_inner_);
    tmp_conv_out_.resize(d_inner_);
    tmp_ssm_out_.resize(d_inner_);
    tmp_out_vec_.resize(d_model_);
}

void MambaLayer::reset_state() {
    std::fill(state_h_.begin(),    state_h_.end(),    0.0f);
    std::fill(conv_buffer_.begin(), conv_buffer_.end(), 0.0f);
    conv_pos_ = 0;
}

void MambaLayer::copy_state_from(const MambaLayer& other) {
  std::memcpy(state_h_.data(), other.state_h_.data(), state_h_.size() * sizeof(float));
  std::memcpy(conv_buffer_.data(), other.conv_buffer_.data(), conv_buffer_.size() * sizeof(float));
  conv_pos_ = other.conv_pos_;
}

MambaLayer::LayerState MambaLayer::getState() const {
  LayerState s;
  s.state_h = state_h_;
  s.conv_buffer = conv_buffer_;
  s.conv_pos = conv_pos_;
  return s;
}

bool MambaLayer::setState(const LayerState& s) {
  if (static_cast<int64_t>(s.state_h.size()) != d_inner_ * d_state_) return false;
  if (static_cast<int64_t>(s.conv_buffer.size()) != d_inner_ * d_conv_) return false;
  if (s.conv_pos < 0 || s.conv_pos >= d_conv_) return false;
  std::memcpy(state_h_.data(), s.state_h.data(), s.state_h.size() * sizeof(float));
  std::memcpy(conv_buffer_.data(), s.conv_buffer.data(), s.conv_buffer.size() * sizeof(float));
  conv_pos_ = s.conv_pos;
  int8_weights_ready_ = false;
  return true;
}

void MambaLayer::setQuantizationMode(QuantMode mode) {
    quant_mode_ = mode;
    int8_weights_ready_ = false;
}

void MambaLayer::prepareInt8Weights() const {
    if (int8_weights_ready_) return;
    auto quantize = [](const std::vector<float>& src, std::vector<int8_t>& dst,
                       std::vector<float>& scale) {
        dst.resize(src.size());
        float min_val = *std::min_element(src.begin(), src.end());
        float max_val = *std::max_element(src.begin(), src.end());
        float abs_max = std::max(std::abs(min_val), std::abs(max_val));
        if (abs_max < 1e-6f) abs_max = 1.0f;
        float s = 127.0f / abs_max;
        for (size_t i = 0; i < src.size(); ++i) {
            dst[i] = static_cast<int8_t>(std::clamp(static_cast<int>(std::lround(src[i] * s)), -127, 127));
        }
        scale.assign(1, 1.0f / s);
    };

    quantize(weight_in_proj_,  weight_in_proj_q8_,  scale_in_proj_);
    quantize(weight_out_proj_, weight_out_proj_q8_, scale_out_proj_);
    quantize(weight_conv1d_,  weight_conv1d_q8_,  scale_conv1d_);
    quantize(dA_,             dA_q8_,             scale_dA_);
    quantize(dB_,             dB_q8_,             scale_dB_);
    quantize(C_,              C_q8_,              scale_C_);

    int8_weights_ready_ = true;
}

float MambaLayer::processSampleInt8(float sample) {
    if (!int8_weights_ready_) prepareInt8Weights();

    float* proj_out = tmp_proj_out_.data();
    for (int64_t i = 0; i < 2 * d_inner_; ++i) {
        proj_out[i] = static_cast<float>(weight_in_proj_q8_[i]) * scale_in_proj_[0];
    }
    proj_out[0] = weight_in_proj_[0] * sample;

    const float* x_branch = proj_out;
    const float* z_branch = proj_out + d_inner_;

    float* conv_out = tmp_conv_out_.data();
    for (int64_t i = 0; i < d_inner_; ++i) {
        const int64_t base = i * d_conv_;
        conv_buffer_[base + conv_pos_] = x_branch[i];

        float val = 0.0f;
        for (int64_t k = 0; k < d_conv_; ++k) {
            val += conv_buffer_[base + (conv_pos_ + k) % d_conv_] *
                   (static_cast<float>(weight_conv1d_q8_[base + k]) * scale_conv1d_[0]);
        }
        conv_out[i] = silu(val);
    }
    conv_pos_ = (conv_pos_ + 1) % d_conv_;

    float* ssm_out = tmp_ssm_out_.data();
    for (int64_t i = 0; i < d_inner_; ++i) {
        const int64_t base = i * d_state_;
        const float x_t = conv_out[i];
        float y_t = 0.0f;
        for (int64_t j = 0; j < d_state_; ++j) {
            int64_t idx = base + j;
            state_h_[idx] = (static_cast<float>(dA_q8_[idx]) * scale_dA_[0] * context_scale_) * state_h_[idx]
                + (static_cast<float>(dB_q8_[idx]) * scale_dB_[0]) * x_t;
            if (!std::isfinite(state_h_[idx])) state_h_[idx] = 0.0f;
            if (std::abs(state_h_[idx]) < 1e-30f) state_h_[idx] = 0.0f;
            y_t += (static_cast<float>(C_q8_[idx]) * scale_C_[0]) * state_h_[idx];
        }
        ssm_out[i] = y_t + D_[i] * x_t;
        if (!std::isfinite(ssm_out[i])) ssm_out[i] = 0.0f;
    }

    float output = 0.0f;
    for (int64_t i = 0; i < d_inner_; ++i) {
        ssm_out[i] *= silu(z_branch[i]);
        output += weight_out_proj_[i] * ssm_out[i];
    }

    if (!std::isfinite(output)) output = 0.0f;
    return output;
}

float MambaLayer::processSample(float sample) {
    if (quant_mode_ != QuantMode::FP32) return processSampleInt8(sample);

    float* proj_out = tmp_proj_out_.data();
    int64_t idx = 0;

#if defined(__AVX__) || defined(__AVX2__)
    __m256 sample_vec = _mm256_set1_ps(sample);
    for (; idx + 7 < 2 * d_inner_; idx += 8) {
        __m256 w = _mm256_loadu_ps(&weight_in_proj_[idx]);
        _mm256_storeu_ps(&proj_out[idx], _mm256_mul_ps(w, sample_vec));
    }
#endif
    for (; idx < 2 * d_inner_; ++idx) {
        proj_out[idx] = weight_in_proj_[idx] * sample;
    }

    const float* x_branch = proj_out;
    const float* z_branch = proj_out + d_inner_;

    float* conv_out = tmp_conv_out_.data();
    for (int64_t i = 0; i < d_inner_; ++i) {
        const int64_t base = i * d_conv_;
        conv_buffer_[base + conv_pos_] = x_branch[i];

        float val = 0.0f;
        for (int64_t k = 0; k < d_conv_; ++k)
            val += conv_buffer_[base + (conv_pos_ + k) % d_conv_] * weight_conv1d_[base + k];
        conv_out[i] = silu(val);
    }
    conv_pos_ = (conv_pos_ + 1) % d_conv_;

    float* ssm_out = tmp_ssm_out_.data();
    for (int64_t i = 0; i < d_inner_; ++i) {
        const int64_t base = i * d_state_;
        const float x_t = conv_out[i];
        float y_t = 0.0f;
        int64_t j = 0;

#if defined(__AVX__) || defined(__AVX2__)
        __m256 y_acc = _mm256_setzero_ps();
        __m256 x_vec = _mm256_set1_ps(x_t);
        const __m256 denormal_cutoff = _mm256_set1_ps(1e-30f);
        const __m256 zero_vec = _mm256_setzero_ps();
        __m256 scale_vec = _mm256_set1_ps(context_scale_);

        for (; j + 7 < d_state_; j += 8) {
            __m256 state = _mm256_loadu_ps(&state_h_[base + j]);
            __m256 da = _mm256_mul_ps(_mm256_loadu_ps(&dA_[base + j]), scale_vec);
            __m256 db = _mm256_loadu_ps(&dB_[base + j]);
            __m256 c = _mm256_loadu_ps(&C_[base + j]);

            __m256 new_state = _mm256_add_ps(_mm256_mul_ps(da, state), _mm256_mul_ps(db, x_vec));
            __m256 abs_mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
            __m256 abs_new_state = _mm256_and_ps(new_state, abs_mask);
            __m256 mask = _mm256_cmp_ps(abs_new_state, denormal_cutoff, _CMP_LT_OQ);
            new_state = _mm256_blendv_ps(new_state, zero_vec, mask);
            _mm256_storeu_ps(&state_h_[base + j], new_state);

            y_acc = _mm256_add_ps(y_acc, _mm256_mul_ps(c, new_state));
        }

        alignas(32) float temp[8];
        _mm256_store_ps(temp, y_acc);
        for(int k=0; k<8; ++k) y_t += temp[k];
#endif

        for (; j < d_state_; ++j) {
            state_h_[base + j] = (dA_[base + j] * context_scale_) * state_h_[base + j] + dB_[base + j] * x_t;
            if (!std::isfinite(state_h_[base + j])) state_h_[base + j] = 0.0f;
            if (std::abs(state_h_[base + j]) < 1e-30f) state_h_[base + j] = 0.0f;
            y_t += C_[base + j] * state_h_[base + j];
        }
        ssm_out[i] = y_t + D_[i] * x_t;
        if (!std::isfinite(ssm_out[i])) ssm_out[i] = 0.0f;
    }

    float output = 0.0f;
    for (int64_t i = 0; i < d_inner_; ++i) {
        ssm_out[i] *= silu(z_branch[i]);
        output += weight_out_proj_[i] * ssm_out[i];
    }

    if (!std::isfinite(output)) output = 0.0f;
    return output;
}

// RT-Safety Justification: processVector() runs on the audio/inference thread.
// - No heap allocations (all buffers pre-allocated as class members).
// - No mutexes, locks, or blocking syscalls.
// - No file I/O or logging.
// - NaN/Inf sanitization after SSM recurrence protects against divergent state
//   from bad weights or numerical overflow. Zeroing is safe: downstream DSP
//   treats zero as silence, and EffectDecoder sigmoid maps zero logit to 0.5.

void MambaLayer::processVector(const float* input, float* output) noexcept {
    float* proj_out = tmp_proj_out_.data();

    // in_proj GEMV: proj_out[i] = sum_j(W_T[j, i] * input[j])
    // weight_in_proj_ stores W^T (shape [d_model, 2*d_inner] row-major)
    for (int64_t i = 0; i < 2 * d_inner_; ++i)
        proj_out[i] = 0.f;
    for (int64_t j = 0; j < d_model_; ++j) {
        const float x_j = input[j];
        const float* row = weight_in_proj_.data() + j * 2 * d_inner_;
        for (int64_t i = 0; i < 2 * d_inner_; ++i)
            proj_out[i] += row[i] * x_j;
    }

    const float* x_branch = proj_out;
    const float* z_branch = proj_out + d_inner_;

    // depthwise conv1d
    float* conv_out = tmp_conv_out_.data();
    for (int64_t i = 0; i < d_inner_; ++i) {
        const int64_t base = i * d_conv_;
        conv_buffer_[base + conv_pos_] = x_branch[i];
        float val = 0.f;
        for (int64_t k = 0; k < d_conv_; ++k)
            val += conv_buffer_[base + (conv_pos_ + k) % d_conv_] * weight_conv1d_[base + k];
        conv_out[i] = silu(val);
    }
    conv_pos_ = (conv_pos_ + 1) % d_conv_;

    // SSM recurrence
    float* ssm_out = tmp_ssm_out_.data();
    for (int64_t i = 0; i < d_inner_; ++i) {
        const int64_t base = i * d_state_;
        const float x_t = conv_out[i];
        float y_t = 0.f;
        for (int64_t j = 0; j < d_state_; ++j) {
            const int64_t idx = base + j;
            state_h_[idx] = (dA_[idx] * context_scale_) * state_h_[idx] + dB_[idx] * x_t;
            if (!std::isfinite(state_h_[idx])) state_h_[idx] = 0.f;
            if (std::abs(state_h_[idx]) < 1e-30f) state_h_[idx] = 0.f;
            y_t += C_[idx] * state_h_[idx];
        }
        ssm_out[i] = y_t + D_[i] * x_t;
        if (!std::isfinite(ssm_out[i])) ssm_out[i] = 0.f;
    }

    // gate: ssm_out *= silu(z_branch)
    for (int64_t i = 0; i < d_inner_; ++i)
        ssm_out[i] *= silu(z_branch[i]);

    // out_proj GEMV: output[i] = sum_j(W[i, j] * ssm_out[j])
    // weight_out_proj_ stores W row-major [d_model, d_inner]
    for (int64_t i = 0; i < d_model_; ++i) {
        float v = 0.f;
        const float* row = weight_out_proj_.data() + i * d_inner_;
        for (int64_t j = 0; j < d_inner_; ++j)
            v += row[j] * ssm_out[j];
        output[i] = v;
    }
}

void MambaLayer::processBlock(const float* input, float* output, int64_t num_samples) {
    for (int64_t t = 0; t < num_samples; ++t)
        output[t] = processSample(input[t]);
}

bool MambaLayer::loadLayerWeights(const float* in_proj,  size_t in_proj_n,
                                   const float* out_proj, size_t out_proj_n,
                                   const float* conv1d,   size_t conv1d_n,
                                   const float* dA,       size_t dA_n,
                                   const float* dB,       size_t dB_n,
                                   const float* C,        size_t C_n,
                                   const float* D,        size_t D_n) {
    const size_t exp_in_proj  = static_cast<size_t>(d_model_ * 2 * d_inner_);
    const size_t exp_out_proj = static_cast<size_t>(d_model_ * d_inner_);
    const size_t exp_conv1d   = static_cast<size_t>(d_inner_ * d_conv_);
    const size_t exp_ssm      = static_cast<size_t>(d_inner_ * d_state_);
    const size_t exp_D        = static_cast<size_t>(d_inner_);

    if (in_proj_n  != exp_in_proj  || out_proj_n != exp_out_proj ||
        conv1d_n   != exp_conv1d   ||
        dA_n       != exp_ssm      || dB_n != exp_ssm || C_n != exp_ssm ||
        D_n        != exp_D) {
        return false;
    }

    std::memcpy(weight_in_proj_.data(),  in_proj,  in_proj_n  * sizeof(float));
    std::memcpy(weight_out_proj_.data(), out_proj, out_proj_n * sizeof(float));
    std::memcpy(weight_conv1d_.data(),   conv1d,   conv1d_n   * sizeof(float));
    std::memcpy(dA_.data(), dA, dA_n * sizeof(float));
    std::memcpy(dB_.data(), dB, dB_n * sizeof(float));
    std::memcpy(C_.data(),  C,  C_n  * sizeof(float));
    std::memcpy(D_.data(),  D,  D_n  * sizeof(float));

    int8_weights_ready_ = false;
    return true;
}
