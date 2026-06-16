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

    // MambaModel exposes dState(), dConv(), and expand() accessors.
    // For this stub, we write what we can and leave the rest as 0.

    Header h;
    h.magic    = ToLE32(MAGIC);
    h.version  = ToLE32(VERSION);
    h.n_layers = ToLE32(static_cast<uint32_t>(model.numLayers()));
    h.d_model  = ToLE32(static_cast<uint32_t>(model.dModel()));
    h.d_state = ToLE32(static_cast<uint32_t>(model.dState()));
    h.d_conv = ToLE32(static_cast<uint32_t>(model.dConv()));
    h.expand = ToLE32(static_cast<uint32_t>(model.expand()));
    h.flags    = ToLE32(flags);

    file.write(reinterpret_cast<const char*>(&h), sizeof(h));
    if (!file) return false;

    // Serialize actual flat weight arrays for each layer.
    // This requires access to the model's weights, which we'll get by temporarily
    // loading them into buffers (since direct weight access isn't exposed in MambaModel).
    // In a full implementation, MambaModel would provide direct weight access.

    // For now, we'll note that weight serialization requires model weight access
    // which isn't currently exposed. The loader can still read weights if they exist
    // in the file, but saving weights would require modifying MambaModel to expose
    // weight data or provide a proper serialization interface.

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
        size_t out_proj_n = static_cast<size_t>(h.d_model) * static_cast<size_t>(d_inner);
        size_t conv1d_n   = static_cast<size_t>(d_inner * d_conv);
        size_t ssm_n      = static_cast<size_t>(d_inner * d_state);
 size_t D_n = static_cast<size_t>(d_inner);
 size_t total_sz = (in_proj_n + out_proj_n + conv1d_n + ssm_n * 3 + D_n) * sizeof(float);

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
 const float* D = consume(D_n);

 if (!model.loadLayerWeights(layer_idx,
 in_proj, in_proj_n,
 out_proj, out_proj_n,
 conv1d, conv1d_n,
 dA, ssm_n,
 dB, ssm_n,
 C, ssm_n,
 D, D_n)) {
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
        size_t out_proj_n = static_cast<size_t>(h.d_model) * static_cast<size_t>(d_inner);
        size_t conv1d_n   = static_cast<size_t>(d_inner * d_conv);
        size_t ssm_n      = static_cast<size_t>(d_inner * d_state);
        size_t D_n        = static_cast<size_t>(d_inner);
        size_t total      = in_proj_n + out_proj_n + conv1d_n + ssm_n * 3 + D_n;

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

// ============================================================================
// Projection section loader
// ============================================================================
MambaWeightLoader::ProjectionWeights
MambaWeightLoader::loadProjections(const std::string& path) {
    ProjectionWeights proj{};

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return proj;

    f.seekg(0, std::ios::end);
    const std::size_t file_size = static_cast<std::size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(file_size);
    if (!f.read(reinterpret_cast<char*>(buf.data()),
                static_cast<std::streamsize>(file_size))) return proj;

    if (file_size < sizeof(Header)) return proj;
    Header h{};
    if (!readHeader(buf.data(), file_size, h) || !validateHeader(h)) return proj;

    const uint32_t d_inner = h.expand * h.d_model;
    const std::size_t layer_floats =
        static_cast<std::size_t>(h.d_model) * 2 * d_inner          // in_proj
        + static_cast<std::size_t>(h.d_model) * d_inner             // out_proj [d_model, d_inner]
        + static_cast<std::size_t>(d_inner) * h.d_conv              // conv1d
        + static_cast<std::size_t>(d_inner) * h.d_state * 3         // dA, dB, C
        + d_inner;                                                   // D
    const std::size_t proj_offset =
        sizeof(Header) + h.n_layers * layer_floats * sizeof(float);

    if (proj_offset + sizeof(uint32_t) < file_size) {
        const uint8_t* p = buf.data() + proj_offset;

        uint32_t magic = 0;
        std::memcpy(&magic, p, 4);

        if (magic == PROJ_MAGIC_V2) {
            // V2: (PRO2_MAGIC, d_model_in, head_out, head_w[head_out*d_model_in], head_b[head_out])
            p += 4;
            if (static_cast<std::size_t>(p - buf.data()) + 8 > file_size) return proj;
            uint32_t d_model_in = 0, head_out = 0;
            std::memcpy(&d_model_in, p, 4); p += 4;
            std::memcpy(&head_out, p, 4); p += 4;
            const std::size_t needed =
                static_cast<std::size_t>(head_out) * d_model_in * sizeof(float) // head_w matrix
                + static_cast<std::size_t>(head_out) * sizeof(float); // head_b
            if (static_cast<std::size_t>(p - buf.data()) + needed > file_size) return proj;
            proj.d_model_in = static_cast<int>(d_model_in);
            proj.head_out = static_cast<int>(head_out);
            proj.head_w.resize(static_cast<std::size_t>(head_out) * d_model_in);
            std::memcpy(proj.head_w.data(), p, proj.head_w.size() * sizeof(float));
            p += proj.head_w.size() * sizeof(float);
            proj.head_b.resize(head_out);
            std::memcpy(proj.head_b.data(), p, head_out * sizeof(float));
            proj.valid = true;
        } else if (magic == PROJ_MAGIC_V3) {
            // V3: (PRO3_MAGIC, d_model_in, head_out, n_audio_features,
            //       head_w[head_out*d_model_in], head_b[head_out],
            //       ctx_proj_w[head_out*d_model_in], ctx_proj_b[head_out],
            //       feat_proj_w[head_out*n_audio_features], feat_proj_b[head_out])
            p += 4;
            if (static_cast<std::size_t>(p - buf.data()) + 12 > file_size) return proj;
            uint32_t d_model_in = 0, head_out = 0, n_feat = 0;
            std::memcpy(&d_model_in, p, 4); p += 4;
            std::memcpy(&head_out, p, 4); p += 4;
            std::memcpy(&n_feat, p, 4); p += 4;
            proj.d_model_in = static_cast<int>(d_model_in);
            proj.head_out = static_cast<int>(head_out);
            proj.n_audio_features = static_cast<int>(n_feat);

            const std::size_t head_w_sz = static_cast<std::size_t>(head_out) * d_model_in;
            const std::size_t head_b_sz = head_out;
            const std::size_t ctx_w_sz = head_w_sz;
            const std::size_t ctx_b_sz = head_out;
            const std::size_t feat_w_sz = static_cast<std::size_t>(head_out) * n_feat;
            const std::size_t feat_b_sz = head_out;
            const std::size_t total_needed = (head_w_sz + head_b_sz + ctx_w_sz + ctx_b_sz
                + feat_w_sz + feat_b_sz) * sizeof(float);
            if (static_cast<std::size_t>(p - buf.data()) + total_needed > file_size) return proj;

            proj.head_w.resize(head_w_sz);
            std::memcpy(proj.head_w.data(), p, head_w_sz * sizeof(float)); p += head_w_sz * sizeof(float);
            proj.head_b.resize(head_b_sz);
            std::memcpy(proj.head_b.data(), p, head_b_sz * sizeof(float)); p += head_b_sz * sizeof(float);

            proj.ctx_proj_w.resize(ctx_w_sz);
            std::memcpy(proj.ctx_proj_w.data(), p, ctx_w_sz * sizeof(float)); p += ctx_w_sz * sizeof(float);
            proj.ctx_proj_b.resize(ctx_b_sz);
            std::memcpy(proj.ctx_proj_b.data(), p, ctx_b_sz * sizeof(float)); p += ctx_b_sz * sizeof(float);
            proj.has_ctx_proj = true;

            if (n_feat > 0) {
                proj.feat_proj_w.resize(feat_w_sz);
                std::memcpy(proj.feat_proj_w.data(), p, feat_w_sz * sizeof(float)); p += feat_w_sz * sizeof(float);
                proj.feat_proj_b.resize(feat_b_sz);
                std::memcpy(proj.feat_proj_b.data(), p, feat_b_sz * sizeof(float)); p += feat_b_sz * sizeof(float);
                proj.has_feat_proj = true;
            }

            proj.valid = true;
        }
        // (V1 PROJ_MAGIC format with encoder is no longer supported — retrain with current code)
    }
    return proj;
}
