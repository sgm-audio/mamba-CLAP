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
 */
class MambaModel {
public:
    MambaModel(int64_t d_model, int64_t n_layers = 4,
               int64_t d_state = 16, int64_t d_conv = 4, int64_t expand = 2);
    ~MambaModel();

    // -------------------------------------------------------------------------
    // RT-safe audio processing
    // -------------------------------------------------------------------------

    /** Run one recurrent SSM step. Updates all layer hidden states. */
    float processSample(float sample);

    /** Process a contiguous block: calls processSample() per sample. */
    void processBlock(const float* input, float* output, int64_t num_samples);

    // -------------------------------------------------------------------------
    // Non-RT: state and weight management
    // -------------------------------------------------------------------------

    /** Zero all hidden states across all layers. Non-RT only. */
    void resetState();

    /** Copy runtime state (hidden + conv + pos) from another model. */
    void copyStateFrom(const MambaModel& other);

    void setContextScale(float scale);

    /**
     * @brief Load weights into a specific layer from flat float arrays.
     *
     * Delegates to MambaLayer::loadLayerWeights(). Non-RT only.
     *
     * @param layer_idx Layer index in [0, n_layers).
     * @return false if layer_idx out of range or size mismatch.
     */
    bool loadLayerWeights(int64_t layer_idx,
                          const float* in_proj,  size_t in_proj_n,
                          const float* out_proj, size_t out_proj_n,
                          const float* conv1d,   size_t conv1d_n,
                          const float* dA,       size_t dA_n,
                          const float* dB,       size_t dB_n,
                          const float* C,        size_t C_n,
                          const float* D,        size_t D_n);

    // -------------------------------------------------------------------------
    // Architecture accessors
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
};

#endif // MAMBA_MODEL_H
