#include "mamba_model.h"
#include <cstring>
#include <cmath>

MambaModel::MambaModel(int64_t d_model, int64_t n_layers,
int64_t d_state, int64_t d_conv, int64_t expand)
: d_model_(d_model), n_layers_(n_layers),
d_state_(d_state), d_conv_(d_conv), expand_(expand),
tmp_frame_(static_cast<size_t>(d_model), 0.f) {
layers_.reserve(n_layers_);
for (int64_t i = 0; i < n_layers_; ++i)
layers_.push_back(std::make_unique<MambaLayer>(d_model_, d_state, d_conv, expand));
}

MambaModel::~MambaModel() = default;

float MambaModel::processSample(float sample) {
float out = sample;
for (auto& layer : layers_)
out = layer->processSample(out);
if (!std::isfinite(out)) out = 0.f;
return out;
}

// sanitizeOutput() is a bounded source-level helper over caller-provided memory.
// This local observation is not a full plug-in callback or hard-real-time audit.

static void sanitizeOutput(float* buf, int64_t n) noexcept {
for (int64_t i = 0; i < n; ++i) {
if (!std::isfinite(buf[i])) buf[i] = 0.f;
}
}

void MambaModel::processFrame(const float* input, float* output) noexcept {
const size_t n = layers_.size();
if (n == 0) {
std::memcpy(output, input, static_cast<size_t>(d_model_) * sizeof(float));
sanitizeOutput(output, d_model_);
return;
}
if (n == 1) {
layers_[0]->processVector(input, output);
sanitizeOutput(output, d_model_);
return;
}

layers_[0]->processVector(input, tmp_frame_.data());
for (size_t i = 1; i < n; ++i) {
if (i % 2 == 1)
layers_[i]->processVector(tmp_frame_.data(), output);
else
layers_[i]->processVector(output, tmp_frame_.data());
}
if ((n - 1) % 2 == 0)
std::memcpy(output, tmp_frame_.data(), static_cast<size_t>(d_model_) * sizeof(float));

sanitizeOutput(output, d_model_);
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

MambaModel::ModelState MambaModel::getState() const {
  ModelState s;
  s.layers.reserve(layers_.size());
  for (const auto& layer : layers_)
    s.layers.push_back(layer->getState());
  return s;
}

bool MambaModel::setState(const ModelState& s) {
  if (static_cast<int64_t>(s.layers.size()) != n_layers_) return false;
  for (size_t i = 0; i < layers_.size(); ++i) {
    if (!layers_[i]->setState(s.layers[i])) return false;
  }
  return true;
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
