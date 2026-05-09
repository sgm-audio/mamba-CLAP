#include "clap-plugin.hpp"
#include <cstring>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <fftw3.h>

namespace fs = std::filesystem;
namespace mamba_audio {

static const clap_plugin_descriptor kDescriptor = {
    .clap_version = CLAP_VERSION,
    .id = "com.mamba-audio.mambafx",
    .name = "MambaFX",
    .vendor = "Mamba Audio",
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = "1.0.0",
    .min_api_version = CLAP_VERSION_INIT(1, 0),
    .description = "Audio effect using Mamba SSM for context-aware processing",
    .features = (const char*[]){
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_UTILITY,
        nullptr
    }
};

// Helper function to compute mel scale
static std::vector<float> mel_scale(int n_mels, int n_fft, double sample_rate) {
    std::vector<float> mel_basis(n_mels * (n_fft/2 + 1), 0.0f);
    // Center frequencies of each mel band
    double f_min = 0.0;
    double f_max = sample_rate / 2.0;
    double mel_min = 2595.0 * std::log10(1.0 + f_min / 700.0);
    double mel_max = 2595.0 * std::log10(1.0 + f_max / 700.0);
    for (int m = 0; m < n_mels; ++m) {
        double mel = mel_min + (mel_max - mel_min) * (m + 1) / (n_mels + 1);
        double f = 700.0 * (std::pow(10.0, mel / 2595.0) - 1.0);
        int f_left = static_cast<int>(std::floor(f * n_fft / sample_rate));
        int f_center = static_cast<int>(std::round(f * n_fft / sample_rate));
        int f_right = static_cast<int>(std::ceil(f * n_fft / sample_rate));
        // Triangle: up to center, then down
        for (int k = f_left; k < f_center; ++k) {
            if (k >= 0 && k < n_fft/2+1) {
                double slope = (k - f_left) / static_cast<double>(f_center - f_left);
                mel_basis[m * (n_fft/2+1) + k] = slope;
            }
        }
        for (int k = f_center; k < f_right; ++k) {
            if (k >= 0 && k < n_fft/2+1) {
                double slope = (f_right - k) / static_cast<double>(f_right - f_center);
                mel_basis[m * (n_fft/2+1) + k] = slope;
            }
        }
    }
    return mel_basis;
}

// Helper to compute pseudo-inverse of a matrix (using SVD would be better, but for simplicity we use a simple method for small matrices)
// Note: This is a naive implementation for demonstration. In production, use a proper linear algebra library.
static torch::Tensor pinverse(const torch::Tensor& mat, double eps=1e-8) {
    // mat: [n_mels, n_fft/2+1]
    // We'll compute the pseudo-inverse via: V * S^+ * U^T
    auto svd = torch::svd(mat);
    auto U = svd.U;
    auto S = svd.S;
    auto V = svd.V;
    // Invert non-zero singular values
    auto S_inv = torch::zeros_like(S);
    for (int i = 0; i < S.size(0); ++i) {
        if (S[i].item<float>() > eps) {
            S_inv[i] = 1.0 / S[i].item<float>();
        }
    }
    // Compute pseudo-inverse: V * diag(S_inv) * U^T
    auto mat_pinv = torch::mm(V, torch::mm(torch::diag(S_inv), U.t()));
    return mat_pinv;
}

MambaClapPlugin::MambaClapPlugin(const clap_plugin_descriptor* desc, const clap_host_t* host)
    : descriptor_(desc), host_(host) {
    std::cerr << "[INFO] MambaFX CLAP plugin created" << std::endl;
}

MambaClapPlugin::~MambaClapPlugin() {
    std::cerr << "[INFO] MambaFX CLAP plugin destroyed" << std::endl;
    deactivate();
}

// CLAP parameter interface
bool MambaClapPlugin::init() {
    std::cerr << "[INFO] MambaFX CLAP plugin initializing" << std::endl;
    // We could allocate resources here, but we do it in activate.
    return true;
}

bool MambaClapPlugin::activate(double sample_rate, double min_blep_latency, double max_blep_latency) {
    std::cerr << "[INFO] Activating MambaFX CLAP plugin" << std::endl;
    sample_rate_ = sample_rate;

    // Validate sample rate
    if (sample_rate_ != 22050.0 && sample_rate_ != 44100.0 && sample_rate_ != 48000.0 && sample_rate_ != 96000.0) {
        std::cerr << "[WARN] Unsupported sample rate: " << sample_rate_ << ". Defaulting to 44100.0 Hz." << std::endl;
        sample_rate_ = 44100.0; // Default to 44.1kHz
    }

    // Initialize STFT processor
    stft_processor_ = std::make_unique<STFTProcessor>(n_fft_, hop_length_, n_mels_, sample_rate_);
    std::cerr << "[INFO] STFT processor initialized with n_fft=" << n_fft_ 
              << ", hop_length=" << hop_length_ 
              << ", n_mels=" << n_mels_ 
              << ", sample_rate=" << sample_rate_ << std::endl;

    // Get model path from environment or use default
    const char* mamba_model_path = getenv("MAMBA_MODEL_PATH");
    std::string model_path;
    if (mamba_model_path == nullptr) {
        model_path = "./models/mamba3_audio_effects.pt";
    } else {
        model_path = mamba_model_path;
    }

    std::cerr << "[INFO] Loading Mamba model from: " << model_path << std::endl;

    try {
        // Initialize Mamba wrapper
        mamba_wrapper_ = std::make_unique<MambaWrapper>(model_path, n_mels_, device_);
        std::cerr << "[INFO] Mamba model loaded successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to load Mamba model: " << e.what() << std::endl;
        return false;
    }

    is_active_ = true;
    std::cerr << "[INFO] MambaFX CLAP plugin activated successfully" << std::endl;
    return true;
}

void MambaClapPlugin::deactivate() {
    if (!is_active_) {
        return;
    }
    std::cerr << "[INFO] Deactivating MambaFX CLAP plugin" << std::endl;
    is_active_ = false;
    is_processing_ = false;
    if (mamba_wrapper_) {
        mamba_wrapper_->resetState();
        std::cerr << "[INFO] Mamba wrapper state reset" << std::endl;
    }
    stft_processor_.reset();
    mamba_wrapper_.reset();
    std::cerr << "[INFO] MambaFX CLAP plugin deactivated" << std::endl;
}

bool MambaClapPlugin::start_processing() {
    if (!is_active_) {
        std::cerr << "[WARN] start_processing called but plugin is not active" << std::endl;
        return false;
    }
    std::cerr << "[INFO] Starting audio processing" << std::endl;
    is_processing_ = true;
    return true;
}

void MambaClapPlugin::stop_processing() {
    if (!is_processing_) {
        return;
    }
    std::cerr << "[INFO] Stopping audio processing" << std::endl;
    is_processing_ = false;
}

// CLAP parameter callbacks
bool MambaClapPlugin::param_value(const clap_param_t* param, double value) {
    if (strcmp(param->id, "effect_type") == 0) {
        if (value < 0.0 || value > 3.0) {
            std::cerr << "[WARN] effect_type parameter out of range [0,3]: " << value << ". Clamping." << std::endl;
        }
        param_effect_type = static_cast<float>(std::clamp(value, 0.0, 3.0));
        std::cerr << "[INFO] Parameter effect_type set to: " << param_effect_type << std::endl;
        return true;
    } else if (strcmp(param->id, "mix") == 0) {
        if (value < 0.0 || value > 1.0) {
            std::cerr << "[WARN] mix parameter out of range [0,1]: " << value << ". Clamping." << std::endl;
        }
        param_mix = static_cast<float>(std::clamp(value, 0.0, 1.0));
        std::cerr << "[INFO] Parameter mix set to: " << param_mix << std::endl;
        return true;
    } else if (strcmp(param->id, "context_length") == 0) {
        if (value < 0.0) {
            std::cerr << "[WARN] context_length parameter negative: " << value << ". Clamping to 0." << std::endl;
        }
        param_context_length = static_cast<float>(value);
        std::cerr << "[INFO] Parameter context_length set to: " << param_context_length << " seconds" << std::endl;
        return true;
    }
    std::cerr << "[WARN] Unknown parameter ID: " << param->id << std::endl;
    return false;
}

double MambaClapPlugin::param_value(const clap_param_t* param) const {
    if (strcmp(param->id, "effect_type") == 0) {
        return static_cast<double>(param_effect_type);
    } else if (strcmp(param->id, "mix") == 0) {
        return static_cast<double>(param_mix);
    } else if (strcmp(param->id, "context_length") == 0) {
        return static_cast<double>(param_context_length);
    }
    std::cerr << "[WARN] Unknown parameter ID in getter: " << param->id << std::endl;
    return 0.0;
}

void MambaClapPlugin::param_change(const clap_param_t* param, double value, uint32_t /*flags*/) {
    // We just update the value; the host should have called param_value first, but we handle it here too.
    param_value(param, value);
    // Optionally, we could smooth the parameter change here to avoid clicks.
    // For now, we just update the value and let the next block use the new value.
    std::cerr << "[INFO] Parameter changed: " << param->id << " = " << value << std::endl;
}

void MambaClapPlugin::process(const clap_process_t* process) {
    if (!is_active_ || !is_processing_ || process->num_samples == 0) {
        if (process->num_samples == 0) {
            // This is normal, just return
            return;
        }
        std::cerr << "[WARN] process() called but plugin is not active or not processing" << std::endl;
        return;
    }

    // Process each channel
    for (int c = 0; c < process->audio_inputs[0].channel_count && c < process->audio_outputs[0].channel_count; ++c) {
        const float* input = process->audio_inputs[c].data32;
        float* output = process->audio_outputs[c].data32;
        int num_samples = process->num_samples;

        // Process audio in blocks to manage latency
        processAudioBlock(input, output, num_samples);
    }
}

bool MambaClapPlugin::processAudioBlock(const float* input, float* output, int num_samples) {
    // Copy input to output first (dry signal)
    std::memcpy(output, input, num_samples * sizeof(float));

    // Skip processing if not active or no model
    if (!mamba_wrapper_) {
        return true;
    }

    try {
        torch::NoGradGuard no_grad;

        // Convert input block to tensor [1, num_samples]
        // Use clone() to ensure the tensor owns its memory and avoids lifetime issues with from_blob
        auto input_tensor = torch::from_blob(const_cast<float*>(input), {1, num_samples}, torch::kFloat32)
                                .clone().to(device_);

        // Convert to mel-spectrogram [time_steps, n_mels]
        auto mel_spec = stft_processor_->forward(input_tensor);

        // Process with Mamba - we need to process frame by frame to maintain state
        int time_steps = mel_spec.size(0);
        torch::Tensor processed_mel = torch::zeros_like(mel_spec);
        
        // Get initial state (empty vector, will be updated by step)
        std::vector<torch::Tensor> state;
        
        // Process each time step
        for (int t = 0; t < time_steps; ++t) {
            // Extract one time step [1, n_mels]
            auto step_input = mel_spec.slice(0, t, t+1);
            
            // Process this step to get Mamba output and update state
            auto step_output = mamba_wrapper_->step(step_input, state);
            
            // Sanity check: if step_output contains NaN/Inf, replace with zeros to prevent propagation
            if (!step_output.isfinite().all().item<bool>()) {
                std::cerr << "[WARN] Mamba step output non-finite, zeroing" << std::endl;
                step_output = torch::zeros_like(step_output);
            }
            
            // Get effect parameters for the current effect type
            int effect_type_idx = getEffectTypeIndex();
            torch::Tensor effect_params = mamba_wrapper_->processEffect(step_input, effect_type_idx, state);
            
            // Apply effect modulation: we use the mean of the effect parameters as a simple modulation strength
            // In a more sophisticated implementation, we would map effect_params to specific effect parameters (e.g., reverb decay)
            float effect_strength = effect_params.mean().item<float>();
            // Map effect_strength to a modulation factor (e.g., 0.5 to 1.5)
            float modulation = 1.0f + (effect_strength * 0.5f); // Simple linear mapping
            
            // Store result: apply modulation to the Mamba output
            processed_mel.slice(0, t, t+1).copy_(step_output * modulation);
        }

        // Convert back to waveform [num_samples]
        auto output_tensor = stft_processor_->inverse(processed_mel);

        // Mix processed (wet) signal with dry signal
        auto output_flat = output_tensor.contiguous().view({-1});
        auto output_ptr = output_flat.data_ptr<float>();
        
        // Apply mix level: dry * (1-mix) + wet * mix
        for (int i = 0; i < num_samples && i < output_flat.size(0); ++i) {
            float wet_sample = output_ptr[i];
            // Sanitize wet signal
            if (std::isnan(wet_sample) || std::isinf(wet_sample)) {
                wet_sample = 0.0f;
            }
            float dry_sample = output[i]; // Already contains dry signal
            float mixed = (1.0f - param_mix) * dry_sample + param_mix * wet_sample;
            // Sanitize mixed signal
            if (std::isnan(mixed) || std::isinf(mixed)) {
                mixed = 0.0f;
            }
            // Soft clipping to prevent overflow (tanh limits to [-1, 1])
            mixed = std::tanh(mixed);
            output[i] = mixed;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Audio processing error: " << e.what() << std::endl;
        // On error, output remains as dry signal (already copied)
    }

    return true;
}

bool MambaClapPlugin::applyMelSpectrogramEnhancement(torch::Tensor& mel_spec) {
    // This function is kept for backward compatibility but not used in new implementation
    try {
        if (mamba_wrapper_) {
            mel_spec = mamba_wrapper_->process(mel_spec);
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Mamba processing error: " << e.what() << std::endl;
    }
    return false;
}

STFTProcessor::STFTProcessor(int n_fft, int hop_length, int n_mels, double sample_rate)
    : n_fft_(n_fft), hop_length_(hop_length), n_mels_(n_mels), sample_rate_(sample_rate) {
    std::cerr << "[INFO] Creating STFTProcessor: n_fft=" << n_fft_ 
              << ", hop_length=" << hop_length_ 
              << ", n_mels=" << n_mels_ 
              << ", sample_rate=" << sample_rate_ << std::endl;
    
    freq_bins_ = n_fft_ / 2 + 1;
    pad_ = n_fft_ / 2;
    
    // Create Hann window (tensor and host copy)
    window_func_ = torch::hann_window(n_fft_, true);
    window_host_ = static_cast<float*>(fftwf_malloc(n_fft_ * sizeof(float)));
    auto window_access = window_func_.accessor<float, 1>();
    for (int i = 0; i < n_fft_; ++i) {
        window_host_[i] = window_access[i];
    }
    
    // Precompute mel basis and its pseudo-inverse
    std::vector<float> mel_basis_data = mel_scale(n_mels, n_fft, sample_rate);
    mel_basis_ = torch::from_blob(mel_basis_data.data(), {n_mels, freq_bins_}, torch::kFloat32).clone();
    // Compute pseudo-inverse of mel basis
    inv_mel_basis_ = pinverse(mel_basis_);
    
    // Allocate FFTW buffers
    fftw_real_buffer_ = static_cast<float*>(fftwf_malloc(n_fft_ * sizeof(float)));
    fftw_complex_buffer_ = static_cast<fftwf_complex*>(fftwf_malloc(freq_bins_ * sizeof(fftwf_complex)));
    
    // Create FFTW plans
    forward_plan_ = fftwf_plan_dft_r2c_1d(n_fft_, fftw_real_buffer_, fftw_complex_buffer_, FFTW_MEASURE);
    inverse_plan_ = fftwf_plan_dft_c2r_1d(n_fft_, fftw_complex_buffer_, fftw_real_buffer_, FFTW_MEASURE);
    
    // Pre-allocate padded buffer for center padding
    // Set to a large value to cover all expected DAW block sizes (up to 32768 samples)
    max_block_size_ = 32768;
    padded_buffer_capacity_ = max_block_size_ + n_fft_;
    padded_buffer_ = static_cast<float*>(fftwf_malloc(padded_buffer_capacity_ * sizeof(float)));
    
    // Compute sum of squared window for overlap-add compensation
    window_energy_sum_ = 0.0f;
    for (int i = 0; i < n_fft_; ++i) {
        float w = window_host_[i];
        window_energy_sum_ += w * w;
    }
    
    last_input_len_ = 0;
}

STFTProcessor::~STFTProcessor() {
    if (forward_plan_) {
        fftwf_destroy_plan(forward_plan_);
        forward_plan_ = nullptr;
    }
    if (inverse_plan_) {
        fftwf_destroy_plan(inverse_plan_);
        inverse_plan_ = nullptr;
    }
    if (fftw_real_buffer_) {
        fftwf_free(fftw_real_buffer_);
        fftw_real_buffer_ = nullptr;
    }
    if (fftw_complex_buffer_) {
        fftwf_free(fftw_complex_buffer_);
        fftw_complex_buffer_ = nullptr;
    }
    if (window_host_) {
        fftwf_free(window_host_);
        window_host_ = nullptr;
    }
    if (padded_buffer_) {
        fftwf_free(padded_buffer_);
        padded_buffer_ = nullptr;
    }
}

torch::Tensor STFTProcessor::forward(const torch::Tensor& waveform) {
    // Ensure we have a 1D tensor and get pointer to data
    auto waveform_1d = waveform.squeeze();
    if (waveform_1d.dim() != 1) {
        throw std::runtime_error("STFTProcessor::forward expects a 1D waveform");
    }
    int L = waveform_1d.size(0);
    last_input_len_ = L; // Store for inverse
    
    float* input_data = waveform_1d.data_ptr<float>();
    
    // Center padding: pad with n_fft/2 zeros on both sides
    int pad = pad_;
    int padded_len = L + n_fft_; // total length after padding
    
    // Ensure pre-allocated buffer is large enough (real-time safe)
    if (padded_len > static_cast<int>(padded_buffer_capacity_)) {
        throw std::runtime_error("STFTProcessor::forward: block size exceeds maximum supported size (" + 
                                 std::to_string(padded_len) + " > " + std::to_string(padded_buffer_capacity_) + ")");
    }
    float* padded = padded_buffer_;
    // Zero the padded buffer
    std::fill(padded, padded + padded_len, 0.0f);
    // Copy input into padded buffer starting at offset pad
    std::memcpy(padded + pad, input_data, L * sizeof(float));
    
    // Number of frames: 1 + floor(L / hop_length_)
    int num_frames = L / hop_length_ + 1;
    if (L % hop_length_ == 0) {
        // If L is exactly divisible by hop, we still get floor(L/hop) + 1, but torch might produce extra frame? 
        // We'll stick to this formula.
    }
    // Prepare output magnitude matrix (freq_bins_ x num_frames) in a torch tensor
    torch::Tensor mag_tensor = torch::zeros({freq_bins_, num_frames}, torch::kFloat32);
    float* mag_data = mag_tensor.data_ptr<float>();
    
    // Process each frame
    for (int t = 0; t < num_frames; ++t) {
        int frame_offset = t * hop_length_;
        // Copy frame data into FFTW real buffer, apply Hann window
        for (int i = 0; i < n_fft_; ++i) {
            float sample = padded[frame_offset + i];
            fftw_real_buffer_[i] = sample * window_host_[i];
        }
        // Execute forward FFT
        fftwf_execute(forward_plan_);
        // Extract magnitude and apply normalized scaling (1/√n_fft)
        float inv_sqrt_n = 1.0f / std::sqrt(static_cast<float>(n_fft_));
        for (int i = 0; i < freq_bins_; ++i) {
            float re = fftw_complex_buffer_[i].re;
            float im = fftw_complex_buffer_[i].im;
            float mag = std::sqrt(re*re + im*im) * inv_sqrt_n;
            // Store in column t: row i, col t -> index i * num_frames + t
            mag_data[i * num_frames + t] = mag;
        }
    }
    
    // If we allocated temporary padded buffer, free it
    if (padded != padded_buffer_) {
        delete[] padded;
    }
    
    // Apply mel filter bank: (n_mels, freq_bins) * (freq_bins, num_frames) -> (n_mels, num_frames)
    torch::Tensor mel = torch::mm(mel_basis_, mag_tensor);
    
    // Log compression
    torch::Tensor log_mel = torch::log(torch::clamp(mel, 1e-5, 1e5));
    
    // Transpose to [time, n_mels]
    return log_mel.transpose(0, 1);
}

torch::Tensor STFTProcessor::inverse(const torch::Tensor& mel_spectrogram) {
    // Input format: [time, mel_bins] (log-mel)
    int T = mel_spectrogram.size(0);
    if (T == 0) {
        return torch::zeros({0}, torch::kFloat32);
    }
    
    // Linear-scale mel: [time, mel_bins] -> [mel_bins, time]
    auto mel_linear = torch::exp(mel_spectrogram.transpose(0, 1));
    
    // Inverse mel to magnitude spectrogram: [n_fft/2+1, time]
    torch::Tensor mag = torch::mm(inv_mel_basis_, mel_linear);
    
    // Prepare magnitude data pointer
    float* mag_data = mag.data_ptr<float>();
    int mag_rows = mag.size(0); // freq_bins_
    int mag_cols = mag.size(1); // T
    
    // Allocate buffer for full overlap-added signal
    int full_len = (T - 1) * hop_length_ + n_fft_;
    // Ensure pre-allocated buffer is large enough (real-time safe)
    if (full_len > static_cast<int>(padded_buffer_capacity_)) {
        throw std::runtime_error("STFTProcessor::inverse: output length exceeds maximum supported buffer size");
    }
    float* full_signal = padded_buffer_;
    std::fill(full_signal, full_signal + full_len, 0.0f);
    
    // Process each frame: overlap-add windowed inverse FFT
    for (int t = 0; t < T; ++t) {
        // Set complex spectrum from magnitude (zero phase)
        for (int i = 0; i < freq_bins_; ++i) {
            float mag_val = mag_data[i * T + t]; // column-major: row i, col t
            fftw_complex_buffer_[i].re = mag_val;
            fftw_complex_buffer_[i].im = 0.0f;
        }
        // Execute inverse FFT
        fftwf_execute(inverse_plan_);
        // FFTW inverse is unscaled; apply 1/√n_fft scaling, then window, and overlap-add
        int offset = t * hop_length_;
        float scale = 1.0f / std::sqrt(static_cast<float>(n_fft_));
        for (int i = 0; i < n_fft_; ++i) {
            full_signal[offset + i] += fftw_real_buffer_[i] * scale * window_host_[i];
        }
    }
    
    // Apply overlap-add compensation: divide by sum of squared windows
    float inv_window_energy = 1.0f / window_energy_sum_;
    for (int i = 0; i < full_len; ++i) {
        full_signal[i] *= inv_window_energy;
    }
    
    // Determine cropping: original length is last_input_len_ (set during forward)
    // With center padding, the original signal is located at samples [pad_ : pad_ + last_input_len_]
    // But we must ensure full_len is at least pad_ + last_input_len_
    size_t original_len = last_input_len_;
    int pad = pad_;
    // If full_len < pad + original_len, something is wrong; we'll clamp
    int start = pad;
    int end = std::min(static_cast<int>(full_len), start + static_cast<int>(original_len));
    int copy_len = end - start;
    if (copy_len < 0) copy_len = 0;
    
    torch::Tensor output = torch::zeros({static_cast<int64_t>(original_len)}, torch::kFloat32);
    float* output_data = output.data_ptr<float>();
    if (copy_len > 0) {
        std::memcpy(output_data, full_signal + start, copy_len * sizeof(float));
    }
    if (original_len > copy_len) {
        // Zero-fill remaining (should not happen)
        std::fill(output_data + copy_len, output_data + original_len, 0.0f);
    }
    
    return output;
}

MambaWrapper::MambaWrapper(const std::string& model_path, int n_mels, torch::Device device)
    : device_(device), n_mels_(n_mels) {
    std::cerr << "[INFO] Creating MambaWrapper: n_mels=" << n_mels_ 
              << ", device=" << (device_.is_cuda() ? "cuda" : "cpu") << std::endl;
    if (fs::exists(model_path)) {
        try {
            // Load TorchScript model
            module_ = torch::jit::load(model_path);
            module_->to(device_);
            module_->eval();
            std::cerr << "[INFO] Loaded Mamba model from: " << model_path << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to load TorchScript model: " << e.what() << std::endl;
            throw;
        }
    } else {
        std::cerr << "[ERROR] Model not found at: " << model_path << std::endl;
        throw std::runtime_error("Model file not found: " + model_path);
    }
    
    // Initialize effect decoders (simple linear layers for now)
    // In a real implementation, these would be trained or loaded from the model
    reverb_decoder_ = std::make_shared<torch::nn::Linear>(n_mels, 4); // 4 reverb parameters
    compression_decoder_ = std::make_shared<torch::nn::Linear>(n_mels, 4); // 4 compression parameters
    saturation_decoder_ = std::make_shared<torch::nn::Linear>(n_mels, 2); // 2 saturation parameters
    eq_decoder_ = std::make_shared<torch::nn::Linear>(n_mels, 5); // 5 EQ bands
    
    // Initialize with small random weights
    torch::nn::init::normal_(*reverb_decoder_->weight, 0.0, 0.1);
    torch::nn::init::constant_(reverb_decoder_->bias, 0.0);
    torch::nn::init::normal_(*compression_decoder_->weight, 0.0, 0.1);
    torch::nn::init::constant_(compression_decoder_->bias, 0.0);
    torch::nn::init::normal_(*saturation_decoder_->weight, 0.0, 0.1);
    torch::nn::init::constant_(saturation_decoder_->bias, 0.0);
    torch::nn::init::normal_(*eq_decoder_->weight, 0.0, 0.1);
    torch::nn::init::constant_(eq_decoder_->bias, 0.0);
    
    std::cerr << "[INFO] Effect decoders initialized" << std::endl;
}

MambaWrapper::~MambaWrapper() {
    std::cerr << "[INFO] Destroying MambaWrapper" << std::endl;
}

std::vector<torch::Tensor> MambaWrapper::getInitialState(int batch_size) {
    // Return empty state for now - the model should handle initialization
    return {};
}

torch::Tensor MambaWrapper::step(const torch::Tensor& input, std::vector<torch::Tensor>& state) {
    // Input format: [batch, n_mels]
    torch::Tensor x = input.to(device_);
    
    // Add sequence dimension if needed [batch, 1, n_mels]
    if (x.dim() == 2) {
        x = x.unsqueeze(1);
    }

    // Prepare inputs for the model
    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(x);
    
    // Add state if we have it from previous processing
    for (const auto& state_tensor : state) {
        inputs.push_back(state_tensor);
    }

    // Run model inference
    torch::jit::IValue result = module_->forward(inputs);
    
    // Extract output and updated state
    torch::Tensor output;
    std::vector<torch::jit::IValue> new_state;
    
    if (result.isTuple()) {
        auto tuple = result.toTuple();
        output = tuple->elements()[0].toTensor();
        // Extract state tensors (everything after the first output)
        for (size_t i = 1; i < tuple->elements().size(); ++i) {
            new_state.push_back(tuple->elements()[i]);
        }
    } else {
        output = result.toTensor();
        // No state returned
    }

    // Remove sequence dimension if we added it
    if (output.dim() == 3 && output.size(1) == 1) {
        output = output.squeeze(1);
    }

    // Update state for next iteration
    state.clear();
    for (const auto& state_tensor : new_state) {
        state.push_back(state_tensor.to(torch::kCPU));
    }

    return output.to(torch::kCPU);
}

torch::Tensor MambaWrapper::processEffect(const torch::Tensor& input, int effect_type, std::vector<torch::Tensor>& state) {
    // Input format: [batch, n_mels]
    torch::Tensor x = input.to(device_);
    
    // Get the Mamba output for this step
    auto mamba_output = step(x, state);
    
    // Apply effect-specific decoder
    torch::Tensor effect_params;
    switch (effect_type) {
        case 0: // reverb
            effect_params = (*reverb_decoder_)(mamba_output);
            break;
        case 1: // compression
            effect_params = (*compression_decoder_)(mamba_output);
            break;
        case 2: // saturation
            effect_params = (*saturation_decoder_)(mamba_output);
            break;
        case 3: // EQ
            effect_params = (*eq_decoder_)(mamba_output);
            break;
        default:
            // Default to identity
            effect_params = mamba_output;
            break;
    }
    
    return effect_params;
}

void MambaWrapper::resetState() {
    // State is managed externally in the process function
    std::cerr << "[INFO] Mamba wrapper state reset requested" << std::endl;
}

} // namespace mamba_audio

static clap_plugin_t createPlugin(const clap_plugin_descriptor* desc, const clap_host_t* host) {
    auto plugin = new mamba_audio::MambaClapPlugin(desc, host);
    return {
        .desc = plugin->get_descriptor(),
        .plugin_data = plugin,
        .init = [](clap_plugin_t* plugin) -> bool {
            return static_cast<mamba_audio::MambaClapPlugin*>(plugin->plugin_data)->init();
        },
        .activate = [](clap_plugin_t* plugin, double sample_rate, double min_blep_latency, double max_blep_latency) -> bool {
            return static_cast<mamba_audio::MambaClapPlugin*>(plugin->plugin_data)->activate(sample_rate, min_blep_latency, max_blep_latency);
        },
        .deactivate = [](clap_plugin_t* plugin) {
            static_cast<mamba_audio::MambaClapPlugin*>(plugin->plugin_data)->deactivate();
        },
        .start_processing = [](clap_plugin_t* plugin) -> bool {
            return static_cast<mamba_audio::MambaClapPlugin*>(plugin->plugin_data)->start_processing();
        },
        .stop_processing = [](clap_plugin_t* plugin) {
            static_cast<mamba_audio::MambaClapPlugin*>(plugin->plugin_data)->stop_processing();
        },
        .process = [](clap_plugin_t* plugin, const clap_process_t* process) {
            return static_cast<mamba_audio::MambaClapPlugin*>(plugin->plugin_data)->process(process);
        },
        .get_extension = [](const clap_plugin_t* plugin, const char* id) -> const void* {
            return nullptr;
        },
        .destroy = [](clap_plugin_t* plugin) {
            delete static_cast<mamba_audio::MambaClapPlugin*>(plugin->plugin_data);
        }
    };
}

extern "C" {
static const clap_plugin_entry_t ClapEntry = {
    .clap_version = CLAP_VERSION,
    .init = [](const char* plugin_path) -> bool {
        std::cout << "[INFO] MambaFX CLAP plugin initialized" << std::endl;
        return true;
    },
    .deinit = []() {
    },
    .get_factory = [](const char* factory_id) -> const clap_plugin_factory_t* {
        return nullptr;
    }
};

CLAP_EXPORT const clap_plugin_entry_t* clap_entry() {
    return &ClapEntry;
}
}