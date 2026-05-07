#include "plugin_processor.h"
#include <iostream>
#include <cmath>

MambaProcessor::MambaProcessor(int sample_rate)
    : sample_rate_(sample_rate),
      dry_wet_mix_(0.5f),  // Default 50% wet
      bypass_(false),
      model_type_(0),
      model_loaded_(false),
      activated_(false),
      smoothed_dry_wet_(0.5f) {
    
    // Initialize audio frontend
    audio_frontend_ = std::make_unique<AudioFrontend>(2048, 512, sample_rate_);
    
    // Initialize Mamba model (will be replaced by loaded model)
    // Using a small model for testing
    mamba_model_ = std::make_unique<MambaModel>(80, 2, 16, 4, 2);  // 80 mel bins, 2 layers
}

MambaProcessor::~MambaProcessor() = default;

void MambaProcessor::activate(double sample_rate, uint32_t min_frame_count, uint32_t max_frame_count) {
    sample_rate_ = static_cast<int>(sample_rate);
    activated_ = true;
    
    // Recreate frontend with new sample rate if needed
    audio_frontend_ = std::make_unique<AudioFrontend>(2048, 512, sample_rate_);
    
    std::cout << "MambaProcessor activated at " << sample_rate_ << " Hz" << std::endl;
}

void MambaProcessor::deactivate() {
    activated_ = false;
    // Reset state
    audio_frontend_->reset();
    if (mamba_model_) {
        mamba_model_->reset_state(1, 1);  // Reset to minimal state
    }
    std::cout << "MambaProcessor deactivated" << std::endl;
}

void MambaProcessor::processBlock(float** inputs, float** outputs, int32_t num_frames, int32_t num_input_channels, int32_t num_output_channels) {
    // If not activated or bypassed, just copy input to output
    if (!activated_ || bypass_.load() || !model_loaded_) {
        for (int32_t c = 0; c < std::min(num_input_channels, num_output_channels); ++c) {
            for (int32_t i = 0; i < num_frames; ++i) {
                outputs[c][i] = inputs[c][i];
            }
        }
        // Zero out any extra output channels
        for (int32_t c = num_input_channels; c < num_output_channels; ++c) {
            for (int32_t i = 0; i < num_frames; ++i) {
                outputs[c][i] = 0.0f;
            }
        }
        return;
    }
    
    // Process each sample through the frontend
    // We'll process in blocks that match the frontend's hop size
    int processed = 0;
    while (processed < num_frames) {
        // How many samples can we process in this iteration?
        int to_process = std::min(hop_size_, num_frames - processed);
        
        // Accept input samples
        if (num_input_channels > 0) {
            // For simplicity, we'll process only the first channel
            // In a stereo plugin, we'd process both channels separately or together
            audio_frontend_->acceptInput(inputs[0] + processed, to_process);
        }
        
        // Check if we have features to process
        if (audio_frontend_->isReadyForOutput()) {
            // Get features from frontend
            torch::Tensor features = audio_frontend_->getFeatures();  // [1, num_mel_bins]
            
            // Run through Mamba model
            torch::Tensor processed_features = mamba_model_->forward(features);  // [1, num_mel_bins]
            
            // Send processed features back to frontend for iSTFT
            audio_frontend_->setModelOutput(processed_features);
        }
        
        // Get output samples from frontend
        if (num_output_channels > 0) {
            float* out_buffer = outputs[0] + processed;
            audio_frontend_->getOutput(out_buffer, to_process);
        }
        
        processed += to_process;
    }
    
    // Apply dry/wet mix with smoothing
    float target_wet = dry_wet_mix_.load();
    smoothed_dry_wet_ = smoothed_dry_wet_ * (1.0f - smoothing_coeff_) + target_wet * smoothing_coeff_;
    float wet = smoothed_dry_wet_;
    float dry = 1.0f - wet;
    
    // Mix dry and wet signals (assuming we processed the first channel)
    if (num_input_channels > 0 && num_output_channels > 0) {
        for (int32_t i = 0; i < num_frames; ++i) {
            float dry_sample = inputs[0][i];  // Original input
            float wet_sample = outputs[0][i]; // Processed output
            outputs[0][i] = dry * dry_sample + wet * wet_sample;
        }
    }
    
    // Copy first channel to other output channels if needed
    for (int32_t c = 1; c < num_output_channels; ++c) {
        for (int32_t i = 0; i < num_frames; ++i) {
            outputs[c][i] = outputs[0][i];
        }
    }
}

void MambaProcessor::setParamValue(uint32_t param_id, double value) {
    switch (param_id) {
        case 0:  // Dry/Wet Mix
            dry_wet_mix_.store(static_cast<float>(value));
            break;
        case 1:  // Model Type
            model_type_.store(static_cast<int>(value));
            break;
        case 2:  // Bypass
            bypass_.store(static_cast<bool>(value));
            break;
        default:
            break;
    }
}

double MambaProcessor::getParamValue(uint32_t param_id) const {
    switch (param_id) {
        case 0:  // Dry/Wet Mix
            return static_cast<double>(dry_wet_mix_.load());
        case 1:  // Model Type
            return static_cast<double>(model_type_.load());
        case 2:  // Bypass
            return static_cast<double>(bypass_.load());
        default:
            return 0.0;
    }
}

bool MambaProcessor::isBypassed() const {
    return bypass_.load();
}

void MambaProcessor::setBypass(bool bypass) {
    bypass_.store(bypass);
}

bool MambaProcessor::loadModel(const std::string& path) {
    bool success = model_loader_.loadModel(path);
    if (success) {
        model_loaded_ = true;
        std::cout << "Model loaded successfully from " << path << std::endl;
        // We could replace our internal mamba_model_ with the loaded one here
        // For now, we'll just use the model loader for inference
    } else {
        model_loaded_ = false;
        std::cerr << "Failed to load model from " << path << std::endl;
    }
    return success;
}