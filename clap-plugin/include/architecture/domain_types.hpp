#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mamba_audio::architecture {

enum class Domain : std::uint8_t {
    SSMCore = 0,
    HostAPI,
    MLPipeline,
    UIState
};

enum class ThreadDomain : std::uint8_t {
    AudioThread = 0,
    MainThread,
    ARAThread
};

enum class ProcessingMode : std::uint8_t {
    TinyMicroBlock = 0,
    Block
};

enum class SnapshotReadStatus : std::uint8_t {
    Ok = 0,
    RetryRequired,
    NoData
};

struct RuntimeCpuBudget {
    // Budget is expressed as fraction of real-time callback budget.
    static constexpr double kDefaultMaxLoad = 0.30;
    static constexpr std::uint32_t kReferenceCpuCores = 4;
    static constexpr double kReferenceCpuGHz = 2.5;
};

// Export tool generates concrete values for maximum dimensions.
struct ModelConfig {
    std::uint32_t d_model = 0;
    std::uint32_t d_state = 0;
    std::uint32_t d_inner = 0;
    std::uint32_t n_layers = 0;
    std::uint32_t cond_dim = 0;
    std::uint32_t max_block_size = 0;
    std::uint64_t parameter_count = 0;

    [[nodiscard]] constexpr ProcessingMode recommendedMode() const noexcept {
        return (parameter_count <= 2'000'000ULL)
            ? ProcessingMode::TinyMicroBlock
            : ProcessingMode::Block;
    }
};

struct WeightDescriptor {
    std::string_view canonical_path;
    std::uint64_t bytes = 0;
    std::uint64_t model_hash = 0;
    std::uint64_t config_hash = 0;
};

} // namespace mamba_audio::architecture
