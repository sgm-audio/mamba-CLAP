#ifndef MAMBA_MODEL_H
#define MAMBA_MODEL_H

#include "mamba_layer.h"
#include <vector>
#include <memory>
#include <cstdint>

/**
 * @brief Custom stack of recurrent SSM prototype layers.
 *
 * This is not the upstream selective Mamba/Mamba-3 implementation. The FP32
 * hot path is designed to reuse preallocated buffers, but hard-real-time safety
 * has not been demonstrated for a complete plug-in or target host.
 *
 * Layer state carries the recurrence between calls; effective audio memory has
 * not been measured. Reset semantics and callback safety must be tested before
 * this model is connected to a shipping audio thread.
 *
 * Thread-domain tags are intended-call-site notes from ADR-002, not an RT audit:
 *   [Audio] — proposed audio-thread path; hard-real-time safety is unverified
 *   [Main]  — intended main-thread lifecycle/configuration path
 *   [Any]   — intended cross-thread path; synchronization must be reviewed
 */
class MambaModel {
public:
    MambaModel(int64_t d_model, int64_t n_layers = 4,          // [Main]
        int64_t d_state = 16, int64_t d_conv = 4, int64_t expand = 2);
    ~MambaModel();                                              // [Main]

    // -------------------------------------------------------------------------
    // Proposed audio-thread processing path — not RT-audited  [Audio]
    // -------------------------------------------------------------------------

    /** Run one recurrent SSM step (d_model=1 scalar path). */   // [Audio]
    float processSample(float sample);

    /** Run one recurrent SSM step on a d_model-dim input vector. */ // [Audio]
    void processFrame(const float* input, float* output) noexcept;

    /** Process a contiguous block: calls processSample() per sample (d_model=1 only). */ // [Audio]
    void processBlock(const float* input, float* output, int64_t num_samples);

    // -------------------------------------------------------------------------
    // Non-RT: state and weight management  [Main]
    // -------------------------------------------------------------------------

    /** Zero all hidden states across all layers. */             // [Main]
    void resetState();

    /** Copy runtime state from another model. */               // [Main]
    void copyStateFrom(const MambaModel& other);

    struct ModelState {
        std::vector<MambaLayer::LayerState> layers;
    };

    ModelState getState() const;                                // [Main] — allocates
    bool setState(const ModelState& s);                         // [Main] — may allocate

    void setContextScale(float scale);                          // [Audio] — writes atomic

    /** Load weights into a specific layer. */                  // [Main] — allocates, copies
    bool loadLayerWeights(int64_t layer_idx,
                          const float* in_proj,  size_t in_proj_n,
                          const float* out_proj, size_t out_proj_n,
                          const float* conv1d,   size_t conv1d_n,
                          const float* dA,       size_t dA_n,
                          const float* dB,       size_t dB_n,
                          const float* C,        size_t C_n,
                          const float* D,        size_t D_n);

    // Architecture accessors  [Any]
    // -------------------------------------------------------------------------
    int64_t numLayers() const { return n_layers_; }
    int64_t dModel()    const { return d_model_;  }
    int64_t dState()    const { return d_state_;  }
    int64_t dConv()     const { return d_conv_;   }
    int64_t expand()    const { return expand_;   }

private:
    int64_t d_model_;
    int64_t n_layers_;
    int64_t d_state_;
    int64_t d_conv_;
    int64_t expand_;
    std::vector<std::unique_ptr<MambaLayer>> layers_;
    std::vector<float> tmp_frame_;  // scratch for processFrame ping-pong [d_model]
};

#endif // MAMBA_MODEL_H
