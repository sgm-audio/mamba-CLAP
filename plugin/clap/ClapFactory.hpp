#pragma once
// ClapFactory — implements clap_plugin_factory_t.
// Called once at library load; returns the plugin descriptor and creates
// plugin instances. This is the sole entry point from the clap_entry symbol.

#include <clap/clap.h>

namespace mamba::clap_adapter {

// Plugin descriptor — lives in static storage.
extern const clap_plugin_descriptor_t kPluginDescriptor;

// Factory vtable — lives in static storage.
extern const clap_plugin_factory_t kPluginFactory;

// clap_entry — the one symbol exported from the .clap shared library.
// Defined as extern "C" in ClapFactory.cpp.

} // namespace mamba::clap_adapter
