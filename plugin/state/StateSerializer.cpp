#include "state/StateSerializer.hpp"
#include "core/ParameterBank.hpp"

#include <cstring>
#include <cassert>
#include <array>

namespace mamba::state {

// ─── Migration table ──────────────────────────────────────────────────────────
// Version 0 → 1: added ModelVariant and InferenceDevice params.
// For existing blobs that have fewer params, fill defaults.
const StateSerializer::MigrationFn StateSerializer::kMigrations[] = {
    /* v0→v1 */ [](StateBlob& b) {
        using P = core::ParamMeta;
        while (b.param_values.size() < core::kParamCount)
            b.param_values.push_back(static_cast<float>(
                core::kParamMeta[b.param_values.size()].default_value));
        b.version = 1;
    },
};

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace {

bool write_u32(const clap_ostream_t* s, uint32_t v) noexcept {
    return s->write(s, &v, 4) == 4;
}
bool read_u32(const clap_istream_t* s, uint32_t& v) noexcept {
    return s->read(s, &v, 4) == 4;
}
bool write_f32(const clap_ostream_t* s, float v) noexcept {
    return s->write(s, &v, 4) == 4;
}
bool read_f32(const clap_istream_t* s, float& v) noexcept {
    return s->read(s, &v, 4) == 4;
}
bool write_string(const clap_ostream_t* s, const std::string& str) noexcept {
    const uint32_t len = static_cast<uint32_t>(str.size());
    if (!write_u32(s, len)) return false;
    if (len > 0 && s->write(s, str.data(), len) != static_cast<int64_t>(len))
        return false;
    return true;
}
bool read_string(const clap_istream_t* s, std::string& str) noexcept {
    uint32_t len = 0;
    if (!read_u32(s, len)) return false;
    if (len > 4096) return false;  // sanity cap
    str.resize(len);
    if (len > 0 && s->read(s, str.data(), len) != static_cast<int64_t>(len))
        return false;
    return true;
}

} // anon

// ─── StateSerializer ─────────────────────────────────────────────────────────

StateSerializer::StateSerializer(core::ParameterBank& params)
    : params_(params)
{}

bool StateSerializer::save(const clap_ostream_t* stream) const {
    StateBlob blob;
    blob.version = kStateVersion;
    blob.param_values.resize(core::kParamCount);
    params_.snapshot(std::span<float, core::kParamCount>(blob.param_values.data(),
                                                          core::kParamCount));
    blob.model_root = model_root_;
    return write_blob(stream, blob);
}

bool StateSerializer::load(const clap_istream_t* stream) {
    StateBlob blob;
    if (!read_blob(stream, blob)) return false;
    migrate(blob);

    // Validate and restore.
    if (blob.param_values.size() != core::kParamCount) return false;
    params_.restore(std::span<const float, core::kParamCount>(
        blob.param_values.data(), core::kParamCount));

    model_root_ = std::move(blob.model_root);
    return true;
}

bool StateSerializer::write_blob(const clap_ostream_t* s, const StateBlob& b) const {
    if (!write_u32(s, kStateMagic))   return false;
    if (!write_u32(s, b.version))     return false;
    if (!write_u32(s, static_cast<uint32_t>(b.param_values.size()))) return false;
    for (float v : b.param_values)
        if (!write_f32(s, v)) return false;
    if (!write_string(s, b.model_root)) return false;
    return true;
}

bool StateSerializer::read_blob(const clap_istream_t* s, StateBlob& b) const {
    uint32_t magic = 0;
    if (!read_u32(s, magic) || magic != kStateMagic) return false;
    if (!read_u32(s, b.version)) return false;

    uint32_t n_params = 0;
    if (!read_u32(s, n_params)) return false;
    if (n_params > 256) return false;  // sanity cap

    b.param_values.resize(n_params);
    for (auto& v : b.param_values)
        if (!read_f32(s, v)) return false;

    if (!read_string(s, b.model_root)) return false;
    return true;
}

void StateSerializer::migrate(StateBlob& blob) {
    constexpr auto n = sizeof(kMigrations) / sizeof(kMigrations[0]);
    while (blob.version < kStateVersion && blob.version < n)
        kMigrations[blob.version](blob);
}

} // namespace mamba::state
