#include "mamba_model.h"

MambaModel::MambaModel(int64_t d_model, int64_t n_layers,
                       int64_t d_state, int64_t d_conv, int64_t expand)
    : d_model_(d_model), n_layers_(n_layers),
      d_state_(d_state), d_conv_(d_conv), expand_(expand) {
    layers_.reserve(n_layers_);
    for (int64_t i = 0; i < n_layers_; ++i)
        layers_.push_back(std::make_unique<MambaLayer>(d_model_, d_state, d_conv, expand));
}

MambaModel::~MambaModel() = default;

float MambaModel::processSample(float sample) {
    float out = sample;
    for (auto& layer : layers_)
        out = layer->processSample(out);
    return out;
}

void MambaModel::processBlock(const float* input, float* output, int64_t num_samples) {
    for (int64_t i = 0; i < num_samples; ++i)
        output[i] = processSample(input[i]);
}

void MambaModel::resetState() {
    for (auto& layer : layers_)
        layer->reset_state();
}

void MambaModel::copyStateFrom(const MambaModel& other) {
    for (size_t i = 0; i < layers_.size(); ++i) {
        layers_[i]->copy_state_from(*other.layers_[i]);
    }
}

void MambaModel::setContextScale(float scale) {
    for (auto& layer : layers_) {
        layer->setContextScale(scale);
    }
}

bool MambaModel::loadLayerWeights(int64_t layer_idx,
                                   const float* in_proj,  size_t in_proj_n,
                                   const float* out_proj, size_t out_proj_n,
                                   const float* conv1d,   size_t conv1d_n,
                                   const float* dA,       size_t dA_n,
                                   const float* dB,       size_t dB_n,
                                   const float* C,        size_t C_n,
                                   const float* D,        size_t D_n) {
    if (layer_idx < 0 || layer_idx >= n_layers_) return false;
    return layers_[static_cast<size_t>(layer_idx)]->loadLayerWeights(
        in_proj, in_proj_n, out_proj, out_proj_n,
        conv1d, conv1d_n,
        dA, dA_n, dB, dB_n, C, C_n, D, D_n);
}
