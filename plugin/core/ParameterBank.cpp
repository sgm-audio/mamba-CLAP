#include "core/ParameterBank.hpp"
#include <cstring>
#include <cstdio>
#include <charconv>

namespace mamba::core {

bool ParameterBank::fill_info(uint32_t param_index, clap_param_info_t* out) noexcept {
    if (param_index >= kParamCount) return false;
    const auto& m = kParamMeta[param_index];
    std::memset(out, 0, sizeof(*out));
    out->id          = param_index;
    out->flags       = m.flags;
    out->min_value   = m.min_value;
    out->max_value   = m.max_value;
    out->default_value = m.default_value;
    std::strncpy(out->name,   m.name,   sizeof(out->name)   - 1);
    std::strncpy(out->module, m.module, sizeof(out->module) - 1);
    return true;
}

bool ParameterBank::value_to_text(uint32_t param_id, double value,
                                   char* buf, uint32_t cap) noexcept
{
    if (param_id >= kParamCount) return false;
    const auto id = static_cast<ParamId>(param_id);
    int written = 0;

    switch (id) {
    case ParamId::EffectType: {
        const char* names[] = {"Reverb","Compress","Saturate","EQ"};
        const int idx = std::clamp(static_cast<int>(value), 0, 3);
        written = std::snprintf(buf, cap, "%s", names[idx]);
        break;
    }
    case ParamId::WetMix:
        written = std::snprintf(buf, cap, "%.0f%%", value * 100.0);
        break;
    case ParamId::InputGain:
    case ParamId::OutputGain:
        written = std::snprintf(buf, cap, "%.1f dB", value);
        break;
    case ParamId::ContextLength:
        written = std::snprintf(buf, cap, "%.0f s", value);
        break;
    case ParamId::ModelVariant: {
        const char* variants[] = {"Tiny (20MB)", "Base (80MB)", "Large (150MB)"};
        const int idx = std::clamp(static_cast<int>(value), 0, 2);
        written = std::snprintf(buf, cap, "%s", variants[idx]);
        break;
    }
    case ParamId::InferenceDevice: {
        written = std::snprintf(buf, cap, "%s", value < 0.5 ? "CPU" : "CUDA");
        break;
    }
    case ParamId::Bypass:
        written = std::snprintf(buf, cap, "%s", value < 0.5 ? "Off" : "On");
        break;
    default:
        written = std::snprintf(buf, cap, "%.3f", value);
        break;
    }
    return written > 0 && static_cast<uint32_t>(written) < cap;
}

bool ParameterBank::text_to_value(uint32_t param_id, const char* text,
                                   double* out_value) noexcept
{
    if (!text || !out_value || param_id >= kParamCount) return false;
    // Simple numeric parse; enum params resolved from first character.
    double v = 0.0;
    auto [ptr, ec] = std::from_chars(text, text + std::strlen(text), v);
    if (ec == std::errc{}) { *out_value = v; return true; }
    // Fallback: return default.
    *out_value = kParamMeta[param_id].default_value;
    return false;
}

} // namespace mamba::core
