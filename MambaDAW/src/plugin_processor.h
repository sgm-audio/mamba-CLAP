#ifndef PLUGIN_PROCESSOR_H
#define PLUGIN_PROCESSOR_H

#include "audio_frontend.h"
#include "mamba_model.h"
#include "model_loader.h"
#include <torch/torch.h>
#include <atomic>
#include <mutex>

class MambaProcessor {
public:
    MambaProcessor(int sample_rate = 44100);
    ~MambaProcessor();

    // Process a block of audio
    void processBlock(float** inputs, float** outputs, int32_t num_frames, int32_t num_input_channels, int32_t num_output_channels);
    
    // Activate/deactivate (called by host)
    void activate(double sample_rate, uint32_t min_frame_count, uint32_t max_frame_count);
    void deactivate();
    
    // Parameter handling
    void setParamValue(uint32_t param_id, double value);
    double getParamValue(uint32_t param_id) const;
    
    // Bypass state
    bool isBypassed() const;
    void setBypass(bool bypass);
    
    // Model loading
    bool loadModel(const std::string& path);
    
private:
    // Audio processing components
    std::unique_ptr<AudioFrontend> audio_frontend_;
    std::unique_ptr<MambaModel> mamba_model_;
    ModelLoader model_loader_;
    
    // Audio format
    int sample_rate_;
    
    // Parameters (thread-safe)
    std::atomic<float> dry_wet_mix_;      // 0.0 = dry only, 1.0 = wet only
    std::atomic<bool> bypass_;            // Bypass state
    std::atomic<int> model_type_;         // Model selection (for future use)
    
    // Internal state
    bool model_loaded_;
    bool activated_;
    
    // For residual connection in model (if needed)
    torch::Tensor residual_buffer_;
    
    // Smoothing for parameters
    float smoothed_dry_wet_;
    const float smoothing_coeff_ = 0.1f;  // Adjust for desired smoothing time
};

#endif // PLUGIN_PROCESSOR_H