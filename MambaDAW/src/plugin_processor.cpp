#include "plugin_processor.h"
#include "mamba_weight_loader.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <new>

namespace {

void storeEffectParamsToAtomics(std::array<std::atomic<float>, N_EFFECT_PARAMS>& atomics,
const EffectParamBlock& p) {
    size_t k = 0;
    atomics[k++].store(p.eq_low_db, std::memory_order_relaxed);
    atomics[k++].store(p.eq_mid_db, std::memory_order_relaxed);
    atomics[k++].store(p.eq_high_db, std::memory_order_relaxed);
    atomics[k++].store(p.eq_low_hz, std::memory_order_relaxed);
    atomics[k++].store(p.eq_high_hz, std::memory_order_relaxed);
    atomics[k++].store(p.eq_wet, std::memory_order_relaxed);
    atomics[k++].store(p.comp_thresh_db, std::memory_order_relaxed);
    atomics[k++].store(p.comp_ratio, std::memory_order_relaxed);
    atomics[k++].store(p.comp_attack_ms, std::memory_order_relaxed);
    atomics[k++].store(p.comp_release_ms, std::memory_order_relaxed);
    atomics[k++].store(p.comp_makeup_db, std::memory_order_relaxed);
    atomics[k++].store(p.comp_wet, std::memory_order_relaxed);
    atomics[k++].store(p.sat_drive, std::memory_order_relaxed);
    atomics[k++].store(p.sat_wet, std::memory_order_relaxed);
    atomics[k++].store(p.sat_tone, std::memory_order_relaxed);
    atomics[k++].store(p.rev_wet, std::memory_order_relaxed);
    atomics[k++].store(p.rev_decay_s, std::memory_order_relaxed);
    atomics[k++].store(p.rev_predelay_ms, std::memory_order_relaxed);
    atomics[k++].store(p.rev_diffusion, std::memory_order_relaxed);
}

EffectParamBlock effectBlockFromAtomics(const std::array<std::atomic<float>, N_EFFECT_PARAMS>& atomics) {
    EffectParamBlock p{};
    size_t k = 0;
    p.eq_low_db = atomics[k++].load(std::memory_order_relaxed);
    p.eq_mid_db = atomics[k++].load(std::memory_order_relaxed);
    p.eq_high_db = atomics[k++].load(std::memory_order_relaxed);
    p.eq_low_hz = atomics[k++].load(std::memory_order_relaxed);
    p.eq_high_hz = atomics[k++].load(std::memory_order_relaxed);
    p.eq_wet = atomics[k++].load(std::memory_order_relaxed);
    p.comp_thresh_db = atomics[k++].load(std::memory_order_relaxed);
    p.comp_ratio = atomics[k++].load(std::memory_order_relaxed);
    p.comp_attack_ms = atomics[k++].load(std::memory_order_relaxed);
    p.comp_release_ms = atomics[k++].load(std::memory_order_relaxed);
    p.comp_makeup_db = atomics[k++].load(std::memory_order_relaxed);
    p.comp_wet = atomics[k++].load(std::memory_order_relaxed);
    p.sat_drive = atomics[k++].load(std::memory_order_relaxed);
    p.sat_wet = atomics[k++].load(std::memory_order_relaxed);
    p.sat_tone = atomics[k++].load(std::memory_order_relaxed);
    p.rev_wet = atomics[k++].load(std::memory_order_relaxed);
    p.rev_decay_s = atomics[k++].load(std::memory_order_relaxed);
    p.rev_predelay_ms = atomics[k++].load(std::memory_order_relaxed);
    p.rev_diffusion = atomics[k++].load(std::memory_order_relaxed);
    return p;
}

} // namespace

PluginProcessor::PluginProcessor(int sample_rate)
: sample_rate_(sample_rate),
    param_gate_(),
    param_smoother_(),
    feature_extractor_(sample_rate),
    inference_thread_(nullptr),
    cpu_budget_(static_cast<double>(sample_rate)) {
try {
  auto engine = std::make_unique<MambaInferenceEngine>();
  mamba_engine_ = engine.get();
  inference_thread_ = std::make_unique<InferenceThread>(std::move(engine));

        mamba_model_ = std::make_unique<MambaModel>(N_MEL, MAMBA_N_LAYERS, MAMBA_D_STATE, MAMBA_D_CONV, MAMBA_EXPAND);
  inference_thread_->setMambaModel(mamba_model_.get());
} catch (const std::bad_alloc&) {
  degraded_.store(true, std::memory_order_relaxed);
  mamba_engine_ = nullptr;
  mamba_model_ = nullptr;
  inference_thread_ = nullptr;
}

  EffectParamBlock defaults{};
  storeEffectParamsToAtomics(ai_params_, defaults);
  model_loaded_.store(false, std::memory_order_relaxed);

  param_smoother_.setTarget(0, 0.5f);
  param_smoother_.setTarget(3, 1.0f);
  param_smoother_.setTarget(4, 1.0f);
  param_smoother_.setTarget(5, 60.0f);
  param_smoother_.setTarget(6, 1.0f);
  param_smoother_.setTarget(7, 1.0f);
  param_smoother_.setTarget(8, 0.0f);
  param_smoother_.setTarget(9, 0.15f);
  param_smoother_.setTarget(10, -20.0f);
  param_smoother_.setTarget(11, 1.5f);
  param_smoother_.reset();
}

