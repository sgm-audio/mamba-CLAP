#pragma once
// ModelLoader — asynchronous, memory-mapped weight loader.
//
// Strategy for a 150 MB Mamba model:
//   1. mmap the .pt file read-only; OS page-faults pages in on demand.
//      Multiple plugin instances in the same process share the same physical
//      pages via the shared WeightStore singleton.
//   2. A background std::thread calls torch::jit::load() passing a custom
//      InputArchive backed by the mmap region — avoids a full heap copy.
//   3. On success, WeightStore stores a shared_ptr<torch::jit::Module>.
//      Each MambaSession receives a non-owning reference; the Module lives
//      as long as any Session or the WeightStore is alive.
//   4. ModelLoader notifies the PluginCore via PluginCore::notify_model_ready()
//      which posts to AudioCommandQueue (lock-free, real-time safe).
//
// Variant dispatch:
//   ParamId::ModelVariant  → kTiny(20MB) | kBase(80MB) | kLarge(150MB)
//   Each variant is a separate TorchScript file on disk.

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
