#include "mamba_weight_loader.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

// ============================================================================
// Endianness helpers
// ============================================================================
static uint32_t Swap32(uint32_t x) {
    return ((x & 0x000000FF) << 24) |
           ((x & 0x0000FF00) <<  8) |
           ((x & 0x00FF0000) >>  8) |
           ((x & 0xFF000000) >> 24);
}

static uint32_t ToLE32(uint32_t x) {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return Swap32(x);
#else
    return x; // Already little-endian on x86_64/ARM
#endif
}

static uint32_t FromLE32(uint32_t x) {
    return ToLE32(x);
}

// ============================================================================
// Header helpers
// ============================================================================
bool MambaWeightLoader::readHeader(const uint8_t* data, size_t size, Header& out) {
    if (size < sizeof(Header)) return false;
    std::memcpy(&out, data, sizeof(Header));

    out.magic     = FromLE32(out.magic);
    out.version   = FromLE32(out.version);
    out.n_layers  = FromLE32(out.n_layers);
    out.d_model   = FromLE32(out.d_model);
    out.d_state   = FromLE32(out.d_state);
    out.d_conv    = FromLE32(out.d_conv);
    out.expand    = FromLE32(out.expand);
    out.flags     = FromLE32(out.flags);

    return true;
}

bool MambaWeightLoader::peekHeader(const uint8_t* data) {
    Header h;
    if (!readHeader(data, sizeof(Header), h)) return false;
    return h.magic == MAGIC && h.version == VERSION;
}

bool MambaWeightLoader::validateHeader(const Header& h) {
    return h.magic == MAGIC && h.version == VERSION &&
           h.n_layers > 0 && h.n_layers < 1000 &&  // reasonable upper bound
           h.d_model > 0 && h.d_model < 10000 &&
           h.d_state > 0 && h.d_state < 10000 &&
           h.d_conv > 0 && h.d_conv < 1000 &&
           h.expand > 0 && h.expand < 10000;
}

// ============================================================================
// Serialization
// ============================================================================
bool MambaWeightLoader::save(const std::string& path, const MambaModel& model, uint32_t flags) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return false;
    }

    // We need to inspect the model's architecture, but MambaModel doesn't expose
    // d_state, d_conv, expand directly. For now, we'll serialize architecture
    // that the loader can use to validate. In a full implementation, we'd add
    // accessors to MambaModel for these parameters.
    //
    // TODO: Add MambaModel::dState(), dConv(), expand() accessors.
    // For this stub, we write what we can and leave the rest as 0.
    
    Header h;
    h.magic    = ToLE32(MAGIC);
    h.version  = ToLE32(VERSION);
    h.n_layers = ToLE32(static_cast<uint32_t>(model.numLayers()));
    h.d_model  = ToLE32(static_cast<uint32_t>(model.dModel()));
    h.d_state  = ToLE32(16);  // placeholder
    h.d_conv   = ToLE32(4);   // placeholder
    h.expand   = ToLE32(2);   // placeholder
    h.flags    = ToLE32(flags);

    file.write(reinterpret_cast<const char*>(&h), sizeof(h));
    if (!file) return false;

    // TODO: serialize actual flat weight arrays for each layer.
    // This requires MambaModel to expose weight pointers or a serialization API.
    // For now, the file only contains the header.
    
    return file.good();
}

// ============================================================================
// Deserialization
// ============================================================================
bool MambaWeightLoader::load(const std::string& path, MambaModel& model) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open file for reading: " << path << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "Failed to read file: " << path << std::endl;
        return false;
    }

    return loadFromMemory(buffer.data(), buffer.size(), model);
}

bool MambaWeightLoader::loadFromMemory(const uint8_t* data, size_t size, MambaModel& model) {
    Header h;
    if (!readHeader(data, size, h)) {
        std::cerr << "Weight file too small" << std::endl;
        return false;
    }

    if (!validateHeader(h)) {
        std::cerr << "Invalid weight file header" << std::endl;
        return false;
    }

    const uint8_t* ptr = data + sizeof(Header);
    size_t remaining = size - sizeof(Header);

    for (uint32_t layer_idx = 0; layer_idx < h.n_layers; ++layer_idx) {
        int64_t d_inner = static_cast<int64_t>(h.d_model) * static_cast<int64_t>(h.expand);
        int64_t d_state = static_cast<int64_t>(h.d_state);
        int64_t d_conv  = static_cast<int64_t>(h.d_conv);

        size_t in_proj_n  = static_cast<size_t>(h.d_model) * 2 * static_cast<size_t>(d_inner);
        size_t out_proj_n = static_cast<size_t>(d_inner);
        size_t conv1d_n   = static_cast<size_t>(d_inner * d_conv);
        size_t ssm_n      = static_cast<size_t>(d_inner * d_state);
        size_t total_sz   = (in_proj_n + out_proj_n + conv1d_n + ssm_n * 3 + out_proj_n) * sizeof(float);

        if (remaining < total_sz) {
            std::cerr << "Weight file truncated at layer " << layer_idx << std::endl;
            return false;
        }

        auto consume = [&](size_t count) -> const float* {
            const float* p = reinterpret_cast<const float*>(ptr);
            ptr      += count * sizeof(float);
            remaining -= count * sizeof(float);
            return p;
        };

        const float* in_proj  = consume(in_proj_n);
        const float* out_proj = consume(out_proj_n);
        const float* conv1d   = consume(conv1d_n);
        const float* dA       = consume(ssm_n);
        const float* dB       = consume(ssm_n);
        const float* C        = consume(ssm_n);
        const float* D        = consume(out_proj_n);

        if (!model.loadLayerWeights(layer_idx,
                                    in_proj,  in_proj_n,
                                    out_proj, out_proj_n,
                                    conv1d,   conv1d_n,
                                    dA,       ssm_n,
                                    dB,       ssm_n,
                                    C,        ssm_n,
                                    D,        out_proj_n)) {
            std::cerr << "Failed to load weights for layer " << layer_idx << std::endl;
            return false;
        }
    }

    return true;
}

// ============================================================================
// Extraction (inspection / validation)
// ============================================================================
bool MambaWeightLoader::extractRawWeights(const std::string& path,
                                          std::vector<std::vector<float>>& outLayers) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return false;

    Header h;
    if (!readHeader(buffer.data(), buffer.size(), h) || !validateHeader(h)) {
        return false;
    }

    const uint8_t* ptr = buffer.data() + sizeof(Header);
    size_t remaining = buffer.size() - sizeof(Header);
    outLayers.clear();

    for (uint32_t layer_idx = 0; layer_idx < h.n_layers; ++layer_idx) {
        int64_t d_inner = static_cast<int64_t>(h.d_model) * static_cast<int64_t>(h.expand);
        int64_t d_state = static_cast<int64_t>(h.d_state);
        int64_t d_conv  = static_cast<int64_t>(h.d_conv);

        size_t in_proj_n  = static_cast<size_t>(h.d_model) * 2 * static_cast<size_t>(d_inner);
        size_t out_proj_n = static_cast<size_t>(d_inner);
        size_t conv1d_n   = static_cast<size_t>(d_inner * d_conv);
        size_t ssm_n      = static_cast<size_t>(d_inner * d_state);
        size_t total      = in_proj_n + out_proj_n + conv1d_n + ssm_n * 3 + out_proj_n;

        if (remaining < total * sizeof(float)) return false;

        outLayers.emplace_back();
        auto& layer = outLayers.back();
        layer.resize(total);

        std::memcpy(layer.data(), ptr, total * sizeof(float));
        ptr      += total * sizeof(float);
        remaining -= total * sizeof(float);
    }

    return true;
}