PluginProcessor::~PluginProcessor() {
  if (inference_thread_) inference_thread_->stop();
}

void PluginProcessor::activate(double sample_rate,
    uint32_t /*min_frame_count*/, uint32_t /*max_frame_count*/) {
    const bool sr_changed = (static_cast<int>(sample_rate) != sample_rate_);
    sample_rate_ = static_cast<int>(sample_rate);
    const float sr = static_cast<float>(sample_rate);
    dsp_chain_.setSampleRate(sr);
    param_smoother_.setSampleRate(sr);
    feature_extractor_.setSampleRate(sample_rate_);
    cpu_budget_.setSampleRate(static_cast<double>(sample_rate));
    if (sr_changed) {
        feature_extractor_.reset();
        mel_hop_counter_ = 0;
        if (mamba_model_) mamba_model_->resetState();
        param_gate_.reset();
        cpu_budget_.reset();
    }
    dsp_chain_.setParams(effectBlockFromAtomics(ai_params_));
    param_smoother_.reset();
    flush_pending_.store(false, std::memory_order_release);
    nan_reset_pending_.store(false, std::memory_order_release);
    nan_count_ = 0;
    if (inference_thread_) {
        inference_thread_->stop();
        inference_thread_->setSampleRate(sr);
        inference_thread_->setContextLengthSeconds(context_length_s_.load(std::memory_order_relaxed));
        inference_thread_->start();
    }
    activated_.store(true, std::memory_order_release);
}

void PluginProcessor::deactivate() {
  activated_.store(false, std::memory_order_release);
  if (inference_thread_) inference_thread_->stop();
  if (mamba_model_) mamba_model_->resetState();
}

void PluginProcessor::resetState() {
  if (mamba_model_) mamba_model_->resetState();
  feature_extractor_.reset();
  mel_hop_counter_ = 0;
  param_gate_.reset();
  flush_pending_.store(false, std::memory_order_release);
  nan_reset_pending_.store(false, std::memory_order_release);
  nan_count_ = 0;
}

