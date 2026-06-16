#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "architecture/domain_types.hpp"
#include "architecture/thread_contracts.hpp"

namespace mamba_audio::architecture {

class ISSMCoreRuntime {
public:
    virtual ~ISSMCoreRuntime() = default;

    // Main thread only; allocates all runtime scratch/state using max block size.
    virtual bool activate(const ModelConfig& config, std::uint32_t sample_rate_hz) noexcept = 0;
    virtual void deactivate() noexcept = 0;

    // Audio thread only. No allocation, no locking, no I/O.
    virtual void processTiny(std::span<const float> in,
                             std::span<float> out,
                             std::uint32_t micro_block_size,
                             const ConditioningFrame& cond) noexcept = 0;

    // Audio thread only. No allocation, no locking, no I/O.
    virtual void processBlock(std::span<const float> in,
                              std::span<float> out,
                              std::uint32_t block_size,
                              const ConditioningFrame& cond) noexcept = 0;

    virtual std::size_t hStateSize() const noexcept = 0;
    virtual std::span<const float> liveHState() const noexcept = 0;
    virtual void resetState() noexcept = 0;
};

class IARAConditioningProjector {
public:
    virtual ~IARAConditioningProjector() = default;

    // ARA/main thread setup.
    virtual bool configure(std::uint32_t cond_dim, const WeightDescriptor& weights) noexcept = 0;

    // Audio thread usage through preloaded projection tensors.
    virtual void project(const ConditioningFrame& cond,
                         std::span<float> out_delta_bias,
                         std::span<float> out_b_bias) const noexcept = 0;
};

class IHostProcessAdapter {
public:
    virtual ~IHostProcessAdapter() = default;

    // CLAP process wrapper entry.
    virtual void process(float* const* audio_in,
                         float* const* audio_out,
                         std::uint32_t num_channels,
                         std::uint32_t num_frames) noexcept = 0;
};

class IModePlanner {
public:
    virtual ~IModePlanner() = default;

    virtual ProcessingMode chooseMode(const ModelConfig& config,
                                      std::uint32_t sample_rate_hz,
                                      std::uint32_t host_max_block_size) const noexcept = 0;

    virtual std::uint32_t chooseChunkSize(ProcessingMode mode,
                                          std::uint32_t host_block_size) const noexcept = 0;
};

} // namespace mamba_audio::architecture
