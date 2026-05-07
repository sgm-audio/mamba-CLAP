#include "model/ModelLoader.hpp"
#include <stdexcept>
#include <cassert>

namespace mamba::model {

// ── WeightStore ───────────────────────────────────────────────────────────────

WeightStore& WeightStore::instance() {
    static WeightStore store;
    return store;
}

std::shared_ptr<torch::jit::Module> WeightStore::get_or_load(
    ModelVariant variant, const std::filesystem::path& root)
{
    const auto key = static_cast<uint8_t>(variant);
    std::lock_guard lock(mutex_);

    if (auto it = cache_.find(key); it != cache_.end()) {
        if (auto sp = it->second.lock()) return sp;
        cache_.erase(it);
    }

    // Load: torch::jit::load reads file, deserialises weights.
    const auto path = ModelLoader::variant_path(root, variant);
    if (!std::filesystem::exists(path))
        throw std::runtime_error("MambaFX: model file not found: " + path.string());

    auto module = std::make_shared<torch::jit::Module>(
        torch::jit::load(path.string()));
    module->eval();

    cache_.emplace(key, module);
    return module;
}

void WeightStore::evict(ModelVariant variant) {
    std::lock_guard lock(mutex_);
    cache_.erase(static_cast<uint8_t>(variant));
}

// ── ModelLoader ───────────────────────────────────────────────────────────────

ModelLoader::ModelLoader(std::filesystem::path model_root)
    : model_root_(std::move(model_root))
{}

ModelLoader::~ModelLoader() {
    cancel();
    if (worker_.joinable()) worker_.join();
}

void ModelLoader::load_async(ModelVariant variant,
                              torch::Device device,
                              ReadyCallback cb)
{
    cancel();
    if (worker_.joinable()) worker_.join();

    cancel_.store(false, std::memory_order_relaxed);
    loading_.store(true,  std::memory_order_release);

    worker_ = std::thread(&ModelLoader::load_worker, this,
                           variant, device, std::move(cb));
}

void ModelLoader::cancel() {
    cancel_.store(true, std::memory_order_release);
}

void ModelLoader::load_worker(ModelVariant variant,
                               torch::Device device,
                               ReadyCallback cb)
{
    try {
        if (cancel_.load(std::memory_order_relaxed)) { loading_.store(false); return; }

        auto module = WeightStore::instance().get_or_load(variant, model_root_);

        if (cancel_.load(std::memory_order_relaxed)) { loading_.store(false); return; }

        // Move weights to target device.
        module->to(device);
        module->eval();

        loading_.store(false, std::memory_order_release);
        if (cb) cb(std::move(module));
    }
    catch (const std::exception& ex) {
        loading_.store(false, std::memory_order_release);
        // In production: log via clap_host_t::log extension.
        (void)ex;
        if (cb) cb(nullptr);   // signal failure
    }
}

std::filesystem::path ModelLoader::variant_path(const std::filesystem::path& root,
                                                  ModelVariant variant)
{
    switch (variant) {
    case ModelVariant::Tiny:  return root / "mamba_fx_tiny.torchscript";
    case ModelVariant::Base:  return root / "mamba_fx_base.torchscript";
    case ModelVariant::Large: return root / "mamba_fx_large.torchscript";
    }
    return root / "mamba_fx_base.torchscript";
}

} // namespace mamba::model
