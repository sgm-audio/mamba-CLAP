#ifndef EFFECT_PARAMS_H
#define EFFECT_PARAMS_H

#include "mamba_config.h"

// Shared contract constants — sourced from generated mamba_config.h (ADR-004).
// Changing dimensions requires cmake reconfigure + rebuild, not source edits.
static constexpr int N_MEL            = MAMBA_N_MEL;
static constexpr int HOP_SAMPLES      = MAMBA_HOP_SAMPLES;
static constexpr int N_FFT            = MAMBA_N_FFT;
static constexpr int N_MAMBA_OUT      = MAMBA_N_MAMBA_OUT;
static constexpr int N_AUDIO_FEATURES = MAMBA_N_AUDIO_FEATURES;
static constexpr int N_SESSION_FEATURES = MAMBA_N_SESSION_FEATURES;
static constexpr int N_EFFECT_PARAMS  = MAMBA_N_EFFECT_PARAMS;

struct CompactAudioFeatures {
    float rms;
    float onset_flag;
    float spectral_centroid_norm;
    float zero_crossing_rate;
};

struct CompactSessionFeatures {
    float bpm_norm;
    float position_bars_norm;
    float key_root_norm;
    float key_mode;
};

struct EffectParamBlock {
    float eq_low_db = 0.f;
    float eq_mid_db = 0.f;
    float eq_high_db = 0.f;
    float eq_low_hz = 200.f;
    float eq_high_hz = 5000.f;
    float eq_wet = 1.f;
    // Manual-path only: explicit mid freq and Q (AI path derives mid from sqrt(lo*hi))
    float eq_mid_hz = 1000.f;
    float eq_mid_q  = 1.0f;

    float comp_thresh_db = -20.f;
    float comp_ratio = 2.f;
    float comp_attack_ms = 10.f;
    float comp_release_ms = 100.f;
    float comp_makeup_db = 0.f;
    float comp_wet = 1.f;

    float sat_drive = 0.f;
    float sat_wet = 0.f;
    float sat_tone = 0.5f;

    float rev_wet = 0.15f;
    float rev_decay_s = 1.5f;
    float rev_predelay_ms = 20.f;
    float rev_diffusion = 0.7f;
};

#endif
