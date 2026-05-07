#include "dsp/AudioRingBuffer.hpp"
#include <cassert>
#include <bit>

namespace mamba::dsp {

namespace {
// Round up to next power of two.
uint32_t next_pow2(uint32_t v) {
    if (v == 0) return 1;
    return static_cast<uint32_t>(std::bit_ceil(v));
}
} // anon

AudioRingBuffer::AudioRingBuffer(uint32_t num_channels, uint32_t capacity_frames)
    : num_channels_(num_channels)
    , capacity_(next_pow2(capacity_frames))
    , storage_(num_channels * capacity_, 0.f)
    , write_head_(num_channels)
    , read_head_(num_channels)
{
    for (uint32_t ch = 0; ch < num_channels_; ++ch) {
        write_head_[ch].store(0, std::memory_order_relaxed);
        read_head_[ch].store(0, std::memory_order_relaxed);
    }
}

bool AudioRingBuffer::push(uint32_t ch, const float* src, uint32_t frames) noexcept {
    assert(ch < num_channels_);
    const uint32_t mask  = capacity_ - 1;
    uint32_t       whead = write_head_[ch].load(std::memory_order_relaxed);
    const uint32_t rhead = read_head_[ch].load(std::memory_order_acquire);

    // Available space (ring is full when wrapping leaves one slot).
    const uint32_t space = (capacity_ - 1) - ((whead - rhead) & mask);
    if (frames > space) return false;

    float* base = storage_.data() + static_cast<std::ptrdiff_t>(ch) * capacity_;

    // Split write across ring boundary if needed.
    const uint32_t first_part  = std::min(frames, capacity_ - (whead & mask));
    const uint32_t second_part = frames - first_part;

    std::memcpy(base + (whead & mask), src, first_part * sizeof(float));
    if (second_part > 0)
        std::memcpy(base, src + first_part, second_part * sizeof(float));

    write_head_[ch].store(whead + frames, std::memory_order_release);
    return true;
}

uint32_t AudioRingBuffer::pop(uint32_t ch, float* dst, uint32_t max_frames) noexcept {
    assert(ch < num_channels_);
    const uint32_t mask  = capacity_ - 1;
    uint32_t       rhead = read_head_[ch].load(std::memory_order_relaxed);
    const uint32_t whead = write_head_[ch].load(std::memory_order_acquire);

    const uint32_t avail = (whead - rhead) & (capacity_ * 2 - 1);
    const uint32_t frames = std::min(avail, max_frames);
    if (frames == 0) return 0;

    const float* base = storage_.data() + static_cast<std::ptrdiff_t>(ch) * capacity_;
    const uint32_t first_part  = std::min(frames, capacity_ - (rhead & mask));
    const uint32_t second_part = frames - first_part;

    std::memcpy(dst, base + (rhead & mask), first_part * sizeof(float));
    if (second_part > 0)
        std::memcpy(dst + first_part, base, second_part * sizeof(float));

    read_head_[ch].store(rhead + frames, std::memory_order_release);
    return frames;
}

uint32_t AudioRingBuffer::available() const noexcept {
    const uint32_t w = write_head_[0].load(std::memory_order_acquire);
    const uint32_t r = read_head_[0].load(std::memory_order_relaxed);
    return (w - r) & (capacity_ * 2 - 1);
}

void AudioRingBuffer::clear() noexcept {
    for (uint32_t ch = 0; ch < num_channels_; ++ch) {
        write_head_[ch].store(0, std::memory_order_relaxed);
        read_head_[ch].store(0, std::memory_order_relaxed);
    }
}

} // namespace mamba::dsp
