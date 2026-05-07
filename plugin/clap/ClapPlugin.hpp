#pragma once
// ClapPlugin — thin C-to-C++ adapter.
//
// Owns the PluginCore and exposes a clap_plugin_t with stable vtable pointers.
// All virtual-table callbacks are static functions that cast the plugin->plugin_data
// pointer back to ClapPlugin*; they must never throw.
//
// Ownership chain:
//   clap_plugin_factory_t::create_plugin()
//       └─► new ClapPlugin (heap)
//               └─► PluginCore (unique_ptr)
//   clap_plugin_t::destroy()
//       └─► delete ClapPlugin (via static callback)

#include <clap/clap.h>
#include <memory>

namespace mamba::core { class PluginCore; }

namespace mamba::clap_adapter {

class ClapPlugin {
public:
    ClapPlugin(const clap_plugin_descriptor_t* desc, const clap_host_t* host);
    ~ClapPlugin();

    ClapPlugin(const ClapPlugin&)            = delete;
    ClapPlugin& operator=(const ClapPlugin&) = delete;

    // Returns the clap_plugin_t the host will interact with.
    // Lifetime: tied to this ClapPlugin object.
    const clap_plugin_t* plugin_handle() const noexcept { return &plugin_; }

    core::PluginCore& core() noexcept { return *core_; }

    // Extension query — returns pointers to extension vtables (static storage).
    const void* get_extension(const char* id) const noexcept;

private:
    // ── clap_plugin_t vtable callbacks (static, noexcept) ────────────────────
    static bool       s_init           (const clap_plugin_t*) noexcept;
    static void       s_destroy        (const clap_plugin_t*) noexcept;
    static bool       s_activate       (const clap_plugin_t*, double, uint32_t, uint32_t) noexcept;
    static void       s_deactivate     (const clap_plugin_t*) noexcept;
    static bool       s_start_processing(const clap_plugin_t*) noexcept;
    static void       s_stop_processing (const clap_plugin_t*) noexcept;
    static void       s_reset          (const clap_plugin_t*) noexcept;
    static clap_process_status
                      s_process        (const clap_plugin_t*,
                                        const clap_process_t*) noexcept;
    static const void* s_get_extension (const clap_plugin_t*, const char*) noexcept;
    static void       s_on_main_thread (const clap_plugin_t*) noexcept;

    // Helper: recover ClapPlugin* from opaque plugin_data.
    static ClapPlugin* self(const clap_plugin_t* p) noexcept {
        return static_cast<ClapPlugin*>(p->plugin_data);
    }

    clap_plugin_t                      plugin_{};
    std::unique_ptr<core::PluginCore>  core_;
};

} // namespace mamba::clap_adapter