void PluginProcessor::buildManualEffectBlock(EffectParamBlock& out) const {
    // RT-Safety: reads atomics (relaxed) and param_smoother_ values only. No allocation.
    const bool global = param_smoother_.getValue(1) >= 0.5f;

    // --- EQ ---
    if (global || eq_manual_enable_.load(std::memory_order_relaxed)) {
        out.eq_low_db   = manual_eq_lo_gain_.load(std::memory_order_relaxed);
        out.eq_low_hz   = manual_eq_lo_freq_.load(std::memory_order_relaxed);
        out.eq_mid_db   = manual_eq_mid_gain_.load(std::memory_order_relaxed);
        out.eq_mid_hz   = manual_eq_mid_freq_.load(std::memory_order_relaxed);
        out.eq_mid_q    = manual_eq_mid_q_.load(std::memory_order_relaxed);
        out.eq_high_db  = manual_eq_hi_gain_.load(std::memory_order_relaxed);
        out.eq_high_hz  = manual_eq_hi_freq_.load(std::memory_order_relaxed);
    } else {
        out.eq_low_db   = ai_params_[0].load(std::memory_order_relaxed);
        out.eq_mid_db   = ai_params_[1].load(std::memory_order_relaxed);
        out.eq_high_db  = ai_params_[2].load(std::memory_order_relaxed);
        out.eq_low_hz   = ai_params_[3].load(std::memory_order_relaxed);
        out.eq_high_hz  = ai_params_[4].load(std::memory_order_relaxed);
        out.eq_mid_hz   = std::sqrt(out.eq_low_hz * out.eq_high_hz);
        out.eq_mid_q    = 1.0f;
    }
    out.eq_wet = param_smoother_.getValue(6);

    // --- Compressor ---
    if (global || comp_manual_enable_.load(std::memory_order_relaxed)) {
        out.comp_thresh_db  = param_smoother_.getValue(10);
        out.comp_ratio      = manual_comp_ratio_.load(std::memory_order_relaxed);
        out.comp_attack_ms  = manual_comp_attack_ms_.load(std::memory_order_relaxed);
        out.comp_release_ms = manual_comp_release_ms_.load(std::memory_order_relaxed);
        out.comp_makeup_db  = manual_comp_makeup_db_.load(std::memory_order_relaxed);
    } else {
        out.comp_thresh_db  = ai_params_[6].load(std::memory_order_relaxed);
        out.comp_ratio      = ai_params_[7].load(std::memory_order_relaxed);
        out.comp_attack_ms  = ai_params_[8].load(std::memory_order_relaxed);
        out.comp_release_ms = ai_params_[9].load(std::memory_order_relaxed);
        out.comp_makeup_db  = ai_params_[10].load(std::memory_order_relaxed);
    }
    out.comp_wet = param_smoother_.getValue(7);

    // --- Saturator ---
    if (global || sat_manual_enable_.load(std::memory_order_relaxed)) {
        out.sat_drive = manual_sat_drive_.load(std::memory_order_relaxed);
        // Convert -1..1 to 0..1 so Saturator's clamp01 gives full range
        out.sat_tone  = (manual_sat_tone_.load(std::memory_order_relaxed) + 1.0f) * 0.5f;
    } else {
        out.sat_drive = ai_params_[12].load(std::memory_order_relaxed);
        out.sat_tone  = ai_params_[14].load(std::memory_order_relaxed);
    }
    out.sat_wet = param_smoother_.getValue(8);

    // --- Reverb ---
    if (global || rev_manual_enable_.load(std::memory_order_relaxed)) {
        out.rev_decay_s     = param_smoother_.getValue(11);
        out.rev_predelay_ms = manual_rev_predelay_ms_.load(std::memory_order_relaxed);
        out.rev_diffusion   = manual_rev_diffusion_.load(std::memory_order_relaxed);
    } else {
        out.rev_wet         = ai_params_[15].load(std::memory_order_relaxed);
        out.rev_decay_s     = ai_params_[16].load(std::memory_order_relaxed);
        out.rev_predelay_ms = ai_params_[17].load(std::memory_order_relaxed);
        out.rev_diffusion   = ai_params_[18].load(std::memory_order_relaxed);
    }
    out.rev_wet = param_smoother_.getValue(9);
}

void PluginProcessor::readInferenceParams(EffectParamBlock& out) {
  if (!inference_thread_) {
    out = effectBlockFromAtomics(ai_params_);
    return;
  }
    if (inference_thread_->effectParamBuffer().try_read(out)) {
        float ew = param_smoother_.getValue(6);
        float cw = param_smoother_.getValue(7);
        float sw = param_smoother_.getValue(8);
        float rw = param_smoother_.getValue(9);
        out.eq_wet = ew;
        out.comp_wet = cw;
        out.eq_low_db *= ew;
        out.eq_mid_db *= ew;
        out.eq_high_db *= ew;
        out.comp_thresh_db = (1.f - cw) * -60.f + cw * out.comp_thresh_db;
        out.rev_wet *= rw;
        out.sat_wet *= sw;
        // Seed explicit mid fields so Eq::setParams dirty-check doesn't re-cook every block
        out.eq_mid_hz = std::sqrt(out.eq_low_hz * out.eq_high_hz);
        out.eq_mid_q  = 1.0f;
        storeEffectParamsToAtomics(ai_params_, out);
  } else {
    out = effectBlockFromAtomics(ai_params_);
  }
}

