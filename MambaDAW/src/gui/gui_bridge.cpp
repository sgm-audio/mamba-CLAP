// CLAP_EXT_GUI bridge — X11 embedded window, JUCE Component hosted inside.
//
// Threading: all GUI extension callbacks are called from the [Main] thread.
// JUCE MessageManager is initialised here; shutdownJuce_GUI() is called in destroy.
// The PluginEditor's internal 30Hz timer drives all UI updates.

#include <clap/clap.h>
#include <clap/ext/gui.h>
#include <clap/ext/params.h>
#include <cstring>

#include "../plugin_instance.h"
#include "PluginEditor.h"
#include "MambaLookAndFeel.h"

static MambaLookAndFeel s_laf;

// Convenience alias so callback bodies read cleanly.
static inline MambaPluginInstance* gui_get_instance(const clap_plugin_t* p) {
    return get_instance(p);
}

// ---------------------------------------------------------------------------
// CLAP_EXT_GUI callbacks
// ---------------------------------------------------------------------------

static bool gui_is_api_supported(const clap_plugin_t*, const char* api, bool is_floating) {
    return !is_floating && std::strcmp(api, CLAP_WINDOW_API_X11) == 0;
}

static bool gui_get_preferred_api(const clap_plugin_t*, const char** api, bool* is_floating) {
    *api        = CLAP_WINDOW_API_X11;
    *is_floating = false;
    return true;
}

static bool gui_create(const clap_plugin_t* plugin, const char* /*api*/, bool /*floating*/) {
    auto* inst = gui_get_instance(plugin);
    if (!inst) return false;
    juce::initialiseJuce_GUI();
    juce::LookAndFeel::setDefaultLookAndFeel(&s_laf);

    const clap_host_params_t* host_params = nullptr;
    if (inst->host)
        host_params = static_cast<const clap_host_params_t*>(
            inst->host->get_extension(inst->host, CLAP_EXT_PARAMS));

    inst->editor = std::make_unique<PluginEditor>(inst->processor, inst->host, host_params);
    return true;
}

static void gui_destroy(const clap_plugin_t* plugin) {
    auto* inst = gui_get_instance(plugin);
    if (!inst) return;
    if (inst->editor) {
        inst->editor->stopTimer();
        inst->editor.reset();
    }
    inst->gui_attached = false;
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    juce::shutdownJuce_GUI();
}

static bool gui_set_scale(const clap_plugin_t*, double) { return false; }

static bool gui_get_size(const clap_plugin_t*, uint32_t* w, uint32_t* h) {
    *w = 700; *h = 520; return true;
}

static bool gui_can_resize(const clap_plugin_t*)                          { return false; }
static bool gui_get_resize_hints(const clap_plugin_t*, clap_gui_resize_hints_t*) { return false; }
static bool gui_adjust_size(const clap_plugin_t*, uint32_t*, uint32_t*)  { return false; }
static bool gui_set_size(const clap_plugin_t*, uint32_t, uint32_t)       { return false; }

static bool gui_set_parent(const clap_plugin_t* plugin, const clap_window_t* window) {
    auto* inst = gui_get_instance(plugin);
    if (!inst || !inst->editor) return false;

    inst->editor->addToDesktop(0, reinterpret_cast<void*>(static_cast<uintptr_t>(window->x11)));
    inst->editor->setBounds(0, 0, 700, 520);
    inst->editor->setVisible(true);
    inst->editor->startTimerHz(30);
    inst->gui_attached = true;
    return true;
}

static bool gui_set_transient(const clap_plugin_t*, const clap_window_t*) { return false; }
static void gui_suggest_title(const clap_plugin_t*, const char*)           {}

static bool gui_show(const clap_plugin_t* plugin) {
    auto* inst = gui_get_instance(plugin);
    if (inst && inst->editor) inst->editor->setVisible(true);
    return true;
}

static bool gui_hide(const clap_plugin_t* plugin) {
    auto* inst = gui_get_instance(plugin);
    if (inst && inst->editor) inst->editor->setVisible(false);
    return true;
}

const clap_plugin_gui_t s_gui_ext = {
    gui_is_api_supported,
    gui_get_preferred_api,
    gui_create,
    gui_destroy,
    gui_set_scale,
    gui_get_size,
    gui_can_resize,
    gui_get_resize_hints,
    gui_adjust_size,
    gui_set_size,
    gui_set_parent,
    gui_set_transient,
    gui_suggest_title,
    gui_show,
    gui_hide,
};
