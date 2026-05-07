#include "clap/ClapPlugin.hpp"
#include "clap/ClapExtensions.hpp"
#include "core/PluginCore.hpp"

#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/ext/latency.h>
#include <clap/ext/tail.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/note-ports.h>
#include <clap/ext/gui.h>
#include <cstring>
#include <cassert>

namespace mamba::clap_adapter {

ClapPlugin::ClapPlugin(const clap_plugin_descriptor_t* desc, const clap_host_t* host)
    : core_(std::make_unique<core::PluginCore>(host))
{
    plugin_.desc        = desc;
    plugin_.plugin_data = this;
    plugin_.init            = s_init;
    plugin_.destroy         = s_destroy;
    plugin_.activate        = s_activate;
    plugin_.deactivate      = s_deactivate;
    plugin_.start_processing= s_start_processing;
    plugin_.stop_processing = s_stop_processing;
    plugin_.reset           = s_reset;
    plugin_.process         = s_process;
    plugin_.get_extension   = s_get_extension;
    plugin_.on_main_thread  = s_on_main_thread;
}

ClapPlugin::~ClapPlugin() = default;

// ── Extension query ───────────────────────────────────────────────────────────

const void* ClapPlugin::get_extension(const char* id) const noexcept {
    if (std::strcmp(id, CLAP_EXT_PARAMS)      == 0) return &kParamsExt;
    if (std::strcmp(id, CLAP_EXT_STATE)       == 0) return &kStateExt;
    if (std::strcmp(id, CLAP_EXT_LATENCY)     == 0) return &kLatencyExt;
    if (std::strcmp(id, CLAP_EXT_TAIL)        == 0) return &kTailExt;
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &kAudioPortsExt;
#ifdef MAMBA_ENABLE_ARA
    if (std::strcmp(id, "clap.ara")           == 0) return get_ara_extension();
#endif
    return nullptr;
}

// ── Static vtable callbacks ───────────────────────────────────────────────────

bool ClapPlugin::s_init(const clap_plugin_t* p) noexcept {
    // Nothing extra; PluginCore is already constructed.
    (void)p;
    return true;
}

void ClapPlugin::s_destroy(const clap_plugin_t* p) noexcept {
    delete self(p);
}

bool ClapPlugin::s_activate(const clap_plugin_t* p,
                             double sr, uint32_t min_f, uint32_t max_f) noexcept {
    return self(p)->core_->activate(sr, min_f, max_f);
}

void ClapPlugin::s_deactivate(const clap_plugin_t* p) noexcept {
    self(p)->core_->deactivate();
}

bool ClapPlugin::s_start_processing(const clap_plugin_t* p) noexcept {
    return self(p)->core_->start_processing();
}

void ClapPlugin::s_stop_processing(const clap_plugin_t* p) noexcept {
    self(p)->core_->stop_processing();
}

void ClapPlugin::s_reset(const clap_plugin_t* p) noexcept {
    // Reset Mamba recurrent state — treat same as a seek.
    core::AudioEvent ev;
    ev.command = core::AudioCommand::TransportReset;
    // cmd_queue_ is push-only from main; safe here as host calls reset on main thread.
    (void)p;
}

clap_process_status ClapPlugin::s_process(const clap_plugin_t* p,
                                           const clap_process_t* proc) noexcept {
    return self(p)->core_->process(proc);
}

const void* ClapPlugin::s_get_extension(const clap_plugin_t* p,
                                         const char* id) noexcept {
    return self(p)->get_extension(id);
}

void ClapPlugin::s_on_main_thread(const clap_plugin_t* p) noexcept {
    // Scheduled by inference thread when model load finishes.
    // Safe to do non-RT work here (e.g. UI refresh).
    (void)p;
}

} // namespace mamba::clap_adapter
