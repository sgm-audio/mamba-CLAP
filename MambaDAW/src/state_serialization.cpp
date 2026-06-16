#include "state_serialization.h"
#include <cstring>
#include <algorithm>
#include <numeric>

namespace StateSerialization {

static constexpr char kBase64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static uint8_t base64DecodeChar(char c) {
  if (c >= 'A' && c <= 'Z') return static_cast<uint8_t>(c - 'A');
  if (c >= 'a' && c <= 'z') return static_cast<uint8_t>(c - 'a' + 26);
  if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0' + 52);
  if (c == '+') return 62;
  if (c == '/') return 63;
  return 255;
}

uint32_t computeModelConfigChecksum(int64_t d_model, int64_t n_layers,
                                    int64_t d_state, int64_t d_conv, int64_t expand) {
  int64_t vals[] = {d_model, n_layers, d_state, d_conv, expand};
  uint32_t h = 0x811C9DC5u; // FNV-1a offset basis
  const auto* bytes = reinterpret_cast<const uint8_t*>(vals);
  for (size_t i = 0; i < sizeof(vals); ++i) {
    h ^= bytes[i];
    h *= 0x01000193u; // FNV-1a prime
  }
  return h;
}

std::string base64Encode(const float* data, size_t count) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(data);
  size_t byte_len = count * sizeof(float);
  std::string out;
  out.reserve((byte_len + 2) / 3 * 4);
  for (size_t i = 0; i < byte_len; i += 3) {
    uint32_t n = static_cast<uint32_t>(bytes[i]) << 16;
    if (i + 1 < byte_len) n |= static_cast<uint32_t>(bytes[i + 1]) << 8;
    if (i + 2 < byte_len) n |= static_cast<uint32_t>(bytes[i + 2]);
    out.push_back(kBase64Table[(n >> 18) & 0x3F]);
    out.push_back(kBase64Table[(n >> 12) & 0x3F]);
    out.push_back((i + 1 < byte_len) ? kBase64Table[(n >> 6) & 0x3F] : '=');
    out.push_back((i + 2 < byte_len) ? kBase64Table[n & 0x3F] : '=');
  }
  return out;
}

bool base64Decode(const std::string& encoded, std::vector<float>& out, size_t expected_count) {
  if (encoded.size() % 4 != 0) return false;
  size_t byte_len = (encoded.size() / 4) * 3;
  if (encoded.size() >= 1 && encoded[encoded.size() - 1] == '=') byte_len--;
  if (encoded.size() >= 2 && encoded[encoded.size() - 2] == '=') byte_len--;
  if (byte_len != expected_count * sizeof(float)) return false;
  out.resize(expected_count);
  auto* dst = reinterpret_cast<uint8_t*>(out.data());
  size_t di = 0;
  for (size_t i = 0; i < encoded.size(); i += 4) {
    uint8_t a = base64DecodeChar(encoded[i]);
    uint8_t b = base64DecodeChar(encoded[i + 1]);
    uint8_t c = (encoded[i + 2] == '=') ? 0 : base64DecodeChar(encoded[i + 2]);
    uint8_t d = (encoded[i + 3] == '=') ? 0 : base64DecodeChar(encoded[i + 3]);
    if (a == 255 || b == 255 || c == 255 || d == 255) return false;
    uint32_t n = (static_cast<uint32_t>(a) << 18) | (static_cast<uint32_t>(b) << 12) |
                 (static_cast<uint32_t>(c) << 6) | static_cast<uint32_t>(d);
    if (di < byte_len) dst[di++] = static_cast<uint8_t>((n >> 16) & 0xFF);
    if (di < byte_len) dst[di++] = static_cast<uint8_t>((n >> 8) & 0xFF);
    if (di < byte_len) dst[di++] = static_cast<uint8_t>(n & 0xFF);
  }
  return di == byte_len;
}

std::string canonicalizeWeightPath(const std::string& path) {
  std::string result = path;
  // Remove trailing slashes
  while (!result.empty() && result.back() == '/')
    result.pop_back();
  // Collapse duplicate slashes
  std::string cleaned;
  cleaned.reserve(result.size());
  bool prev_slash = false;
  for (char c : result) {
    if (c == '/') {
      if (!prev_slash) cleaned.push_back(c);
      prev_slash = true;
    } else {
      cleaned.push_back(c);
      prev_slash = false;
    }
  }
  return cleaned;
}

// Binary layout:
// v5: [0..3] magic, [4..7] schema=5, [8..271] double params[33] (264 bytes),
//     [272..275] checksum, [276..279] weight_path_len, [weight_path bytes],
//     h_state_encoding byte, n_layers, then per-layer: (sh_count, cb_count, conv_pos, base64_len, base64_bytes) or (sh_count, cb_count, conv_pos, raw floats)
// v4: same as v5 but params[14] (112 bytes); includes h_state_encoding byte
// v3: same as v4 but no h_state_encoding byte; h_state is raw binary
// v2: [0..3] magic, [4..7] schema=2, [8..111] double params[13] (104 bytes), then checksum+path+layers
// v1: [0..3] magic, [4..7] schema=1, [8..103] double params[12] (96 bytes), then checksum+path+layers

