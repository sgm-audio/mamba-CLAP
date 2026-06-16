#pragma once
// Atomic mirror of the host transport; written by the audio thread,
// read by inference and ARA threads without locks.

#include <atomic>
#include <cstdint>

namespace mamba::core {

struct TransportState {
    std::atomic<bool>     is_playing{false};
    std::atomic<bool>     is_recording{false};
    std::atomic<double>   sample_rate{44100.0};
    std::atomic<double>   tempo_bpm{120.0};
    std::atomic<int32_t>  time_sig_numerator{4};
    std::atomic<int32_t>  time_sig_denominator{4};
    std::atomic<int64_t>  song_pos_samples{0};   // absolute sample position
    std::atomic<uint64_t> seek_generation{0};    // incremented on every seek

    // Called on audio thread each process() cycle.
    void update(bool playing, bool recording,
                double sr, double bpm,
                int tsig_num, int tsig_den,
                int64_t pos_samples,
                bool is_seek) noexcept
    {
        is_playing.store(playing,   std::memory_order_relaxed);
        is_recording.store(recording, std::memory_order_relaxed);
        sample_rate.store(sr,       std::memory_order_relaxed);
        tempo_bpm.store(bpm,        std::memory_order_relaxed);
        time_sig_numerator.store(tsig_num, std::memory_order_relaxed);
        time_sig_denominator.store(tsig_den, std::memory_order_relaxed);
        song_pos_samples.store(pos_samples, std::memory_order_relaxed);
        if (is_seek)
            seek_generation.fetch_add(1, std::memory_order_release);
    }
};

} // namespace mamba::core
