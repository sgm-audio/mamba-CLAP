#ifndef MAMBA_WEIGHT_LOADER_H
#define MAMBA_WEIGHT_LOADER_H

#include "mamba_model.h"
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Custom binary weight loader for Mamba models.
 *
 * Replaces torch::jit::load() with a flat binary format that has no
dependencies outside the C++ standard library.
 *
 * Format (little-endian, version 1):
 *   uint32_t magic   = 0x574D424D  ("MMBW" reversed for LE)
 *   uint32_t version = 1
 *   uint32_t n_layers
 *   uint32_t d_model
 *   uint32_t d_state
 *   uint32_t d_conv
 *   uint32_t expand
 *   uint32_t flags   (reserved: quantization mode, etc.)
 *
 * Followed by per-layer flat float arrays (all little-endian):
 *   in_proj  [d_model * 2 * d_inner]
 *   out_proj [d_inner]
 *   conv1d   [d_inner * d_conv]
 *   dA       [d_inner * d_state]
 *   dB       [d_inner * d_state]
 *   C        [d_inner * d_state]
 *   D        [d_inner]
 */
class MambaWeightLoader {
public:
    static constexpr uint32_t MAGIC   = 0x574D424D; // "MMBW"
    static constexpr uint32_t VERSION = 1;

    /**
     * @brief Serialize a MambaModel to a custom binary file.
     *
     * Must be called on a non-RT thread.
     *
     * @param path   Output file path.
     * @param model  Model to serialize.
     * @param flags  Optional flags (e.g., quantization mode).
     * @return true on success.
     */
    static bool save(const std::string& path, const MambaModel& model, uint32_t flags = 0);

    /**
     * @brief Deserialize weights from a custom binary file into an existing model.
     *
     * Must be called on a non-RT thread.
     *
     * @param path   Input file path.
     * @param model  Model to populate.
     * @return true on success.
     */
    static bool load(const std::string& path, MambaModel& model);

    /**
     * @brief Load weights from an in-memory buffer.
     *
     * Useful for embedded resources or IPC.
     *
     * @param data   Pointer to the binary data.
     * @param size   Size of the buffer in bytes.
     * @param model  Model to populate.
     * @return true on success.
     */
    static bool loadFromMemory(const uint8_t* data, size_t size, MambaModel& model);

    /**
     * @brief Peek at the header to verify format before full load.
     *
     * @param data Pointer to at least 28 bytes of data.
     * @return true if magic and version match.
     */
    static bool peekHeader(const uint8_t* data);

    /**
     * @brief Extract raw per-layer weights for inspection or validation.
     *
     * @param path       Binary file path.
     * @param outLayers  Output: each outer vector is a layer, each inner vector is a flat float array.
     * @return true on success.
     */
    static bool extractRawWeights(const std::string& path,
                                  std::vector<std::vector<float>>& outLayers);

private:
    struct Header {
        uint32_t magic;
        uint32_t version;
        uint32_t n_layers;
        uint32_t d_model;
        uint32_t d_state;
        uint32_t d_conv;
        uint32_t expand;
        uint32_t flags;
    };

    static_assert(sizeof(Header) == 32, "Header must be exactly 32 bytes");

    static bool readHeader(const uint8_t* data, size_t size, Header& out);
    static bool validateHeader(const Header& h);

    // Prevent instantiation
    MambaWeightLoader() = delete;
};

#endif // MAMBA_WEIGHT_LOADER_H
