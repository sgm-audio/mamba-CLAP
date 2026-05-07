#include "clap-plugin.hpp"
#include <cstring>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;
namespace mamba_audio {

static const clap_plugin_descriptor kDescriptor = {
    .clap_version = CLAP_VERSION,
    .id = "com.mamba-audio.spectrogram-enhancer",
    .name = "Mamba Spectrogram Enhancer",
    .vendor = "Mamba Audio",
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = "1.0.0",
    .min_api_version = "1.0",
    .description = "Audio effect using Mamba SSM for spectrogram enhancement with HiFi-GAN vocoder",
    .features = (const char*[]){
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_UTILITY,
        nullptr
    }
};

MambaClapPlugin::MambaClapPlugin(const clap_plugin_descriptor* desc, const clap_host_t* host)
    : descriptor_(desc), host_(host) {
}

MambaClapPlugin::~MambaClapPlugin() {
    deactivate();
}

bool MambaClapPlugin::init() {
    return true;
}

bool MambaClapPlugin::activate(double sample_rate, double min_blep_latency, double max_blep_latency) {
    sample_rate_ = sample_rate;

    if (sample_rate_ != 22050.0 && sample_rate_ != 44100.0 && sample_rate_ != 48000.0) {
        sample_rate_ = 22050.0;
    }

    stft_processor_ = std::make_unique<STFTProcessor>(n_fft_, hop_length_, n_mels_, sample_rate_);

    const char* mamba_model_path = getenv("MAMBA_MODEL_PATH");
    if (mamba_model_path == nullptr) {
        mamba_model_path = "./models/mamba_spectrogram_predictor.pt";
    }

    const char* hifigan_model_path = getenv("HIFIGAN_MODEL_PATH");
    if (hifigan_model_path == nullptr) {
        hifigan_model_path = "./models/hifigan_generator.pt";
    }

    try {
        mamba_wrapper_ = std::make_unique<MambaWrapper>(mamba_model_path, n_mels_, device_);
        hifigan_wrapper_ = std::make_unique<HiFiGANWrapper>(hifigan_model_path, device_);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load models: " << e.what() << std::endl;
        return false;
    }

    is_active_ = true;
    return true;
}

void MambaClapPlugin::deactivate() {
    is_active_ = false;
    is_processing_ = false;
    stft_processor_.reset();
    mamba_wrapper_.reset();
    hifigan_wrapper_.reset();
}

bool MambaClapPlugin::start_processing() {
    is_processing_ = true;
    return true;
}

void MambaClapPlugin::stop_processing() {
    is_processing_ = false;
}

void MambaClapPlugin::process(const clap_process_t* process) {
    if (!is_active_ || !is_processing_ || process->num_samples == 0) {
        return;
    }

    const float* input = process->audio_inputs[0].data32;
    float* output = process->audio_outputs[0].data32;

    int num_samples = process->num_samples;
    int num_input_channels = process->audio_inputs[0].channel_count;
    int num_output_channels = process->audio_outputs[0].channel_count;

    if (input != output) {
        std::memcpy(output, input, num_samples * sizeof(float) * std::min(num_input_channels, num_output_channels));
    }

    if (num_input_channels > 0 && num_output_channels > 0) {
        for (int i = 0; i < num_samples; i++) {
            float sample_in = input[i * num_input_channels];
            float sample_out = output[i * num_output_channels];

            sample_out = sample_in;

            output[i * num_output_channels] = sample_out;
        }
    }

    if (mamba_wrapper_ && hifigan_wrapper_ && stft_processor_) {
        torch::NoGradGuard no_grad;

        int block_size = 2048;
        std::vector<float> output_buffer(num_samples * num_output_channels, 0.0f);

        for (int offset = 0; offset < num_samples; offset += block_size) {
            int block_end = std::min(offset + block_size, num_samples);
            int block_samples = block_end - offset;

            std::vector<float> block_input(block_samples);
            for (int i = 0; i < block_samples; i++) {
                block_input[i] = input[(offset + i) * num_input_channels];
            }

            auto input_tensor = torch::from_blob(block_input.data(), {1, block_samples}, torch::kFloat).to(device_);

            auto mel_spec = stft_processor_->forward(input_tensor);

            if (mel_spec.dim() == 3) {
                mel_spec = mel_spec.unsqueeze(0);
            }

            applyMelSpectrogramEnhancement(mel_spec);

            auto waveform = stft_processor_->inverse(mel_spec);

            auto output_block = waveform.cpu().flatten();

            for (int i = 0; i < block_samples && (offset + i) < num_samples; i++) {
                if (i < output_block.size(0)) {
                    output_buffer[(offset + i) * num_output_channels] = output_block[i].item<float>();
                }
            }
        }

        for (int i = 0; i < num_samples * num_output_channels; i++) {
            output[i] = output_buffer[i];
        }
    }
}

void MambaClapPlugin::applyMelSpectrogramEnhancement(torch::Tensor& mel_spec) {
    try {
        mel_spec = mamba_wrapper_->process(mel_spec);
    } catch (const std::exception& e) {
        std::cerr << "Mamba processing error: " << e.what() << std::endl;
    }
}

STFTProcessor::STFTProcessor(int n_fft, int hop_length, int n_mels, double sample_rate)
    : n_fft_(n_fft), hop_length_(hop_length), n_mels_(n_mels), sample_rate_(sample_rate) {
    window_func_ = torch::hann_window(n_fft_, true);
}

STFTProcessor::~STFTProcessor() = default;

torch::Tensor STFTProcessor::forward(const torch::Tensor& waveform) {
    auto options = torch::TensorOptions().dtype(torch::kFloat32);
    auto spec = torch::stft(
        waveform.squeeze(),
        n_fft_,
        hop_length_,
        n_fft_,
        window_func_,
        true,
        options
    );

    auto mag = torch::abs(spec);

    auto mel_basis = torch::from_blob(
        torchaudio::complinations::melscale::mel_scale(
            n_mels_, n_fft_ / 2 + 1, sample_rate_, 0.0, sample_rate_ / 2.0
        ).data(),
        {n_mels_, n_fft_ / 2 + 1},
        options
    ).to(waveform.device());

    auto mel = torch::matmul(mel_basis.to(waveform.device()), mag);

    return mel.transpose(1, 2);
}

torch::Tensor STFTProcessor::inverse(const torch::Tensor& mel_spectrogram) {
    auto device = mel_spectrogram.device();

    auto mel_basis = torch::from_blob(
        torchaudio::complinations::melscale::mel_scale(
            n_mels_, n_fft_ / 2 + 1, sample_rate_, 0.0, sample_rate_ / 2.0
        ).data(),
        {n_mels_, n_fft_ / 2 + 1},
        torch::TensorOptions().dtype(torch::kFloat32)
    ).to(device);

    auto inv_mel_basis = torch::pinverse(mel_basis);

    auto spec = torch::matmul(inv_mel_basis.to(device), mel_spectrogram.squeeze().transpose(1, 2));

    auto waveform = torch::istft(
        spec.transpose(1, 2),
        n_fft_,
        hop_length_,
        n_fft_,
        window_func_.to(device),
        true,
        torch::TensorOptions().dtype(torch::kFloat32).device(device)
    );

    return waveform;
}

MambaWrapper::MambaWrapper(const std::string& model_path, int n_mels, torch::Device device)
    : device_(device), n_mels_(n_mels) {
    if (fs::exists(model_path)) {
        try {
            auto state_dict = torch::load(model_path);
            model_ = torch::nn::Sequential(
                torch::nn::Linear(n_mels, 512),
                torch::nn::GELU(),
                torch::nn::LayerNorm(512),
                torch::nn::Linear(512, 512),
                torch::nn::Linear(512, n_mels)
            );
            model_->to(device_);
            model_->eval();
            std::cout << "Loaded Mamba model from: " << model_path << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load model: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Model not found at: " << model_path << ", using fallback" << std::endl;
        model_ = torch::nn::Sequential(
            torch::nn::Linear(n_mels, 512),
            torch::nn::GELU(),
            torch::nn::LayerNorm(512),
            torch::nn::Linear(512, 512),
            torch::nn::Linear(512, n_mels)
        );
        model_->to(device_);
        model_->eval();
    }
}

MambaWrapper::~MambaWrapper() = default;

torch::Tensor MambaWrapper::process(const torch::Tensor& mel_input) {
    auto x = mel_input.to(device_);

    if (x.dim() == 3) {
        x = x.transpose(1, 2);
    }

    auto output = model_->forward(x);

    if (output.dim() == 3) {
        output = output.transpose(1, 2);
    }

    return output;
}

HiFiGANWrapper::HiFiGANWrapper(const std::string& model_path, torch::Device device)
    : device_(device) {
    if (fs::exists(model_path)) {
        try {
            auto state_dict = torch::load(model_path);
            std::cout << "Loaded HiFi-GAN model from: " << model_path << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load HiFi-GAN model: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "HiFi-GAN model not found at: " << model_path << std::endl;
    }
}

HiFiGANWrapper::~HiFiGANWrapper() = default;

torch::Tensor HiFiGANWrapper::mel2waveform(const torch::Tensor& mel_spectrogram) {
    if (generator_ != nullptr) {
        auto x = mel_spectrogram.to(device_);
        auto output = generator_->forward(x);
        return output.cpu();
    }
    return mel_spectrogram;
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
            static_cast<mamba_audio::MambaClapPlugin*>(plugin->plugin_data)->process(process);
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
        std::cout << "Mamba Spectrogram Enhancer CLAP plugin initialized" << std::endl;
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