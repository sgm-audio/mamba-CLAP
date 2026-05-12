#include "plugin_processor.h"
#include "mamba_weight_loader.h"
#include "feature_extractor_rt.h"
#include <cmath>
#include <algorithm>
#include <cstring>

PluginProcessor::PluginProcessor(int sample_rate)
    : sample_rate_(sample_rate),
      param_gate_(std::array<double, 4>{0.05, 0.0, 1.0, 0.02}) {
    mamba_model_ = std::make_unique<MambaModel>(1, 4, 16, 4, 2);
    snapshot_model_ = std::make_unique<MambaModel>(1, 4, 16, 4, 2);
}

PluginProcessor::~PluginProcessor() = default;

// =============================================================================
// Lifecycle (non-RT thread)
// =============================================================================

void PluginProcessor::activate(double sample_rate,
                               uint32_t /*min_frame_count*/,
                               uint32_t /*max_frame_count*/) {
    sample_rate_ = static_cast<int>(sample_rate);
    mamba_model_->resetState();
    snapshot_model_->resetState();
    crossfade_samples_.store(0, std::memory_order_release);
    param_gate_.reset();
    flush_pending_.store(false, std::memory_order_release);
    activated_.store(true, std::memory_order_release);
}

void PluginProcessor::deactivate() {
    activated_.store(false, std::memory_order_release);
    mamba_model_->resetState();
    snapshot_model_->resetState();
}

void PluginProcessor::resetState() {
    mamba_model_->resetState();
    snapshot_model_->resetState();
    param_gate_.reset();
    flush_pending_.store(false, std::memory_order_release);
    crossfade_samples_.store(0, std::memory_order_release);
}

// =============================================================================
// Audio processing (RT thread)
// =============================================================================

void PluginProcessor::processBlock(float** inputs, float** outputs,
                                   int32_t num_frames,
                                   int32_t num_input_channels,
                                   int32_t num_output_channels) {
    // INVARIANTS: No heap allocations. No logging. No mutex. No torch.
    const bool is_active = activated_.load(std::memory_order_acquire);
    const bool is_bypass = bypass_.load(std::memory_order_acquire);
    const bool has_model = model_loaded_.load(std::memory_order_acquire);
    const bool flush_now = flush_pending_.exchange(false, std::memory_order_acq_rel);

    // Apply context scale dynamically
    const float ctx_scale = base_context_scale_.load(std::memory_order_relaxed);
    mamba_model_->setContextScale(ctx_scale);
    snapshot_model_->setContextScale(ctx_scale);

    // Passthrough: not active, bypassed, or no weights loaded yet.
    if (!is_active || is_bypass || !has_model) {
        const int32_t copy_ch = std::min(num_input_channels, num_output_channels);
        for (int32_t c = 0; c < copy_ch; ++c)
            std::copy_n(inputs[c], num_frames, outputs[c]);
        for (int32_t c = copy_ch; c < num_output_channels; ++c)
            std::fill_n(outputs[c], num_frames, 0.0f);
        return;
    }

    // Read mix once, outside the sample loop (single atomic load per block).
    const float wet = dry_wet_mix_.load(std::memory_order_relaxed);
    const float dry = 1.0f - wet;

    if (flush_now) {
        snapshot_model_->copyStateFrom(*mamba_model_);
        mamba_model_->resetState();
        crossfade_samples_.store(CROSSFADE_LEN, std::memory_order_release);
    }

    int xf = crossfade_samples_.load(std::memory_order_acquire);
    if (xf > 0) {
        int xf_end = std::min<int>(xf, num_frames);
        for (int i = 0; i < xf_end; ++i) {
            // Mono sum — handles mono and stereo input uniformly.
            float in_sample = 0.0f;
            for (int32_t c = 0; c < num_input_channels; ++c)
                in_sample += inputs[c][i];
            if (num_input_channels > 1)
                in_sample /= static_cast<float>(num_input_channels);

            float old_out = snapshot_model_->processSample(in_sample);
            float new_out = mamba_model_->processSample(in_sample);
            if (!std::isfinite(old_out)) old_out = 0.0f;
            if (!std::isfinite(new_out)) new_out = 0.0f;

            // Linear crossfade: alpha=1.0 (old) -> 0.0 (new)
            float alpha = static_cast<float>(xf - i) / static_cast<float>(CROSSFADE_LEN);
            float xf_out = old_out * alpha + new_out * (1.0f - alpha);
            float mixed = std::tanh(dry * in_sample + wet * xf_out);

            for (int32_t c = 0; c < num_output_channels; ++c)
                outputs[c][i] = mixed;
        }
        int remaining = std::max(0, xf - xf_end);
        crossfade_samples_.store(remaining, std::memory_order_release);

        // Continue normal processing for the remainder of the block
        for (int i = xf_end; i < num_frames; ++i) {
            float in_sample = 0.0f;
            for (int32_t c = 0; c < num_input_channels; ++c)
                in_sample += inputs[c][i];
            if (num_input_channels > 1)
                in_sample /= static_cast<float>(num_input_channels);

            float out_sample = mamba_model_->processSample(in_sample);
            if (!std::isfinite(out_sample)) out_sample = 0.0f;
            float mixed = std::tanh(dry * in_sample + wet * out_sample);

            for (int32_t c = 0; c < num_output_channels; ++c)
                outputs[c][i] = mixed;
        }
        return;
    }

    for (int32_t i = 0; i < num_frames; ++i) {
        // Mono sum — handles mono and stereo input uniformly.
        float in_sample = 0.0f;
        for (int32_t c = 0; c < num_input_channels; ++c)
            in_sample += inputs[c][i];
        if (num_input_channels > 1)
            in_sample /= static_cast<float>(num_input_channels);

        // Recurrent SSM step — allocation-free, all buffers pre-allocated.
        float out_sample = mamba_model_->processSample(in_sample);

        // NaN/Inf guard — clamp to silence rather than crash.
        if (!std::isfinite(out_sample)) out_sample = 0.0f;

        // Dry/wet blend + soft clip (tanh) to prevent overflow artifacts.
        float mixed = std::tanh(dry * in_sample + wet * out_sample);

        for (int32_t c = 0; c < num_output_channels; ++c)
            outputs[c][i] = mixed;
    }
}

