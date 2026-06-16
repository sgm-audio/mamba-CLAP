#include "dsp/EffectApplicator.hpp"
#include <cmath>
#include <algorithm>

namespace mamba::dsp {

EffectApplicator::EffectApplicator() = default;

EffectCoefficients EffectApplicator::decode(EffectType type,
                                             const float* p,
                                             int n) const noexcept
{
    EffectCoefficients c;
    // Guard: clamp index access
    auto get = [&](int i, float def = 0.f) -> float {
        return (i < n) ? p[i] : def;
    };

    switch (type) {
    case EffectType::Reverb:
        c.decay_s       = 0.5f + std::abs(get(0)) * 9.5f;   // 0.5–10 s
        c.pre_diffusion = std::clamp(get(1), 0.f, 1.f);
        c.wet_level     = std::clamp(get(2), 0.f, 1.f);
        break;

    case EffectType::Compression:
        c.threshold_db  = -40.f + std::abs(get(0)) * 40.f;  // -40–0 dBFS
        c.ratio         = 1.f + std::abs(get(1)) * 19.f;    // 1:1–20:1
        c.attack_ms     = 0.1f + std::abs(get(2)) * 99.9f;  // 0.1–100 ms
        c.release_ms    = 10.f + std::abs(get(3)) * 990.f;  // 10–1000 ms
        c.makeup_db     = get(4) * 24.f;                     // ±24 dB
        break;

    case EffectType::Saturation:
        c.drive = std::clamp(get(0), 0.f, 1.f);
        c.tone  = std::clamp(get(1), 0.f, 1.f);
        break;

    case EffectType::EQ:
        for (int i = 0; i < 5; ++i)
            c.eq_gains[i] = std::clamp(get(i) * 18.f, -18.f, 18.f);  // ±18 dB
        break;
    }
    return c;
}

void EffectApplicator::apply_to_mel(EffectType type,
                                     const EffectCoefficients& c,
                                     float* mel, int n_mels) noexcept
{
    switch (type) {
    case EffectType::EQ: {
        // 5-band log-frequency split applied directly to mel bins.
        const int band_size = n_mels / 5;
        for (int b = 0; b < 5; ++b) {
            const float gain_linear = std::pow(10.f, c.eq_gains[b] / 20.f);
            const int   start = b * band_size;
            const int   end   = (b == 4) ? n_mels : start + band_size;
            for (int i = start; i < end; ++i)
                mel[i] += std::log(std::max(gain_linear, 1e-6f));
        }
        break;
    }
    case EffectType::Reverb:
        // Reverb adds spectral energy (simplified: boost all bins slightly).
        for (int i = 0; i < n_mels; ++i)
            mel[i] += c.wet_level * 0.1f;
        break;
    default:
        break;  // Compression and saturation act on time-domain samples.
    }
}

float EffectApplicator::apply_compressor_gain(float sample,
                                               const EffectCoefficients& c,
                                               float sr) noexcept
{
    const float abs_sample = std::abs(sample);
    const float threshold  = std::pow(10.f, c.threshold_db / 20.f);
    const float attack_coef  = std::exp(-1.f / (c.attack_ms  * 0.001f * sr));
    const float release_coef = std::exp(-1.f / (c.release_ms * 0.001f * sr));

    // Peak envelope follower.
    if (abs_sample > compressor_envelope_)
        compressor_envelope_ = attack_coef  * compressor_envelope_ + (1.f - attack_coef)  * abs_sample;
    else
        compressor_envelope_ = release_coef * compressor_envelope_ + (1.f - release_coef) * abs_sample;

    if (compressor_envelope_ <= threshold)
        return 1.f;

    // Gain computation: GR in dB.
    const float input_db  = 20.f * std::log10(std::max(compressor_envelope_, 1e-9f));
    const float output_db = c.threshold_db + (input_db - c.threshold_db) / c.ratio;
    const float gr_db     = output_db - input_db + c.makeup_db;
    return std::pow(10.f, gr_db / 20.f);
}

float EffectApplicator::apply_saturation(float x, float drive) noexcept {
    // Arctangent soft-clip: y = (2/π) · atan(π/2 · x · drive)
    const float d = 1.f + drive * 9.f;  // 1–10× drive
    return (2.f / 3.14159265f) * std::atan(1.5707963f * x * d);
}

} // namespace mamba::dsp
