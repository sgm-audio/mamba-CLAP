#ifndef MAMBA_WEIGHT_LOADER_H
#define MAMBA_WEIGHT_LOADER_H

#include "mamba_model.h"
#include "dsp/effect_params.h"
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Custom binary weight loader for Mamba models.
 *
 * Thread-domain per ADR-002: ALL methods are [Main] only.
 * Uses mmap, file I/O, and heap allocation. Never call from audio thread.
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

    // -----------------------------------------------------------------------
    // Optional PROJ section (appended after layer weights by export_weights.py
    // when exporting a full EffectControllerMamba checkpoint).
    //
    // Layout (little-endian):
    //   uint32_t proj_magic  = PROJ_MAGIC
    //   uint32_t enc_in      (== N_MEL for d_model=1 encoder)
    //   uint32_t head_out    (== N_MAMBA_OUT)
    //   float    enc_w[enc_in]
    //   float    enc_b
    //   float    head_w[head_out]
    //   float    head_b[head_out]
    // -----------------------------------------------------------------------
    static constexpr uint32_t PROJ_MAGIC    = 0x4A4F5250; // V1: enc+scalar-head ("PROJ" LE)
    static constexpr uint32_t PROJ_MAGIC_V2 = 0x324F5250; // V2: matrix head, no enc ("PRO2" LE)

    static constexpr uint32_t PROJ_MAGIC_V3 = 0x334F5250; // V3: head + ctx_proj + feat_proj ("PRO3" LE)

    // Weights from the optional PROJ section appended after layer data.
    // V2 format: head_w is [head_out × d_model_in] row-major matrix.
    struct ProjectionWeights {
        bool valid{false};
        int d_model_in{0}; // N_MEL
        int head_out{0};   // N_MAMBA_OUT
        std::vector<float> head_w; // [head_out * d_model_in] row-major
        std::vector<float> head_b; // [head_out]

        // V3 additions: context projection (N_MEL → head_out)
        bool has_ctx_proj{false};
        std::vector<float> ctx_proj_w; // [head_out * d_model_in]
        std::vector<float> ctx_proj_b; // [head_out]

        // V3 additions: feature projection (N_AUDIO_FEATURES → head_out)
        bool has_feat_proj{false};
        int n_audio_features{0};
        std::vector<float> feat_proj_w; // [head_out * n_audio_features]
        std::vector<float> feat_proj_b; // [head_out]
    };

    // Reads the PROJ section (V1, V2, or V3) from a .mambaweights file.
    // Returns valid=false if the section is absent or malformed.
    static ProjectionWeights loadProjections(const std::string& path);

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