void PluginProcessor::processBlock(float** inputs, float** outputs,
    int32_t num_frames,
    int32_t num_input_channels,
    int32_t num_output_channels) {
    /* RT-Safety Justification: beginBlock()/endBlock() use clock_gettime(CLOCK_MONOTONIC)
       which is async-signal-safe and lock-free per POSIX. No heap allocation, no mutex.
       All CpuBudgetMonitor state is pre-allocated fixed-size arrays. */
    cpu_budget_.beginBlock();

    const bool is_active = activated_.load(std::memory_order_acquire);
    const bool is_bypass = bypass_.load(std::memory_order_acquire);
    const bool has_model = model_loaded_.load(std::memory_order_acquire);
    const bool is_degraded = degraded_.load(std::memory_order_acquire);
    const bool is_overloaded = cpu_budget_.isOverloaded();
    cpu_overload_.store(is_overloaded, std::memory_order_relaxed);
    const bool flush_now = flush_pending_.exchange(false, std::memory_order_acq_rel);

    if (nan_reset_pending_.exchange(false, std::memory_order_acq_rel)) {
        if (mamba_model_) mamba_model_->resetState();
        feature_extractor_.reset();
        mel_hop_counter_ = 0;
        nan_count_ = 0;
    }

 if (!is_active || is_bypass || is_degraded) {
 const int32_t copy_ch = std::min(num_input_channels, num_output_channels);
 for (int32_t c = 0; c < copy_ch; ++c)
 std::copy_n(inputs[c], num_frames, outputs[c]);
 for (int32_t c = copy_ch; c < num_output_channels; ++c)
 std::fill_n(outputs[c], num_frames, 0.0f);
 cpu_budget_.endBlock();
 return;
 }

    param_smoother_.processAll();

    // Under sustained CPU overload (>30% for >=32 consecutive blocks), bypass
    // DSP processing and pass dry audio through to prevent buffer underruns.
    if (is_overloaded) {
        const float out_gain = param_smoother_.getValue(4);
        const int32_t copy_ch = std::min(num_input_channels, num_output_channels);
        for (int32_t c = 0; c < copy_ch; ++c) {
            for (int32_t i = 0; i < num_frames; ++i)
                outputs[c][i] = inputs[c][i] * out_gain;
        }
        for (int32_t c = copy_ch; c < num_output_channels; ++c)
            std::fill_n(outputs[c], num_frames, 0.0f);
        cpu_budget_.endBlock();
        return;
    }

  const float wet = param_smoother_.getValue(0);
  const float dry = 1.0f - wet;
  const float in_gain = param_smoother_.getValue(3);
  const float out_gain = param_smoother_.getValue(4);
  const bool global_manual = param_smoother_.getValue(1) >= 0.5f;
  const bool is_manual = global_manual
      || eq_manual_enable_.load(std::memory_order_relaxed)
      || comp_manual_enable_.load(std::memory_order_relaxed)
      || sat_manual_enable_.load(std::memory_order_relaxed)
      || rev_manual_enable_.load(std::memory_order_relaxed);
  const float ctx_seconds = param_smoother_.getValue(5);
  const float ctx_scale = (ctx_seconds - kContextLenMin) / (kContextLenMax - kContextLenMin);
  if (inference_thread_) inference_thread_->setContextScale(ctx_scale);

  const int et = static_cast<int>(effect_type_.load(std::memory_order_relaxed));
  dsp_chain_.setEffectType(static_cast<EffectType>(et));

    if (flush_now) {
        if (mamba_model_) mamba_model_->resetState();
        feature_extractor_.reset();
        mel_hop_counter_ = 0;
        reset_context_.store(false, std::memory_order_release);
        reinit_pending_.store(true, std::memory_order_release);
    }

  if (is_manual) {
        EffectParamBlock manual_params{};
        buildManualEffectBlock(manual_params);
        dsp_chain_.setParams(manual_params);
    } else {
        EffectParamBlock inferred_params{};
        readInferenceParams(inferred_params);
        dsp_chain_.setParams(inferred_params);
    }

  for (int32_t idx = 0; idx < num_frames;) {
    const int32_t chunk =
        std::min<int32_t>(kDspMaxBlockFrames, num_frames - idx);

    for (int32_t i = 0; i < chunk; ++i) {
      float m = 0.0f;
      for (int32_t c = 0; c < num_input_channels; ++c)
        m += inputs[c][idx + i] * in_gain;
      if (num_input_channels > 1)
        m /= static_cast<float>(num_input_channels);
      mel_mono_scratch_[static_cast<size_t>(i)] = m;
    }

 if (!is_manual && has_model && !dsp_chain_.isSilent()) {
			int n_mel = feature_extractor_.pushInterleaved(
				mel_mono_scratch_.data(), chunk, 1, mel_scratch_.data());
			for (int f = 0; f < n_mel; ++f) {
				AudioFeatures rt_feats = extract_features(
					mel_mono_scratch_.data(), chunk, static_cast<double>(sample_rate_));
				CompactAudioFeatures compact_feats{};
				compact_feats.rms = rt_feats.rms;
				compact_feats.onset_flag = rt_feats.onset_detected ? 1.f : 0.f;
				compact_feats.spectral_centroid_norm = rt_feats.spectral_centroid /
					(static_cast<float>(sample_rate_) * 0.5f);
				compact_feats.zero_crossing_rate = rt_feats.zero_crossing_rate;
        if (inference_thread_) {
          (void)inference_thread_->enqueueMelFrame(
            mel_scratch_.data() + f * N_MEL, compact_feats);
        }
			}
		}

		for (int32_t i = 0; i < chunk; ++i) {
			if (num_input_channels >= 2) {
				dsp_dry_l_[static_cast<size_t>(i)] = inputs[0][idx + i] * in_gain;
				dsp_dry_r_[static_cast<size_t>(i)] = inputs[1][idx + i] * in_gain;
			} else {
				dsp_dry_l_[static_cast<size_t>(i)] = inputs[0][idx + i] * in_gain;
				dsp_dry_r_[static_cast<size_t>(i)] = inputs[0][idx + i] * in_gain;
			}
			if (!std::isfinite(dsp_dry_l_[static_cast<size_t>(i)])) {
				dsp_dry_l_[static_cast<size_t>(i)] = 0.0f;
				++nan_count_;
			}
			if (!std::isfinite(dsp_dry_r_[static_cast<size_t>(i)])) {
				dsp_dry_r_[static_cast<size_t>(i)] = 0.0f;
				++nan_count_;
			}
		}

		if (nan_count_ >= kNanThreshold) {
			nan_reset_pending_.store(true, std::memory_order_release);
			nan_count_ = 0;
		}

    const int dsp_in_ch = (num_input_channels >= 2) ? 2 : 1;
    float* in_ptrs[2] = {dsp_dry_l_.data(), dsp_dry_r_.data()};
    float* out_ptrs[2] = {dsp_wet_l_.data(), dsp_wet_r_.data()};
        dsp_chain_.processBlock(in_ptrs, out_ptrs, chunk, dsp_in_ch, 2);

        if (inference_thread_) {
            inference_thread_->setPaused(dsp_chain_.isSilent());
        }

    for (int32_t i = 0; i < chunk; ++i) {
      const int32_t g = idx + i;

		float wet_l = dsp_wet_l_[static_cast<size_t>(i)];
		float wet_r = dsp_wet_r_[static_cast<size_t>(i)];
		if (!std::isfinite(wet_l)) { wet_l = 0.0f; ++nan_count_; }
		if (!std::isfinite(wet_r)) { wet_r = 0.0f; ++nan_count_; }

      const float dry_l = dsp_dry_l_[static_cast<size_t>(i)];
      const float dry_r = dsp_dry_r_[static_cast<size_t>(i)];
      const float mixed_l = std::tanh(dry * dry_l + wet * wet_l) * out_gain;
      const float mixed_r = std::tanh(dry * dry_r + wet * wet_r) * out_gain;

      if (num_output_channels >= 2) {
        outputs[0][g] = mixed_l;
        outputs[1][g] = mixed_r;
        for (int32_t c = 2; c < num_output_channels; ++c)
          outputs[c][g] = 0.0f;
      } else {
        outputs[0][g] = 0.5f * (mixed_l + mixed_r);
      }
    }

    idx += chunk;
  }

  // Update output peak levels for the GUI level meter (audio thread, relaxed store).
  float pk_l = 0.f, pk_r = 0.f;
  for (int32_t i = 0; i < num_frames; ++i) {
    pk_l = std::max(pk_l, std::abs(outputs[0][i]));
    pk_r = (num_output_channels > 1) ? std::max(pk_r, std::abs(outputs[1][i])) : pk_l;
  }
    level_peak_l_.store(pk_l, std::memory_order_relaxed);
    level_peak_r_.store(pk_r, std::memory_order_relaxed);

    cpu_budget_.endBlock();
}

