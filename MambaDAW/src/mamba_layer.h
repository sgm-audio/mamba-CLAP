#ifndef MAMBA_LAYER_H
#define MAMBA_LAYER_H

#include <vector>
#include <cstdint>
#include <cmath>

/**
 * @brief Custom recurrent SSM prototype; not upstream Mamba or Mamba-3.
 *
 * Status: prototype / in progress. The FP32 processing path reuses buffers
 * allocated during construction, but no full plug-in allocation or deadline
 * audit has been performed. Do not describe this class as hard-real-time safe.
 *
 * The recurrence uses learned, input-independent dA, dB, and C arrays. It is
 * not the input-dependent selective recurrence of upstream Mamba/Mamba-3.
 *
 * Weight loading (loadLayerWeights) must only be called on a non-audio thread.
 * The experimental INT8 path lazily allocates quantized buffers on first use
 * and must not be entered from an audio callback as currently implemented.
 */

enum class QuantMode {
    FP32, ///< Full-precision float (default)
    INT8, ///< Experimental; first use lazily allocates and is not RT-safe.
    INT4  ///< Not implemented.
};

class MambaLayer {
public:
    MambaLayer(int64_t d_model, int64_t d_state = 16,
               int64_t d_conv = 4, int64_t expand = 2);

    // -------------------------------------------------------------------------
    // Processing methods: buffer reuse is intended, but hard-RT safety is unverified.
    // -------------------------------------------------------------------------

    /** Process a single sample through the SSM step (d_model=1 path). */
    float processSample(float sample);

    /** Process a d_model-dim vector through one SSM step (d_model>1 path). */
    void processVector(const float* input, float* output) noexcept;

    /** Process a block: calls processSample() sequentially (d_model=1 only). */
    void processBlock(const float* input, float* output, int64_t num_samples);

    // -------------------------------------------------------------------------
    // Non-RT: state and weight management
    // -------------------------------------------------------------------------

  /** Zero all hidden states (h_t) and convolution ring buffer. */
  void reset_state();

  void copy_state_from(const MambaLayer& other);

  struct LayerState {
    std::vector<float> state_h;
    std::vector<float> conv_buffer;
    int64_t conv_pos;
  };

  LayerState getState() const;
  bool setState(const LayerState& s);

    /** Scale the state decay matrix (dA) dynamically. 1.0 = normal, < 1.0 = shorter context. */
    void setContextScale(float scale) { context_scale_ = scale; }

    /**
     * @brief Load layer weights from flat float arrays.
     *
     * Validates expected sizes against architecture before copying.
     * Must be called on a non-RT thread.
     *
     * @param in_proj   [d_model * 2 * d_inner] input projection weights
     * @param out_proj  [d_inner]                output projection weights
     * @param conv1d    [d_inner * d_conv]        depthwise conv1d weights
     * @param dA        [d_inner * d_state]       discrete A matrix
     * @param dB        [d_inner * d_state]       discrete B matrix
     * @param C         [d_inner * d_state]       C output matrix
     * @param D         [d_inner]                 skip connection weights
     * @return true if all sizes matched and weights were copied.
     */
    bool loadLayerWeights(const float* in_proj,  size_t in_proj_n,
                           const float* out_proj, size_t out_proj_n,
                           const float* conv1d,   size_t conv1d_n,
                           const float* dA,       size_t dA_n,
                           const float* dB,       size_t dB_n,
                           const float* C,        size_t C_n,
                           const float* D,        size_t D_n);

    // -------------------------------------------------------------------------
    // Quantization API (Non-RT)
    // -------------------------------------------------------------------------

    void setQuantizationMode(QuantMode mode);
    QuantMode getQuantizationMode() const { return quant_mode_; }

    // -------------------------------------------------------------------------
    // Architecture accessors (const, no allocation)
    // -------------------------------------------------------------------------
    int64_t dModel()  const { return d_model_; }
    int64_t dState()  const { return d_state_; }
    int64_t dConv()   const { return d_conv_;  }
    int64_t expand()  const { return expand_;  }
    int64_t dInner()  const { return d_inner_; }

private:
    int64_t d_model_;
    int64_t d_state_;
    int64_t d_conv_;
    int64_t expand_;
    int64_t d_inner_;

    // --- FP32 weights (always kept as master copy) ---
    std::vector<float> weight_in_proj_;   // [d_model * 2 * d_inner]
    std::vector<float> weight_out_proj_;  // [d_inner]
    std::vector<float> weight_conv1d_;    // [d_inner * d_conv]
    std::vector<float> dA_;              // [d_inner * d_state]
    std::vector<float> dB_;              // [d_inner * d_state]
    std::vector<float> C_;               // [d_inner * d_state]
    std::vector<float> D_;               // [d_inner]

    // --- INT8 quantized weights (optional, populated on demand) ---
    QuantMode quant_mode_ = QuantMode::FP32;
    mutable bool int8_weights_ready_ = false; // lazy path allocates on first use
    mutable std::vector<int8_t>  weight_in_proj_q8_;
    mutable std::vector<int8_t>  weight_out_proj_q8_;
    mutable std::vector<int8_t>  weight_conv1d_q8_;
    mutable std::vector<int8_t>  dA_q8_;
    mutable std::vector<int8_t>  dB_q8_;
    mutable std::vector<int8_t>  C_q8_;
    // Per-channel scale factors for dequantization
    mutable std::vector<float> scale_in_proj_;
    mutable std::vector<float> scale_out_proj_;
    mutable std::vector<float> scale_conv1d_;
    mutable std::vector<float> scale_dA_;
    mutable std::vector<float> scale_dB_;
    mutable std::vector<float> scale_C_;

    // --- Runtime state (persistent across processSample calls) ---
    std::vector<float> state_h_;         // [d_inner * d_state]  — the SSM memory
    std::vector<float> conv_buffer_;     // [d_inner * d_conv]   — causal conv ring buf
    int64_t conv_pos_;

    // Pre-allocated temporaries — zero RT heap allocation
    mutable std::vector<float> tmp_proj_out_; // [2 * d_inner]
    mutable std::vector<float> tmp_conv_out_; // [d_inner]
    mutable std::vector<float> tmp_ssm_out_;  // [d_inner]
    mutable std::vector<float> tmp_out_vec_;  // [d_model]  — processVector output scratch

    // --- Quantized helpers ---
    void prepareInt8Weights() const;   // lazy: one-shot FP32->INT8 conversion
    float processSampleInt8(float sample);

    float context_scale_ = 1.0f;

    inline float sigmoid(float x) const { return 1.0f / (1.0f + std::exp(-x)); }
    inline float silu(float x)    const { return x * sigmoid(x); }
};

#endif // MAMBA_LAYER_H
