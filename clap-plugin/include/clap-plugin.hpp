#pragma once

#include <clap/clap.h>
#include <torch/torch.h>
#include <torch/script.h>
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <cstdint>
#include <fftw3.h>

namespace mamba_audio {

class STFTProcessor;
class MambaWrapper;

class MambaClapPlugin {
public:
    MambaClapPlugin(const clap_plugin_descriptor* desc, const clap_host_t* host);
    ~MambaClapPlugin();

    bool init();
    bool activate(double sample_rate, double min_blep_latency, double max_blep_latency);
    void deactivate();
    bool start_processing();
    void stop_processing();
    void process(const clap_process_t* process);

    // Parameter callbacks
    bool param_value(const char* param_id, double value);
    double param_value(const char* param_id) const;
    void param_change(const char* param_id, double value, uint32_t flags);

    const clap_plugin_descriptor* get_descriptor() const { return descriptor_; }
    const clap_host_t* get_host() const { return host_; }

    static const clap_plugin_t* getPluginInterface();

private:
    bool processAudioBlock(const float* input, float* output, int num_samples);
    bool applyMelSpectrogramEnhancement(torch::Tensor& mel_spec);

    const clap_plugin_descriptor* descriptor_;
    const clap_host_t* host_;

    double sample_rate_ = 44100.0;
    bool is_active_ = false;
    bool is_processing_ = false;

    std::unique_ptr<STFTProcessor> stft_processor_;
    std::unique_ptr<MambaWrapper> mamba_wrapper_;

    int hop_length_ = 256;
    int n_fft_ = 1024;
    int n_mels_ = 80;

    // Maximum supported block size for pre-allocation
    static constexpr int MAX_BLOCK_SIZE = 32768;
    static constexpr int MAX_FRAMES = (MAX_BLOCK_SIZE / 256) + 2; // worst case

    // REAL-TIME SAFE: Pre-allocated buffers (allocated in activate(), never in process())
    torch::Tensor prealloc_input_;       // [1, MAX_BLOCK_SIZE]
    torch::Tensor prealloc_mel_spec_;    // [MAX_FRAMES, n_mels]
    torch::Tensor prealloc_processed_mel_; // [MAX_FRAMES, n_mels]
    torch::Tensor prealloc_zeros_;        // [1, n_mels] - reusable zero tensor for error path

    // Persistent Mamba state (survives across blocks)
    std::vector<torch::Tensor> mamba_state_;
    bool state_initialized_ = false;

    // Atomic parameter access (safe from NRT thread)
    std::atomic<float> param_effect_type_{0.0f};
    std::atomic<float> param_mix_{0.5f};

    // Metrics (for monitoring and diagnostics)
    std::atomic<uint64_t> blocks_processed_count_{0};
    std::atomic<uint64_t> total_fft_time_us_{0};
    std::atomic<uint64_t> processing_error_count_{0};
    std::atomic<uint64_t> last_error_code_{0};

    // Get metrics snapshot (thread-safe)
    struct Metrics {
        uint64_t blocks_processed;
        uint64_t total_fft_time_us;
        uint64_t processing_errors;
        double avg_fft_time_us() const {
            return blocks_processed > 0 ? static_cast<double>(total_fft_time_us) / blocks_processed : 0.0;
        }
    };
    Metrics getMetrics() const {
        return Metrics{
            blocks_processed_count_.load(std::memory_order_relaxed),
            total_fft_time_us_.load(std::memory_order_relaxed),
            processing_error_count_.load(std::memory_order_relaxed)
        };
    }

    // Helper to map effect_type float to int index
    int getEffectTypeIndex() const {
        float v = param_effect_type_.load(std::memory_order_relaxed);
        if (v < 0.0f) v = 0.0f;
        if (v > 3.0f) v = 3.0f;
        return static_cast<int>(std::round(v));
    };
};

class STFTProcessor {
public:
    STFTProcessor(int n_fft, int hop_length, int n_mels, double sample_rate);
    ~STFTProcessor();

    torch::Tensor forward(const torch::Tensor& waveform);
    torch::Tensor inverse(const torch::Tensor& mel_spectrogram);

private:
    int n_fft_;
    int hop_length_;
    int n_mels_;
    double sample_rate_;
    int freq_bins_; // n_fft/2 + 1
    int pad_;       // n_fft_/2

    torch::Tensor window_func_;
    // Mel filter bank: [n_mels, n_fft/2+1]
    torch::Tensor mel_basis_;
    torch::Tensor inv_mel_basis_; // pseudo-inverse for ISTFT

    // FFTW resources
    fftwf_plan forward_plan_;
    fftwf_plan inverse_plan_;
    float* fftw_real_buffer_;
    fftwf_complex* fftw_complex_buffer_;
    float* window_host_; // host copy of Hann window for FFTW
    // For center padding (replicating torch.stft center=True)
    float* padded_buffer_;
    size_t padded_buffer_capacity_;
    int max_block_size_;

    // State for inverse cropping
    size_t last_input_len_ = 0; // input length from most recent forward call
    float window_energy_sum_ = 0.0f; // sum of squared window for compensation
};

class MambaWrapper {
public:
    MambaWrapper(const std::string& model_path, int n_mels, torch::Device device);
    ~MambaWrapper();

    // Get initial state for streaming processing
    std::vector<torch::Tensor> getInitialState(int batch_size = 1);
    // Process one time step: input [batch, n_mels], updates state, returns output [batch, n_mels]
    torch::Tensor step(const torch::Tensor& input, std::vector<torch::Tensor>& state);
    // Process effect-specific: input [batch, n_mels], current state, effect type -> effect parameters
    torch::Tensor processEffect(const torch::Tensor& input, int effect_type, std::vector<torch::Tensor>& state);
    void resetState(); // For clearing context when needed

private:
    torch::Device device_;
    torch::jit::Module module_;
    int n_mels_;

    // Effect decoders (simple linear layers for now)
    torch::nn::Linear reverb_decoder_{nullptr};
    torch::nn::Linear compression_decoder_{nullptr};
    torch::nn::Linear saturation_decoder_{nullptr};
    torch::nn::Linear eq_decoder_{nullptr};
};

} // namespace mamba_audio
