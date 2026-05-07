#ifndef AUDIO_FRONTEND_H
#define AUDIO_FRONTEND_H

#include <torch/torch.h>
#include <vector>
#include <cmath>

class AudioFrontend {
public:
    AudioFrontend(int window_size = 2048, int hop_size = 512,
                  int sample_rate = 44100, int num_mel_bins = 80,
                  float mel_low_freq = 0.0f, float mel_high_freq = 0.0f);
    ~AudioFrontend();

    // Accept hop_size samples of input audio
    void acceptInput(const float* input, int num_samples);
    
    // Check if we have processed enough to produce output
    bool isReadyForOutput() const;
    
    // Get hop_size samples of output audio
    void getOutput(float* output, int num_samples);
    
    // Get the current normalized log-mel features (for model input)
    // Returns a tensor of shape [1, num_mel_bins] (batch=1, features=num_mel_bins)
    torch::Tensor getFeatures() const;
    
    // Set model output (log-mel spectrogram) for the current frame
    void setModelOutput(const torch::Tensor& model_output);
    
    // Reset state (e.g., between audio files)
    void reset();

private:
    void computeSTFTFrame();
    void computeLogMel();
    void normalizeFeatures();
    void computeiSTFT();
    void overlapAdd();
    
    int window_size_;
    int hop_size_;
    int sample_rate_;
    int num_mel_bins_;
    float mel_low_freq_;
    float mel_high_freq_;
    
    int freq_bins_; // window_size/2 + 1
    
    torch::Tensor hann_window_;
    torch::Tensor mel_filterbank_; // [num_mel_bins, freq_bins]
    torch::Tensor mel_pinv_;       // [freq_bins, num_mel_bins]
    
    torch::Tensor running_mean_;   // [num_mel_bins]
    torch::Tensor running_var_;    // [num_mel_bins]
    float momentum_;
    bool stats_initialized_;
    
    // Circular buffer for input audio (holds window_size samples)
    std::vector<float> input_buffer_;
    int input_buffer_write_pos_;
    int samples_since_last_frame_;
    
    // For STFT processing
    torch::Tensor windowed_signal_; // [window_size]
    torch::Tensor stft_complex_;    // [freq_bins] complex (real and imaginary interleaved as [real0, imag0, real1, imag1, ...])
    torch::Tensor magnitude_;       // [freq_bins]
    torch::Tensor phase_;           // [freq_bins]
    
    // For Mel and normalization
    torch::Tensor log_mel_;         // [num_mel_bins]
    torch::Tensor normalized_features_; // [num_mel_bins]
    
    // For iSTFT
    torch::Tensor mel_spec_approx_; // [num_mel_bins]
    torch::Tensor linear_mag_approx_; // [freq_bins]
    torch::Tensor iSTFT_complex_;   // [freq_bins] complex
    torch::Tensor iSTFT_windowed_;  // [window_size]
    
    // Overlap-add buffers
    std::vector<float> output_buffer_;
    int output_buffer_write_pos_;
    int output_buffer_read_pos_;
    int overlap_add_offset_;
};

#endif // AUDIO_FRONTEND_H