void PluginProcessor::setParamValue(uint32_t param_id, double value) {
  const float fv = static_cast<float>(value);
  switch (param_id) {
  case 0: dry_wet_mix_.store(fv, std::memory_order_relaxed); param_smoother_.setTarget(0, fv); break;
  case 1: effect_mode_.store(static_cast<int>(fv), std::memory_order_relaxed); param_smoother_.setTarget(1, fv); break;
  case 2: bypass_.store(value >= 0.5, std::memory_order_relaxed); param_smoother_.setTarget(2, value >= 0.5 ? 1.f : 0.f); break;
  case 3: input_gain_.store(fv, std::memory_order_relaxed); param_smoother_.setTarget(3, fv); break;
  case 4: output_gain_.store(fv, std::memory_order_relaxed); param_smoother_.setTarget(4, fv); break;
    case 5: context_length_s_.store(fv, std::memory_order_relaxed); param_smoother_.setTarget(5, fv); if (inference_thread_) inference_thread_->setContextLengthSeconds(fv); break;
  case 6: eq_wet_.store(fv, std::memory_order_relaxed); param_smoother_.setTarget(6, fv); break;
  case 7: comp_wet_.store(fv, std::memory_order_relaxed); param_smoother_.setTarget(7, fv); break;
  case 8: sat_wet_.store(fv, std::memory_order_relaxed); param_smoother_.setTarget(8, fv); break;
  case 9: rev_wet_.store(fv, std::memory_order_relaxed); param_smoother_.setTarget(9, fv); break;
  case 10: manual_comp_thresh_db_.store(fv, std::memory_order_relaxed); param_smoother_.setTarget(10, fv); break;
  case 11: manual_rev_decay_s_.store(fv, std::memory_order_relaxed); param_smoother_.setTarget(11, fv); break;
  case 12: effect_type_.store(static_cast<int>(fv), std::memory_order_relaxed); break;
    case 13: if (fv >= 0.5f) { flush_pending_.store(true, std::memory_order_release); reset_context_.store(true, std::memory_order_release); } break;
    // Per-section manual enables
    case 14: eq_manual_enable_.store(fv >= 0.5f, std::memory_order_relaxed); break;
    case 15: comp_manual_enable_.store(fv >= 0.5f, std::memory_order_relaxed); break;
    case 16: sat_manual_enable_.store(fv >= 0.5f, std::memory_order_relaxed); break;
    case 17: rev_manual_enable_.store(fv >= 0.5f, std::memory_order_relaxed); break;
    // Manual EQ params
    case 18: manual_eq_lo_gain_.store(fv, std::memory_order_relaxed); break;
    case 19: manual_eq_lo_freq_.store(fv, std::memory_order_relaxed); break;
    case 20: manual_eq_mid_gain_.store(fv, std::memory_order_relaxed); break;
    case 21: manual_eq_mid_freq_.store(fv, std::memory_order_relaxed); break;
    case 22: manual_eq_mid_q_.store(fv, std::memory_order_relaxed); break;
    case 23: manual_eq_hi_gain_.store(fv, std::memory_order_relaxed); break;
    case 24: manual_eq_hi_freq_.store(fv, std::memory_order_relaxed); break;
    // Manual compressor extra params
    case 25: manual_comp_ratio_.store(fv, std::memory_order_relaxed); break;
    case 26: manual_comp_attack_ms_.store(fv, std::memory_order_relaxed); break;
    case 27: manual_comp_release_ms_.store(fv, std::memory_order_relaxed); break;
    case 28: manual_comp_makeup_db_.store(fv, std::memory_order_relaxed); break;
    // Manual saturator
    case 29: manual_sat_drive_.store(fv, std::memory_order_relaxed); break;
    case 30: manual_sat_tone_.store(fv, std::memory_order_relaxed); break;
    // Manual reverb extra params
    case 31: manual_rev_predelay_ms_.store(fv, std::memory_order_relaxed); break;
    case 32: manual_rev_diffusion_.store(fv, std::memory_order_relaxed); break;
    default: break;
  }
}

