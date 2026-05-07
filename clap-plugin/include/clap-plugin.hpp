#pragma once

#include <clap/clap.h>
#include <torch/torch.h>
#include <memory>
#include <vector>
#include <string>

namespace mamba_audio {

class STFTProcessor;
class MambaWrapper;
class HiFiGANWrapper;

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

    const clap_plugin_descriptor* get_descriptor() const { return descriptor_; }
    const clap_host_t* get_host() const { return host_; }

    static const clap_plugin_t* getPluginInterface();

private:
    bool processAudioBlock(const float* input, float* output, int num_samples);
    bool applyMelSpectrogramEnhancement(torch::Tensor& mel_spec);

    const clap_plugin_descriptor* descriptor_;
    const clap_host_t* host_;

    double sample_rate_ = 22050.0;
    bool is_active_ = false;
    bool is_processing_ = false;

    std::unique_ptr<STFTProcessor> stft_processor_;
    std::unique_ptr<MambaWrapper> mamba_wrapper_;
    std::unique_ptr<HiFiGANWrapper> hifigan_wrapper_;

    int hop_length_ = 256;
    int n_fft_ = 1024;
    int n_mels_ = 80;

    torch::Device device_ = torch::kCPU;
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

    torch::Tensor window_func_;
};

class MambaWrapper {
public:
    MambaWrapper(const std::string& model_path, int n_mels, torch::Device device);
    ~MambaWrapper();

    torch::Tensor process(const torch::Tensor& mel_input);

private:
    torch::Device device_;
    torch::nn::Sequential model_;
    int n_mels_;
};

class HiFiGANWrapper {
public:
    HiFiGANWrapper(const std::string& model_path, torch::Device device);
    ~HiFiGANWrapper();

    torch::Tensor mel2waveform(const torch::Tensor& mel_spectrogram);

private:
    torch::Device device_;
    torch::nn::Module* generator_ = nullptr;
};

} // namespace mamba_audio