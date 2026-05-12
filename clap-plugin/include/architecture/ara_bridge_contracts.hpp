#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "architecture/domain_types.hpp"
#include "architecture/thread_contracts.hpp"

namespace mamba_audio::architecture {

struct ARAMusicalContext {
    double tempo_bpm = 120.0;
    std::int32_t key_signature = 0;
    std::int32_t meter_num = 4;
    std::int32_t meter_den = 4;
    double playhead_quarter_notes = 0.0;
    float energy = 0.0F;
    float spectral_tilt = 0.0F;
};

class IARAMusicalContextExtractor {
public:
    virtual ~IARAMusicalContextExtractor() = default;

    // ARA thread only.
    virtual ARAMusicalContext readContext() noexcept = 0;
};

class IConditioningEncoder {
public:
    virtual ~IConditioningEncoder() = default;

    // ARA thread only.
    virtual ConditioningFrame encode(const ARAMusicalContext& context,
                                     std::uint32_t cond_dim,
                                     std::uint64_t timeline_samples) const noexcept = 0;
};

class IARAOfflineRenderCoordinator {
public:
    virtual ~IARAOfflineRenderCoordinator() = default;

    // ARA/main thread only.
    virtual void beginOfflineRender(std::uint64_t render_id) noexcept = 0;
    virtual void endOfflineRender(std::uint64_t render_id) noexcept = 0;
};

} // namespace mamba_audio::architecture
