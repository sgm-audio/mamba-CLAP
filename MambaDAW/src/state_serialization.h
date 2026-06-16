#ifndef STATE_SERIALIZATION_H
#define STATE_SERIALIZATION_H

#include "mamba_model.h"
#include <cstdint>
#include <string>
#include <vector>

namespace StateSerialization {
// Thread-domain: ALL functions in this namespace are [Main] only.
// They allocate (std::vector, std::string) and must never be called from the audio thread.

static constexpr uint32_t kMagic = 0x4D425354; // "MSTB" LE
static constexpr uint32_t kSchemaVersion = 5;
static constexpr uint32_t kMaxStateSize = 16 * 1024 * 1024;
static constexpr uint32_t kMaxBase64HStateSize = 32 * 1024 * 1024;

struct PluginState {
 double params[33];
 std::string weight_path;
 MambaModel::ModelState model_state;
 uint32_t schema_version = kSchemaVersion;
 uint32_t model_config_checksum = 0;
 uint8_t h_state_encoding = 1; // 0=binary (v3 legacy), 1=base64 (v4+)
};

uint32_t computeModelConfigChecksum(int64_t d_model, int64_t n_layers,
                                    int64_t d_state, int64_t d_conv, int64_t expand);

std::vector<uint8_t> encode(const PluginState& state);

bool decode(const uint8_t* data, uint32_t size, PluginState& out);

std::string base64Encode(const float* data, size_t count);

bool base64Decode(const std::string& encoded, std::vector<float>& out, size_t expected_count);

std::string canonicalizeWeightPath(const std::string& path);

} // namespace StateSerialization

#endif // STATE_SERIALIZATION_H
