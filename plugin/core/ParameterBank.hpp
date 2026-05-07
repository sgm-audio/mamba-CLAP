#pragma once
// Central parameter registry.
//   • Each parameter is stored as a std::atomic<float> — reads from the
//     real-time audio thread are always wait-free.
//   • The bank also owns the clap_param_info_t descriptors so the CLAP
//     extension layer can answer host queries without touching the audio path.
//   • Value normalisation (0.0–1.0 plain ↔ physical units) lives here so
//     all layers share the same conversion logic.

#include <atomic>
#include <array>
#include <cstdint>
#include <string_view>
#include <span>
#include <clap/clap.h>

namespace mamba::core {

// ─── Parameter IDs ────────────────────────────────────────────────────────────
// Stable IDs: never reorder or reuse; add new ones at the end.
enum class ParamId : uint32_t {
    EffectType      = 0,   // 0=reverb 1=compress 2=saturate 3=eq
    WetMix          = 1,   // 0.0 – 1.0
    InputGain       = 2,   // -24 dB – +24 dB
    OutputGain      = 3,   // -24 dB – +24 dB
    ContextLength   = 4,   // 1.0 s – 120.0 s (informational for model)
    ModelVariant    = 5,   // 0=tiny(20MB) 1=base(80MB) 2=large(150MB)
    InferenceDevice = 6,   // 0=CPU 1=CUDA (if available)
    Bypass          = 7,   // 0=off 1=on
    COUNT           = 8
};

static constexpr uint32_t kParamCount = static_cast<uint32_t>(ParamId::COUNT);

// ─── Default and range tables ─────────────────────────────────────────────────
struct ParamMeta {
    const char*   name;
    const char*   module;       // path group shown in host automation lane
    double        min_value;
    double        max_value;
    double        default_value;
    uint32_t      flags;        // clap_param_info_flags
};

inline constexpr std::array<ParamMeta, kParamCount> kParamMeta = {{
    { "Effect Type",       "MambaFX/Processing", 0.0,   3.0,   0.0,   CLAP_PARAM_IS_ENUM        },
    { "Wet Mix",           "MambaFX/Processing", 0.0,   1.0,   0.5,   CLAP_PARAM_IS_AUTOMATABLE },
    { "Input Gain",        "MambaFX/Levels",    -24.0,  24.0,  0.0,   CLAP_PARAM_IS_AUTOMATABLE },
    { "Output Gain",       "MambaFX/Levels",    -24.0,  24.0,  0.0,   CLAP_PARAM_IS_AUTOMATABLE },
    { "Context Length (s)","MambaFX/Model",      1.0,   120.0, 60.0,  CLAP_PARAM_IS_AUTOMATABLE },
    { "Model Variant",     "MambaFX/Model",      0.0,   2.0,   1.0,   CLAP_PARAM_IS_ENUM        },
    { "Inference Device",  "MambaFX/Model",      0.0,   1.0,   0.0,   CLAP_PARAM_IS_ENUM        },
    { "Bypass",            "MambaFX/Utility",    0.0,   1.0,   0.0,   CLAP_PARAM_IS_STEPPED
                                                                     | CLAP_PARAM_IS_AUTOMATABLE },
}};

// ─── ParameterBank ────────────────────────────────────────────────────────────
class ParameterBank {
public:
    ParameterBank() {
        for (uint32_t i = 0; i < kParamCount; ++i)
            values_[i].store(static_cast<float>(kParamMeta[i].default_value),
                             std::memory_order_relaxed);
    }

    // Real-time safe — called from any thread.
    [[nodiscard]] float get(ParamId id) const noexcept {
        return values_[idx(id)].load(std::memory_order_relaxed);
    }

    // Real-time safe — called from audio thread or host automation callbacks.
    void set(ParamId id, float value) noexcept {
        values_[idx(id)].store(clamp(id, value), std::memory_order_relaxed);
    }

    // Non-real-time helpers for CLAP extension queries.
    static bool fill_info(uint32_t param_index, clap_param_info_t* out) noexcept;
    static bool value_to_text(uint32_t param_id, double value,
                              char* out_buf, uint32_t out_capacity) noexcept;
    static bool text_to_value(uint32_t param_id, const char* text,
                              double* out_value) noexcept;

    // Snapshot all values (non-real-time; for state serialisation).
    void snapshot(std::span<float, kParamCount> out) const noexcept {
        for (uint32_t i = 0; i < kParamCount; ++i)
            out[i] = values_[i].load(std::memory_order_relaxed);
    }

    void restore(std::span<const float, kParamCount> in) noexcept {
        for (uint32_t i = 0; i < kParamCount; ++i)
            values_[i].store(clamp(static_cast<ParamId>(i), in[i]),
                             std::memory_order_relaxed);
    }

private:
    alignas(64) std::array<std::atomic<float>, kParamCount> values_{};

    static constexpr uint32_t idx(ParamId id) noexcept {
        return static_cast<uint32_t>(id);
    }
    static float clamp(ParamId id, float v) noexcept {
        const auto& m = kParamMeta[idx(id)];
        if (v < static_cast<float>(m.min_value)) return static_cast<float>(m.min_value);
        if (v > static_cast<float>(m.max_value)) return static_cast<float>(m.max_value);
        return v;
    }
};

} // namespace mamba::core