std::vector<uint8_t> encode(const PluginState& state) {
 std::vector<uint8_t> buf;
 buf.reserve(4096);
 auto write_u32 = [&](uint32_t v) {
  uint8_t b[4];
  std::memcpy(b, &v, 4);
  for (int i = 0; i < 4; ++i) buf.push_back(b[i]);
 };
 auto write_i64 = [&](int64_t v) {
  uint8_t b[8];
  std::memcpy(b, &v, 8);
  for (int i = 0; i < 8; ++i) buf.push_back(b[i]);
 };
 auto write_f32 = [&](float v) {
  uint8_t b[4];
  std::memcpy(b, &v, 4);
  for (int i = 0; i < 4; ++i) buf.push_back(b[i]);
 };
 auto write_u8 = [&](uint8_t v) {
  buf.push_back(v);
 };
 auto write_bytes = [&](const void* data, size_t n) {
  const auto* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < n; ++i) buf.push_back(p[i]);
 };

 write_u32(kMagic);
 const uint32_t schema = state.schema_version;
 write_u32(schema);
 const int num_params = (schema >= 5) ? 33 : (schema >= 3) ? 14 : (schema >= 2) ? 13 : 12;
 for (int i = 0; i < num_params; ++i) {
  uint8_t db[8];
  std::memcpy(db, &state.params[i], 8);
  for (int j = 0; j < 8; ++j) buf.push_back(db[j]);
 }
 write_u32(state.model_config_checksum);
 write_u32(static_cast<uint32_t>(state.weight_path.size()));
 write_bytes(state.weight_path.data(), state.weight_path.size());

 if (schema >= 4) {
  write_u8(state.h_state_encoding);
 }

 write_u32(static_cast<uint32_t>(state.model_state.layers.size()));
 for (const auto& layer : state.model_state.layers) {
  write_u32(static_cast<uint32_t>(layer.state_h.size()));
  write_u32(static_cast<uint32_t>(layer.conv_buffer.size()));
  write_i64(layer.conv_pos);
  if (schema >= 4 && state.h_state_encoding == 1) {
   std::string b64 = base64Encode(layer.state_h.data(), layer.state_h.size());
   write_u32(static_cast<uint32_t>(b64.size()));
   write_bytes(b64.data(), b64.size());
   std::string b64_cb = base64Encode(layer.conv_buffer.data(), layer.conv_buffer.size());
   write_u32(static_cast<uint32_t>(b64_cb.size()));
   write_bytes(b64_cb.data(), b64_cb.size());
  } else {
   for (float v : layer.state_h) write_f32(v);
   for (float v : layer.conv_buffer) write_f32(v);
  }
 }

 if (buf.size() > kMaxStateSize) buf.clear();
 return buf;
}

bool decode(const uint8_t* data, uint32_t size, PluginState& out) {
 if (!data || size < 8) return false;
 auto read_u32 = [&](size_t& off) -> uint32_t {
  uint32_t v;
  std::memcpy(&v, data + off, 4);
  off += 4;
  return v;
 };
 auto read_i64 = [&](size_t& off) -> int64_t {
  int64_t v;
  std::memcpy(&v, data + off, 8);
  off += 8;
  return v;
 };
 auto read_f32 = [&](size_t& off) -> float {
  float v;
  std::memcpy(&v, data + off, 4);
  off += 4;
  return v;
 };
 auto read_u8 = [&](size_t& off) -> uint8_t {
  uint8_t v = data[off];
  off += 1;
  return v;
 };

 size_t off = 0;
 uint32_t magic = read_u32(off);
 if (magic != kMagic) return false;
 out.schema_version = read_u32(off);
 if (out.schema_version > kSchemaVersion) return false;

 std::memset(out.params, 0, sizeof(out.params));
 const int num_params = (out.schema_version >= 5) ? 33 : (out.schema_version >= 3) ? 14 : (out.schema_version >= 2) ? 13 : 12;
 if (off + num_params * 8 > size) return false;
 for (int i = 0; i < num_params; ++i) {
  std::memcpy(&out.params[i], data + off, 8);
  off += 8;
 }
 out.model_config_checksum = read_u32(off);
 uint32_t wp_len = read_u32(off);
 if (off + wp_len > size) return false;
 out.weight_path.assign(reinterpret_cast<const char*>(data + off), wp_len);
 off += wp_len;

 uint8_t h_state_encoding = 0;
 if (out.schema_version >= 4) {
  if (off + 1 > size) return false;
  h_state_encoding = read_u8(off);
 }
 out.h_state_encoding = h_state_encoding;

 uint32_t n_layers = read_u32(off);
 out.model_state.layers.resize(n_layers);
 for (uint32_t li = 0; li < n_layers; ++li) {
  uint32_t sh_count = read_u32(off);
  uint32_t cb_count = read_u32(off);
  int64_t conv_pos = read_i64(off);
  out.model_state.layers[li].conv_pos = conv_pos;

  if (h_state_encoding == 1) {
   uint32_t b64_len = read_u32(off);
   if (off + b64_len > size) return false;
   std::string b64_str(reinterpret_cast<const char*>(data + off), b64_len);
   off += b64_len;
   if (!base64Decode(b64_str, out.model_state.layers[li].state_h, sh_count))
    return false;

   uint32_t b64_cb_len = read_u32(off);
   if (off + b64_cb_len > size) return false;
   std::string b64_cb_str(reinterpret_cast<const char*>(data + off), b64_cb_len);
   off += b64_cb_len;
   if (!base64Decode(b64_cb_str, out.model_state.layers[li].conv_buffer, cb_count))
    return false;
  } else {
   if (off + sh_count * sizeof(float) + cb_count * sizeof(float) > size) return false;
   out.model_state.layers[li].state_h.resize(sh_count);
   for (uint32_t i = 0; i < sh_count; ++i)
    out.model_state.layers[li].state_h[i] = read_f32(off);
   out.model_state.layers[li].conv_buffer.resize(cb_count);
   for (uint32_t i = 0; i < cb_count; ++i)
    out.model_state.layers[li].conv_buffer[i] = read_f32(off);
  }
 }
 return true;
}

} // namespace StateSerialization