double PluginProcessor::getParamValue(uint32_t param_id) const {
  switch (param_id) {
  case 0: return static_cast<double>(dry_wet_mix_.load(std::memory_order_relaxed));
  case 1: return static_cast<double>(effect_mode_.load(std::memory_order_relaxed));
  case 2: return bypass_.load(std::memory_order_relaxed) ? 1.0 : 0.0;
  case 3: return static_cast<double>(input_gain_.load(std::memory_order_relaxed));
  case 4: return static_cast<double>(output_gain_.load(std::memory_order_relaxed));
  case 5: return static_cast<double>(context_length_s_.load(std::memory_order_relaxed));
  case 6: return static_cast<double>(eq_wet_.load(std::memory_order_relaxed));
  case 7: return static_cast<double>(comp_wet_.load(std::memory_order_relaxed));
  case 8: return static_cast<double>(sat_wet_.load(std::memory_order_relaxed));
  case 9: return static_cast<double>(rev_wet_.load(std::memory_order_relaxed));
  case 10: return static_cast<double>(manual_comp_thresh_db_.load(std::memory_order_relaxed));
  case 11: return static_cast<double>(manual_rev_decay_s_.load(std::memory_order_relaxed));
    case 12: return static_cast<double>(effect_type_.load(std::memory_order_relaxed));
    case 13: return 0.0;
    case 14: return eq_manual_enable_.load(std::memory_order_relaxed) ? 1.0 : 0.0;
    case 15: return comp_manual_enable_.load(std::memory_order_relaxed) ? 1.0 : 0.0;
    case 16: return sat_manual_enable_.load(std::memory_order_relaxed) ? 1.0 : 0.0;
    case 17: return rev_manual_enable_.load(std::memory_order_relaxed) ? 1.0 : 0.0;
    case 18: return static_cast<double>(manual_eq_lo_gain_.load(std::memory_order_relaxed));
    case 19: return static_cast<double>(manual_eq_lo_freq_.load(std::memory_order_relaxed));
    case 20: return static_cast<double>(manual_eq_mid_gain_.load(std::memory_order_relaxed));
    case 21: return static_cast<double>(manual_eq_mid_freq_.load(std::memory_order_relaxed));
    case 22: return static_cast<double>(manual_eq_mid_q_.load(std::memory_order_relaxed));
    case 23: return static_cast<double>(manual_eq_hi_gain_.load(std::memory_order_relaxed));
    case 24: return static_cast<double>(manual_eq_hi_freq_.load(std::memory_order_relaxed));
    case 25: return static_cast<double>(manual_comp_ratio_.load(std::memory_order_relaxed));
    case 26: return static_cast<double>(manual_comp_attack_ms_.load(std::memory_order_relaxed));
    case 27: return static_cast<double>(manual_comp_release_ms_.load(std::memory_order_relaxed));
    case 28: return static_cast<double>(manual_comp_makeup_db_.load(std::memory_order_relaxed));
    case 29: return static_cast<double>(manual_sat_drive_.load(std::memory_order_relaxed));
    case 30: return static_cast<double>(manual_sat_tone_.load(std::memory_order_relaxed));
    case 31: return static_cast<double>(manual_rev_predelay_ms_.load(std::memory_order_relaxed));
    case 32: return static_cast<double>(manual_rev_diffusion_.load(std::memory_order_relaxed));
    default: return 0.0;
  }
}

