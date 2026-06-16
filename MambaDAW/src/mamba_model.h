#ifndef MAMBA_MODEL_H
#define MAMBA_MODEL_H

#include "mamba_layer.h"
#include <vector>
#include <memory>
#include <cstdint>

/**
 * @brief CPU-Optimized, State-Centric Mamba Model.
 *
 * Owns a stack of MambaLayers. Provides sample-by-sample processing
 * that is real-time safe (no heap allocation in hot paths).
 *
 * The hidden state across all layers IS the plugin's memory.
 * Never reset state from the RT thread.
 *
 * Thread-domain tags per ADR-002:
 *   [Audio] — hard RT, called from process()
 *   [Main]  — main thread, may allocate/do I/O
 *   [Any]   — safe from any thread
 */
class MambaModel {
public:
    MambaModel(int64_t d_model, int64_t n_layers = 4,          // [Main]
        int64_t d_state = 16, int64_t d_conv = 4, int64_t expand = 2);
    ~MambaModel();                                              // [Main]

    // -------------------------------------------------------------------------
    // RT-safe audio processing  [Audio]
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
