#include "clap/ClapFactory.hpp"
#include "clap/ClapPlugin.hpp"

#include <clap/clap.h>
#include <clap/factory/plugin-factory.h>
#include <cstring>

namespace mamba::clap_adapter {

// ─── Plugin descriptor ────────────────────────────────────────────────────────
const clap_plugin_descriptor_t kPluginDescriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id           = "com.sgm-studios.mamba-fx",
    .name         = "MambaFX",
    .vendor       = "SGM Studios",
    .url          = "https://sgm-studios.com/mamba-fx",
    .manual_url   = "https://sgm-studios.com/mamba-fx/manual",
    .support_url  = "https://sgm-studios.com/support",
    .version      = "1.0.0",
    .description  = "Mamba-SSM adaptive audio effects with ARA 2 region intelligence",
    .features     = (const char*[]){
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr
    },
};

// ─── Factory callbacks ────────────────────────────────────────────────────────
static uint32_t factory_get_plugin_count(const clap_plugin_factory_t*) noexcept {
    return 1u;
}

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(
    const clap_plugin_factory_t*, uint32_t index) noexcept
{
    return index == 0 ? &kPluginDescriptor : nullptr;
}

static const clap_plugin_t* factory_create_plugin(
    const clap_plugin_factory_t*,
    const clap_host_t*             host,
    const char*                    plugin_id) noexcept
{
    if (!clap_version_is_compatible(host->clap_version)) return nullptr;
    if (std::strcmp(plugin_id, kPluginDescriptor.id) != 0) return nullptr;

    auto* plugin = new (std::nothrow) ClapPlugin(&kPluginDescriptor, host);
    return plugin ? plugin->plugin_handle() : nullptr;
}

const clap_plugin_factory_t kPluginFactory = {
    .get_plugin_count      = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin         = factory_create_plugin,
};

} // namespace mamba::clap_adapter

// ─── Entry point (only export from the .clap shared library) ─────────────────
static bool entry_init(const char* /*plugin_path*/) noexcept { return true; }
static void entry_deinit() noexcept {}
static const void* entry_get_factory(const char* factory_id) noexcept {
    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0)
        return &mamba::clap_adapter::kPluginFactory;
    return nullptr;
}

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,
    .init         = entry_init,
    .deinit       = entry_deinit,
    .get_factory  = entry_get_factory,
};
