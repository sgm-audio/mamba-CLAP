#pragma once
// OverlapAddSynthesizer — accumulates ISTFT output frames with a Hann window
// and delivers the final time-domain output to the host audio buffer.
//
// The implementation currently sizes its vectors in the constructor and is
// intended not to grow them during synthesise(). Verify bounds and allocations
// before callback use; no full real-time audit or audio-quality validation exists.
//
// Dry/wet placement here is an architectural choice to benchmark and audition.

#include <vector>
#include <cstdint>

namespace mamba::dsp {

class OverlapAddSynthesizer {
public:
    OverlapAddSynthesizer(int n_fft, int hop_length, uint32_t max_block_frames);

    // Inference thread: push one processed ISTFT output frame [n_fft] per channel.
    void push_frame(const float* frame_ch0, const float* frame_ch1, int n_fft) noexcept;

    // Audio thread: read up to |frames| samples per channel into |dst[ch]|.
    // Applies wet/dry mix vs. |dry[ch]| (which is the unprocessed input buffer).
    void synthesise(float* const* dst, uint32_t channels,
                    uint32_t frames, float wet_mix) noexcept;

    // Reset all internal state (e.g. on transport seek).
    void reset() noexcept;

private:
    int      n_fft_;
    int      hop_length_;
    uint32_t max_block_;

    // Overlap-add accumulation buffers; one per channel.
    static constexpr int kMaxChannels = 2;
    std::vector<float>   ola_buf_[kMaxChannels];  // [n_fft + max_block] each
    int                  ola_read_head_[kMaxChannels]{};
};

} // namespace mamba::dsp
