#pragma once
// AudioRingBuffer — multi-channel, lock-free, real-time-safe ring buffer.
//
// Writer: audio thread (push per channel per process block).
// Reader: inference worker thread (pop into mel-spectrogram pipeline).
//
// Implementation uses two per-channel SPSC atomic pointers (read/write heads).
// The buffer is pre-allocated in activate(); no alloc occurs at runtime.
//
// Capacity must cover the worst-case latency between inference and audio:
//   ≥ max_block_size × 8  (enough for 8 blocks of inference lag).

#include <atomic>
#include <vector>
#include <span>
#include <cstring>
#include <algorithm>

namespace mamba::dsp {

class AudioRingBuffer {
public:
    AudioRingBuffer(uint32_t num_channels, uint32_t capacity_frames);

    // Audio thread: push |frames| samples for channel |ch|.
    // Returns false and does nothing if buffer is full.
    bool push(uint32_t ch, const float* src, uint32_t frames) noexcept;

    // Inference thread: read up to |max_frames| samples per channel.
    // Returns actual frames read (may be less if fewer are available).
    uint32_t pop(uint32_t ch, float* dst, uint32_t max_frames) noexcept;

    // Inference thread: how many frames are available (channel 0 as reference).
    uint32_t available() const noexcept;

    void clear() noexcept;

    uint32_t num_channels()    const noexcept { return num_channels_; }
    uint32_t capacity_frames() const noexcept { return capacity_; }

private:
    uint32_t num_channels_;
    uint32_t capacity_;  // power-of-two

    // Per-channel storage and head/tail pointers.
    // We keep them in a flat vector; stride = capacity_.
    std::vector<float> storage_;
    std::vector<std::atomic<uint32_t>> write_head_;  // per channel
    std::vector<std::atomic<uint32_t>> read_head_;   // shared across channels (ch 0)
};

} // namespace mamba::dsp
