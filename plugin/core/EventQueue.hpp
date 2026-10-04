#pragma once
// Fixed-capacity SPSC queue prototype for cross-thread commands. push()/pop()
// have no explicit dynamic allocation or blocking, but lock-free atomics are
// platform-dependent and the complete callback path has not been RT-audited.
// Capacity must be a power-of-two; fixed at compile time via template param.

#include <atomic>
#include <array>
#include <optional>
#include <cstddef>
#include <type_traits>

namespace mamba::core {

template<typename T, std::size_t Capacity>
    requires (Capacity >= 2 && (Capacity & (Capacity - 1)) == 0)  // power-of-two
class SPSCQueue {
public:
    static_assert(std::is_trivially_destructible_v<T>,
        "T must be trivially destructible for bounded queue teardown");

    // Producer thread only.
    [[nodiscard]] bool push(const T& item) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire))
            return false;  // full
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer thread only.
    [[nodiscard]] std::optional<T> pop() noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return std::nullopt;  // empty
        T item = buffer_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return item;
    }

    [[nodiscard]] bool empty() const noexcept {
        return tail_.load(std::memory_order_acquire)
            == head_.load(std::memory_order_acquire);
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::array<T, Capacity> buffer_{};
};

// ─── Typed command variant sent from UI/ARA threads → audio thread ───────────
enum class AudioCommand : uint8_t {
    None = 0,
    ParamChanged,      // parameter automation arrived
    TransportReset,    // host seek; Mamba recurrent state should reset
    ModelReloaded,     // background load finished; swap in new session
    Bypass,
    Unbypass,
};

struct AudioEvent {
    AudioCommand command{AudioCommand::None};
    uint32_t     param_id{0};
    float        param_value{0.f};
    void*        payload{nullptr};  // non-owning, valid for command lifetime
};

// Inference-thread → audio-thread result queue
struct InferenceResult {
    uint32_t sequence_id{0};   // monotonic; audio thread checks for freshness
    float    output_gain{1.f};
    bool     valid{false};
};

using AudioCommandQueue  = SPSCQueue<AudioEvent,       256>;
using InferenceResultQueue = SPSCQueue<InferenceResult, 64>;

} // namespace mamba::core