// =============================================================================
// Parameters (non-RT writes, RT reads via atomics)
// =============================================================================

void PluginProcessor::setParamValue(uint32_t param_id, double value) {
    switch (param_id) {
        case 0: dry_wet_mix_.store(static_cast<float>(value),       std::memory_order_relaxed); break;
        case 1: model_type_.store(static_cast<int>(value),           std::memory_order_relaxed); break;
        case 2: bypass_.store(static_cast<double>(value) >= 0.5,    std::memory_order_relaxed); break;
        case 3: base_context_scale_.store(static_cast<float>(value),     std::memory_order_relaxed); break;
        default: break;
    }
}

double PluginProcessor::getParamValue(uint32_t param_id) const {
    switch (param_id) {
        case 0: return static_cast<double>(dry_wet_mix_.load(std::memory_order_relaxed));
        case 1: return static_cast<double>(model_type_.load(std::memory_order_relaxed));
        case 2: return bypass_.load(std::memory_order_relaxed) ? 1.0 : 0.0;
        case 3: return static_cast<double>(base_context_scale_.load(std::memory_order_relaxed));
        default: return 0.0;
    }
}

bool PluginProcessor::isBypassed() const {
    return bypass_.load(std::memory_order_relaxed);
}

void PluginProcessor::setBypass(bool bypass) {
    bypass_.store(bypass, std::memory_order_relaxed);
}

void PluginProcessor::processParamUpdates(const std::vector<ParamSnapshot>& updates) {
    ParamUpdateAction action = param_gate_.evaluate(updates);

    // Apply parameter updates to atomics
    for (const auto& upd : updates) {
        switch (upd.param_id) {
            case 0:
                dry_wet_mix_.store(static_cast<float>(upd.value), std::memory_order_relaxed);
                break;
            case 1:
                model_type_.store(static_cast<int>(upd.value), std::memory_order_relaxed);
                break;
            case 2:
                bypass_.store(upd.value >= 0.5, std::memory_order_relaxed);
                break;
            case 3:
                base_context_scale_.store(static_cast<float>(upd.value), std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }

    if (action == ParamUpdateAction::FlushAndReinit) {
        flush_pending_.store(true, std::memory_order_release);
    }
    // UpdatePromptOnly and NoChange don't need RT signaling; the inference thread
    // would read the atomic param values directly.
}

void PluginProcessor::updateSessionFromTransport(double bpm, double tsig_num, double tsig_denom, double pos) {
    (void)bpm;
    (void)tsig_num;
    (void)tsig_denom;
    (void)pos;
    // Session-level triggers (BPM, time-sig, loop) always flush SSM state.
    flush_pending_.store(true, std::memory_order_release);
}

// =============================================================================
// Model loading (non-RT thread only)
// =============================================================================

bool PluginProcessor::loadModel(const std::string& path) {
    if (!MambaWeightLoader::load(path, *mamba_model_)) {
        model_loaded_.store(false, std::memory_order_release);
        return false;
    }
    if (!MambaWeightLoader::load(path, *snapshot_model_)) {
        model_loaded_.store(false, std::memory_order_release);
        return false;
    }
    mamba_model_->resetState();
    snapshot_model_->resetState();
    model_loaded_.store(true, std::memory_order_release);
    return true;
}

bool PluginProcessor::isModelLoaded() const {
    return model_loaded_.load(std::memory_order_relaxed);
}
