#pragma once
// EffectApplicator — translates StepOutput::effect_params into
// concrete DSP coefficient updates and applies them sample-accurately.
//
// Design: Applies DSP coefficients in the mel-spectrogram domain
// (magnitude scaling, band gain) rather than time-domain for lowest latency.
// For effects requiring time-domain processing (saturation, compression)
// the coefficients are forwarded to lightweight scalar DSP primitives.
//
// All methods are noexcept and allocation-free.

#include <cstdint>
#include <array>

namespace mamba::dsp {

struct EffectCoefficients {
    // Reverb
    float decay_s{1.5f};
    float pre_diffusion{0.5f};
    float wet_level{0.3f};
    // Compressor
    float threshold_db{-12.f};
    float ratio{4.f};
    float attack_ms{5.f};
    float release_ms{100.f};
    float makeup_db{0.f};
    // Saturation
    float drive{0.5f};
    float tone{0.5f};
    // EQ: 5-band gains in dB
    std::array<float, 5> eq_gains{0.f, 0.f, 0.f, 0.f, 0.f};
};

enum class EffectType : uint8_t {
    Reverb = 0, Compression = 1, Saturation = 2, EQ = 3
};

class EffectApplicator {
public:
    EffectApplicator();

    // Decode model effect_params[0..7] into EffectCoefficients.
    // Mapping is effect-type-specific; param layout defined by training.
    EffectCoefficients decode(EffectType type,
                               const float* params,
                               int           n_params) const noexcept;

    // Apply coefficients to a mel-spectrogram frame in-place.
    // |mel| is a pointer to n_mels floats (log-mel magnitudes).
    void apply_to_mel(EffectType            type,
                      const EffectCoefficients& coef,
                      float*                mel,
                      int                   n_mels) noexcept;

    // Apply compressor gain to a scalar sample (time-domain compression path).
    float apply_compressor_gain(float sample,
                                 const EffectCoefficients& coef,
                                 float sample_rate) noexcept;

    // Soft-clip saturation.
    static float apply_saturation(float x, float drive) noexcept;

private:
    // Compressor envelope state (per-instance, not per-step — must be reset on seek).
    float compressor_envelope_{0.f};
};

} // namespace mamba::dsp
