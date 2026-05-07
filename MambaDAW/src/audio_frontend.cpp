#include "audio_frontend.h"
#include <cmath>
#include <algorithm>
#include <numeric>

constexpr float PI = 3.14159265358979323846f;

// Helper function to compute Mel scale
static float hzToMel(float hz) {
    return 2595.0f * std::log10f(1.0f + hz / 700.0f);
}

// Helper function to compute inverse Mel scale
static float melToHz(float mel) {
    return 700.0f * (std::powf(10.0f, mel / 2595.0f) - 1.0f);
}

AudioFrontend::AudioFrontend(int window_size, int hop_size, int sample_rate, 
                           int num_mel_bins, float mel_low_freq, float mel_high_freq)
    : window_size_(window_size),
      hop_size_(hop_size),
      sample_rate_(sample_rate),
      num_mel_bins_(num_mel_bins),
      mel_low_freq_(mel_low_freq),
      mel_high_freq_(mel_high_freq),
      freq_bins_(window_size / 2 + 1),
      momentum_(0.01f),
      stats_initialized_(false),
      input_buffer_write_pos_(0),
      samples_since_last_frame_(0),
      output_buffer_write_pos_(0),
      output_buffer_read_pos_(0),
      overlap_add_offset_(0) {
    
    // Create Hann window
    hann_window_ = torch::zeros({window_size_}, torch::kFloat);
    for (int i = 0; i < window_size_; ++i) {
        hann_window_.index_put_({i}, 0.5f * (1.0f - std::cos(2.0f * PI * i / (window_size_ - 1)));
    }
    
    // Initialize mel filterbank
    mel_filterbank_ = torch::zeros({num_mel_bins_, freq_bins_}, torch::kFloat);
    
    // If mel frequencies not specified, compute them
    float low_mel = mel_low_freq_ > 0 ? hzToMel(mel_low_freq_) : 0;
    float high_mel = mel_high_freq_ > 0 ? hzToMel(mel_high_freq_) : hzToMel(sample_rate_ / 2.0f);
    
    for (int m = 0; m < num_mel_bins_; ++m) {
        float mel_low = low_mel + (high_mel - low_mel) * m / (num_mel_bins_ + 1);
        float mel_center = low_mel + (high_mel - low_mel) * (m + 1) / (num_mel_bins_ + 1);
        float mel_high = low_mel + (high_mel - low_mel) * (m + 2) / (num_mel_bins_ + 1);
        
        float hz_low = melToHz(mel_low);
        float hz_center = melToHz(mel_center);
        float hz_high = melToHz(mel_high);
        
        int bin_low = static_cast<int>(std::floor(hz_low * window_size_ / sample_rate_));
        int bin_center = static_cast<int>(std::floor(hz_center * window_size_ / sample_rate_));
        int bin_high = static_cast<int>(std::floor(hz_high * window_size_ / sample_rate_));
        
        bin_low = std::max(0, bin_low);
        bin_center = std::min(freq_bins_ - 1, bin_center);
        bin_high = std::min(freq_bins_ - 1, bin_high);
        
        for (int k = bin_low; k <= bin_high; ++k) {
            float weight;
            if (k < bin_center) {
                weight = (k - bin_low) / (bin_center - bin_low);
            } else {
                weight = (bin_high - k) / (bin_high - bin_center);
            }
            mel_filterbank_.index_put_({m, k}, std::max(0.0f, weight));
        }
    }
    
    // Compute pseudo-inverse for approximate reconstruction (simplified)
    // In practice, you'd use a more sophisticated method
    mel_pinv_ = mel_filterbank_.t(); // Simple transpose as approximation
    
    // Initialize buffers
    input_buffer_.assign(window_size_, 0.0f);
    output_buffer_.assign(window_size_ + hop_size_, 0.0f); // Extra space for overlap-add
    
    // Initialize statistics tensors
    running_mean_ = torch::zeros({num_mel_bins_}, torch::kFloat);
    running_var_ = torch::zeros({num_mel_bins_}, torch::kFloat);
    
    // Initialize processing tensors
    windowed_signal_ = torch::zeros({window_size_}, torch::kFloat);
    stft_complex_ = torch::zeros({freq_bins_ * 2}, torch::kFloat); // Real and imaginary interleaved
    magnitude_ = torch::zeros({freq_bins_}, torch::kFloat);
    phase_ = torch::zeros({freq_bins_}, torch::kFloat);
    log_mel_ = torch::zeros({num_mel_bins_}, torch::kFloat);
    normalized_features_ = torch::zeros({num_mel_bins_}, torch::kFloat);
    mel_spec_approx_ = torch::zeros({num_mel_bins_}, torch::kFloat);
    linear_mag_approx_ = torch::zeros({freq_bins_}, torch::kFloat);
    iSTFT_complex_ = torch::zeros({freq_bins_ * 2}, torch::kFloat);
    iSTFT_windowed_ = torch::zeros({window_size_}, torch::kFloat);
}

AudioFrontend::~AudioFrontend() = default;

void AudioFrontend::acceptInput(const float* input, int num_samples) {
    for (int i = 0; i < num_samples; ++i) {
        input_buffer_[input_buffer_write_pos_] = input[i];
        input_buffer_write_pos_ = (input_buffer_write_pos_ + 1) % window_size_;
        samples_since_last_frame_++;
        
        if (samples_since_last_frame_ >= hop_size_) {
            computeSTFTFrame();
            computeLogMel();
            normalizeFeatures();
            computeiSTFT();
            overlapAdd();
            samples_since_last_frame_ = 0;
        }
    }
}

bool AudioFrontend::isReadyForOutput() const {
    // We can output when we have processed at least one frame
    return samples_since_last_frame_ < hop_size_; // Simplified check
}

void AudioFrontend::getOutput(float* output, int num_samples) {
    int samples_to_copy = std::min(num_samples, static_cast<int>(output_buffer_.size()) - output_buffer_read_pos_);
    
    for (int i = 0; i < samples_to_copy; ++i) {
        output[i] = output_buffer_[output_buffer_read_pos_];
        output_buffer_read_pos_ = (output_buffer_read_pos_ + 1) % output_buffer_.size();
    }
    
    // If we didn't fill the entire output, zero-pad the rest
    for (int i = samples_to_copy; i < num_samples; ++i) {
        output[i] = 0.0f;
    }
}

torch::Tensor AudioFrontend::getFeatures() const {
    // Return a batch of size 1
    return normalized_features_.unsqueeze(0); // [1, num_mel_bins]
}

void AudioFrontend::setModelOutput(const torch::Tensor& model_output) {
    // model_output should be [1, num_mel_bins] or [num_mel_bins]
    torch::Tensor output = model_output.squeeze(0);
    if (output.sizes() == torch::IntArrayRef{num_mel_bins_}) {
        mel_spec_approx_ = output.clone();
    }
}

void AudioFrontend::reset() {
    std::fill(input_buffer_.begin(), input_buffer_.end(), 0.0f);
    std::fill(output_buffer_.begin(), output_buffer_.end(), 0.0f);
    input_buffer_write_pos_ = 0;
    samples_since_last_frame_ = 0;
    output_buffer_write_pos_ = 0;
    output_buffer_read_pos_ = 0;
    overlap_add_offset_ = 0;
    running_mean_.zero_();
    running_var_.zero_();
    stats_initialized_ = false;
}

void AudioFrontend::computeSTFTFrame() {
    // Copy input buffer to windowed signal and apply Hann window
    for (int i = 0; i < window_size_; ++i) {
        int idx = (input_buffer_write_pos_ - window_size_ + i + window_size_) % window_size_;
        windowed_signal_.index_put_({i}, input_buffer_[idx] * hann_window_.index({i}).item<float>());
    }
    
    // Compute FFT (using torch's FFT for simplicity - in production you'd use a more efficient implementation)
    // Note: torch::fft::rfft expects complex input, but we have real input
    auto fft_result = torch::fft::rfft(windowed_signal_.unsqueeze(0), /*signal_ndim*/1, /*normalized*/false);
    // fft_result is complex: [1, freq_bins]
    
    // Extract real and imaginary parts
    auto fft_real = fft_result.real(); // [1, freq_bins]
    auto fft_imag = fft_result.imag(); // [1, freq_bins]
    
    // Interleave real and imaginary parts in our tensor
    for (int i = 0; i < freq_bins_; ++i) {
        stft_complex_.index_put_({i * 2}, fft_real.index({0, i}));
        stft_complex_.index_put_({i * 2 + 1}, fft_imag.index({0, i}));
    }
    
    // Compute magnitude and phase
    for (int i = 0; i < freq_bins_; ++i) {
        float real = stft_complex_.index({i * 2}).item<float>();
        float imag = stft_complex_.index({i * 2 + 1}).item<float>();
        magnitude_.index_put_({i}, std::sqrt(real * real + imag * imag));
        phase_.index_put_({i}, std::atan2(imag, real));
    }
}

void AudioFrontend::computeLogMel() {
    // Magnitude to Mel scale: mel_spec = mel_filterbank * magnitude
    log_mel_ = torch::mv(mel_filterbank_, magnitude_);
    
    // Log compression
    log_mel_ = torch::log(torch::clamp(log_mel_, 1e-5f, 1e5f));
}

void AudioFrontend::normalizeFeatures() {
    if (!stats_initialized_) {
        running_mean_ = log_mel_.clone();
        running_var_ = torch::zeros_like(log_mel_);
        stats_initialized_ = true;
    } else {
        // Update running statistics
        running_mean_ = (1.0f - momentum_) * running_mean_ + momentum_ * log_mel_;
        running_var_ = (1.0f - momentum_) * running_var_ + momentum_ * torch::pow(log_mel_ - running_mean_, 2);
    }
    
    // Normalize
    normalized_features_ = (log_mel_ - running_mean_) / torch::sqrt(running_var_ + 1e-5f);
}

void AudioFrontend::computeiSTFT() {
    // Approximate magnitude from Mel: linear_mag = mel_pinv * mel_spec
    linear_mag_approx_ = torch::mv(mel_pinv_, mel_spec_approx_);
    
    // For phase, we'll use the input phase (simple approach)
    // In a more advanced implementation, we could use Griffin-Lim
    
    // Reconstruct complex spectrum
    for (int i = 0; i < freq_bins_; ++i) {
        float mag = linear_mag_approx_.index({i}).item<float>();
        float phase = this->phase_.index({i}).item<float>(); // Use input phase
        iSTFT_complex_.index_put_({i * 2}, mag * std::cos(phase));
        iSTFT_complex_.index_put_({i * 2 + 1}, mag * std::sin(phase));
    }
    
    // Compute inverse FFT
    auto fft_real = iSTFT_complex_.slice(0, 0, freq_bins_ * 2, 2); // Even indices
    auto fft_imag = iSTFT_complex_.slice(0, 1, freq_bins_ * 2, 2); // Odd indices
    auto fft_complex = torch::complex(fft_real, fft_imag);
    
    auto ifft_result = torch::fft::irfft(fft_complex.unsqueeze(0), window_size_, /*signal_ndim*/1, /*normalized*/false);
    iSTFT_windowed_ = ifft_result.squeeze(0);
    
    // Apply window again (for overlap-add)
    for (int i = 0; i < window_size_; ++i) {
        iSTFT_windowed_.index_put_({i}, iSTFT_windowed_.index({i}).item<float>() * hann_window_.index({i}).item<float>());
    }
}

void AudioFrontend::overlapAdd() {
    // Add windowed iSTFT result to output buffer
    for (int i = 0; i < window_size_; ++i) {
        int idx = (output_buffer_write_pos_ + i) % output_buffer_.size();
        output_buffer_[idx] += iSTFT_windowed_.index({i}).item<float>();
    }
    
    output_buffer_write_pos_ = (output_buffer_write_pos_ + window_size_) % output_buffer_.size();
    overlap_add_offset_ = (overlap_add_offset_ + window_size_) % output_buffer_.size();
}