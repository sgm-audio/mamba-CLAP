#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mamba_audio::architecture {

struct SerializedPluginState {
    std::uint32_t schema_version = 1;
    std::uint64_t config_hash = 0;
    std::string weight_file_path;
    std::vector<float> params;
    std::string h_state_base64;
};

class IStateSerializer {
public:
    virtual ~IStateSerializer() = default;

    // Main thread only.
    virtual SerializedPluginState capture() const = 0;
    virtual bool restore(const SerializedPluginState& state) = 0;
};

class IBase64Codec {
public:
    virtual ~IBase64Codec() = default;

    virtual std::string encode(std::span<const float> bytes_as_float) const = 0;
    virtual bool decode(std::string_view b64, std::span<float> out) const = 0;
};

class IWeightPathPolicy {
public:
    virtual ~IWeightPathPolicy() = default;

    virtual bool canonicalize(std::string_view input_path, std::string& out_canonical) const = 0;
    virtual bool isAllowed(std::string_view canonical_path) const = 0;
};

} // namespace mamba_audio::architecture
