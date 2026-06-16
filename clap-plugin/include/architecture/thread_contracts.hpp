#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

#include "architecture/domain_types.hpp"

namespace mamba_audio::architecture {

constexpr std::size_t kMaxParams = 256;
constexpr std::size_t kMaxCondDim = 256;

struct ParameterEvent {
    std::uint16_t param_id = 0;
    float value = 0.0F;
    std::uint32_t sample_offset = 0;
};

struct alignas(16) ConditioningFrame {
    std::uint64_t version = 0;
    std::uint64_t timeline_samples = 0;
    std::array<float, kMaxCondDim> cond{};
    std::uint32_t cond_dim = 0;
    std::uint32_t flags = 0;
};

class IParameterIngress {
public:
    virtual ~IParameterIngress() = default;

    // Main thread producer.
    virtual bool push(const ParameterEvent& event) noexcept = 0;

    // Audio thread consumer.
    virtual std::size_t popBulk(std::span<ParameterEvent> out) noexcept = 0;
};

class IAtomicParamBlock {
public:
    virtual ~IAtomicParamBlock() = default;

    virtual void store(std::uint16_t param_id, float value) noexcept = 0;
    virtual float load(std::uint16_t param_id) const noexcept = 0;
};

class IConditioningMailbox {
public:
    virtual ~IConditioningMailbox() = default;

    // ARA thread producer writes inactive slot and flips active index.
    virtual void publish(const ConditioningFrame& frame) noexcept = 0;

    // Audio thread reader.
    virtual ConditioningFrame loadLatest() const noexcept = 0;
};

class IStateSnapshotSeqlock {
public:
    virtual ~IStateSnapshotSeqlock() = default;

    // Audio thread writer.
    virtual void beginWrite() noexcept = 0;
    virtual void writeState(std::span<const float> h_state) noexcept = 0;
    virtual void endWrite() noexcept = 0;

    // Main thread reader.
    virtual SnapshotReadStatus readState(std::span<float> out_h_state) const noexcept = 0;
};

class IThreadLawEnforcer {
public:
    virtual ~IThreadLawEnforcer() = default;

    virtual bool isAudioThread() const noexcept = 0;
    virtual bool isMainThread() const noexcept = 0;
    virtual bool isARAThread() const noexcept = 0;
};

} // namespace mamba_audio::architecture