bool PluginProcessor::isBypassed() const {
    return bypass_.load(std::memory_order_relaxed);
}

void PluginProcessor::setBypass(bool bypass) {
  bypass_.store(bypass, std::memory_order_relaxed);
}

void PluginProcessor::markParamGuiOrigin(uint32_t param_id) {
  if (param_id < kNumClapParams)
    gui_suppress_[param_id].store(kGuiSuppressMax, std::memory_order_release);
}

bool PluginProcessor::isParamGuiSuppressed(uint32_t param_id) const {
  if (param_id >= kNumClapParams) return false;
  return gui_suppress_[param_id].load(std::memory_order_acquire) > 0;
}

void PluginProcessor::tickParamGuiSuppress() {
  for (uint32_t i = 0; i < kNumClapParams; ++i) {
    int v = gui_suppress_[i].load(std::memory_order_relaxed);
    if (v > 0) gui_suppress_[i].store(v - 1, std::memory_order_release);
  }
}

void PluginProcessor::processParamUpdates(const std::vector<ParamSnapshot>& updates) {
  for (const auto& upd : updates) {
    if (upd.param_id < kNumClapParams && isParamGuiSuppressed(upd.param_id))
      continue;
    setParamValue(upd.param_id, upd.value);
  }
  ParamUpdateAction action = param_gate_.evaluate(updates);
  if (action == ParamUpdateAction::FlushAndReinit) {
    flush_pending_.store(true, std::memory_order_release);
  }
}

void PluginProcessor::processParamUpdates(const ParamSnapshot* updates, uint32_t count) {
  for (uint32_t i = 0; i < count; ++i) {
    if (updates[i].param_id < kNumClapParams && isParamGuiSuppressed(updates[i].param_id))
      continue;
    setParamValue(updates[i].param_id, updates[i].value);
  }
  ParamUpdateAction action = param_gate_.evaluate(updates, count);
  if (action == ParamUpdateAction::FlushAndReinit) {
    flush_pending_.store(true, std::memory_order_release);
  }
}

void PluginProcessor::updateSessionFromTransport(double bpm, double tsig_num, double tsig_denom, double pos,
    bool is_playing, bool is_looping, bool /*has_seeked*/) {
    (void)is_playing; (void)is_looping; (void)tsig_num; (void)tsig_denom;
    if (inference_thread_) {
        CompactSessionFeatures sess{};
        sess.bpm_norm = static_cast<float>(std::clamp(bpm / 300.0, 0.0, 1.0));
        sess.position_bars_norm = static_cast<float>(std::clamp(pos / 512.0, 0.0, 1.0));
        sess.key_root_norm = 0.f;
        sess.key_mode = 0.f;
        inference_thread_->setSessionFeatures(sess);
    }
}

