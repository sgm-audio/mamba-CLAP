#pragma once
// ModelLoader — proposed asynchronous model-loading interface.
//
// STATUS: model sizes, mmap behavior, shared-page savings, and callback handoff
// below are design hypotheses; no packaged checkpoints or end-to-end loader/runtime
// path is validated. Audit ownership, lifetimes, errors, and callback behavior
// before relying on this design. In particular, model variants are not shipped.
//
// Proposed approach: load weights on a background thread, optionally share an
// immutable module, then notify the processor without doing loading work on the
// audio callback. Actual memory/timing savings and thread safety remain unmeasured.

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <filesystem>
#include <torch/script.h>

namespace mamba::core { class PluginCore; }

namespace mamba::model {

// ── Variant ──────────────────────────────────────────────────────────────────
enum class ModelVariant : uint8_t { Tiny = 0, Base = 1, Large = 2 };

// ── WeightStore — process-global singleton ────────────────────────────────────
// Ensures multiple plugin instances share the same loaded weights.
class WeightStore {
public:
    static WeightStore& instance();

    // Returns cached module or loads synchronously (used in tests only).
    std::shared_ptr<torch::jit::Module> get_or_load(ModelVariant variant,
                                                     const std::filesystem::path& root);

    void evict(ModelVariant variant);

private:
    WeightStore() = default;
    std::mutex  mutex_;
    std::unordered_map<uint8_t, std::weak_ptr<torch::jit::Module>> cache_;
};

// ── ModelLoader ───────────────────────────────────────────────────────────────
class ModelLoader {
public:
    using ReadyCallback = std::function<void(std::shared_ptr<torch::jit::Module>)>;

    explicit ModelLoader(std::filesystem::path model_root);
    ~ModelLoader();

    // Non-blocking: starts background load; calls |cb| on the loader thread
    // when complete.  The callback must be thread-safe.
    void load_async(ModelVariant variant,
                    torch::Device target_device,
                    ReadyCallback cb);

    // Cancel in-flight load (e.g. user changed variant mid-load).
    void cancel();

    bool is_loading() const noexcept { return loading_.load(std::memory_order_relaxed); }

private:
    void load_worker(ModelVariant variant, torch::Device device, ReadyCallback cb);

    static std::filesystem::path variant_path(const std::filesystem::path& root,
                                               ModelVariant variant);

    std::filesystem::path  model_root_;
    std::thread            worker_;
    std::atomic<bool>      loading_{false};
    std::atomic<bool>      cancel_{false};
};

} // namespace mamba::model