void PluginProcessor::handleDeferredReinit() {
  if (!inference_thread_) return;
  if (!reinit_pending_.exchange(false, std::memory_order_acq_rel))
    if (!nan_reset_pending_.exchange(false, std::memory_order_acq_rel))
      return;
  inference_thread_->stop();
  inference_thread_->start();
}

bool PluginProcessor::hasDeferredReinit() const {
	return reinit_pending_.load(std::memory_order_acquire) ||
		nan_reset_pending_.load(std::memory_order_acquire);
}

bool PluginProcessor::loadModel(const std::string& path) {
  if (!mamba_model_) {
    degraded_.store(true, std::memory_order_relaxed);
    return false;
  }
  if (!MambaWeightLoader::load(path, *mamba_model_)) {
    model_loaded_.store(false, std::memory_order_release);
    return false;
  }
  mamba_model_->resetState();
  weight_path_ = path;

        auto proj = MambaWeightLoader::loadProjections(path);
        if (proj.valid && mamba_engine_) {
            mamba_engine_->setProjectionWeights(
                proj.head_w.data(), proj.head_b.data(), proj.head_out);
            if (proj.has_ctx_proj)
                mamba_engine_->setContextProjectionWeights(
                    proj.ctx_proj_w.data(), proj.ctx_proj_b.data(), proj.head_out);
            if (proj.has_feat_proj)
                mamba_engine_->setFeatureProjectionWeights(
                    proj.feat_proj_w.data(), proj.feat_proj_b.data(),
                    proj.head_out, proj.n_audio_features);
        }

  model_loaded_.store(true, std::memory_order_release);
  return true;
}

bool PluginProcessor::isModelLoaded() const {
    return model_loaded_.load(std::memory_order_relaxed);
}

float PluginProcessor::getContextFillRatio() const noexcept {
    return inference_thread_ ? inference_thread_->contextFillRatio() : 0.f;
}

bool PluginProcessor::isInferenceActive() const noexcept {
    return inference_thread_ &&
           inference_thread_->is_running() &&
           !inference_thread_->isPaused();
}

float PluginProcessor::getAIParamValue(int index) const noexcept {
    if (index < 0 || index >= N_EFFECT_PARAMS) return 0.f;
    return ai_params_[static_cast<size_t>(index)].load(std::memory_order_relaxed);
}

std::vector<uint8_t> PluginProcessor::saveState() {
 StateSerialization::PluginState ps;
 for (uint32_t i = 0; i < kNumClapParams; ++i)
  ps.params[i] = getParamValue(i);
 ps.weight_path = weight_path_;
 if (mamba_model_) {
  bool was_running = inference_thread_ && inference_thread_->is_running();
  if (was_running) inference_thread_->stop();
  ps.model_state = mamba_model_->getState();
  ps.model_config_checksum = StateSerialization::computeModelConfigChecksum(
   mamba_model_->dModel(), mamba_model_->numLayers(),
   mamba_model_->dState(), mamba_model_->dConv(), mamba_model_->expand());
  if (was_running) inference_thread_->start();
 }
 return StateSerialization::encode(ps);
}

bool PluginProcessor::loadState(const uint8_t* data, uint32_t size) {
  StateSerialization::PluginState ps;
  if (!StateSerialization::decode(data, size, ps))
    return false;
  for (uint32_t i = 0; i < kNumClapParams; ++i)
    setParamValue(i, ps.params[i]);
  if (!ps.weight_path.empty()) {
    ps.weight_path = StateSerialization::canonicalizeWeightPath(ps.weight_path);
    if (!loadModel(ps.weight_path))
      if (mamba_model_) mamba_model_->resetState();
  }
  if (mamba_model_) {
    uint32_t expected_checksum = StateSerialization::computeModelConfigChecksum(
      mamba_model_->dModel(), mamba_model_->numLayers(),
      mamba_model_->dState(), mamba_model_->dConv(), mamba_model_->expand());
        if (ps.model_config_checksum != 0 && ps.model_config_checksum != expected_checksum) {
            checksum_mismatch_.store(true, std::memory_order_release);
            mamba_model_->resetState();
            return true;
        }
    if (!mamba_model_->setState(ps.model_state))
      mamba_model_->resetState();
  }
  return true;
}
