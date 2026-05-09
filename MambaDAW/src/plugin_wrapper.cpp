/**
 * @file plugin_wrapper.cpp
 * @brief CLAP v1.2.7 plugin entry point for MambaDAW.
 *
 * Implements the CLAP C ABI. Bridges CLAP host calls to PluginProcessor.
 *
 * RT constraints enforced here:
 *  - process() uses pre-allocated static channel pointer arrays (no new/delete).
 *  - No logging in the audio callback path.
 *  - reset() calls PluginProcessor::resetState() — correctly implemented.
 */

#include <clap/clap.h>
#include <clap/factory/plugin-factory.h>
#include <clap/ext/params.h>
#include <clap/ext/audio-ports.h>
#include "plugin_processor.h"
#include "parameter_gate.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

// =============================================================================
// Plugin instance data — stored in clap_plugin_t::plugin_data
// =============================================================================

struct MambaPluginInstance {
    PluginProcessor processor;
    const clap_host_t* host = nullptr;

    explicit MambaPluginInstance(double sample_rate = 44100.0)
        : processor(static_cast<int>(sample_rate)) {}
};

// Helper to retrieve the instance from a CLAP plugin pointer.
static inline MambaPluginInstance* get_instance(const clap_plugin_t* plugin) {
    return static_cast<MambaPluginInstance*>(plugin->plugin_data);
}

// Pre-allocated channel pointer arrays — avoids heap allocation in RT callback.
static constexpr uint32_t MAX_CHANNELS = 8u;
static float* s_input_ptrs[MAX_CHANNELS]  = {};
static float* s_output_ptrs[MAX_CHANNELS] = {};

// =============================================================================
// Plugin descriptor
// =============================================================================

static const char* const s_features[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    nullptr
};

static const clap_plugin_descriptor_t s_descriptor = {
    CLAP_VERSION_INIT,
    /* id          */ "com.mambabased.MambaDAW",
    /* name        */ "MambaDAW",
    /* vendor      */ "Mamba Based",
    /* url         */ "https://github.com/state-spaces/mamba",
    /* manual_url  */ "",
    /* support_url */ "",
    /* version     */ "1.0.0",
    /* description */ "Real-time Mamba SSM audio processing (state-centric, CPU-only)",
    /* features    */ s_features
};

// =============================================================================
// clap_plugin_t callbacks
// =============================================================================

static bool plugin_init(const clap_plugin_t* plugin) {
    (void)plugin;
    return true;
}

static void plugin_destroy(const clap_plugin_t* plugin) {
    delete get_instance(plugin);
}

static bool plugin_activate(const clap_plugin_t* plugin,
                            double sample_rate,
                            uint32_t min_frames,
                            uint32_t max_frames) {
    get_instance(plugin)->processor.activate(sample_rate, min_frames, max_frames);
    return true;
}

static void plugin_deactivate(const clap_plugin_t* plugin) {
    get_instance(plugin)->processor.deactivate();
}

static bool plugin_start_processing(const clap_plugin_t* /*plugin*/) {
    return true;
}

static void plugin_stop_processing(const clap_plugin_t* /*plugin*/) {}

// Called by host to zero all DSP state (e.g. on transport loop jump).
// [audio-thread & active]
static void plugin_reset(const clap_plugin_t* plugin) {
    get_instance(plugin)->processor.resetState();
}

// RT audio callback — ZERO heap allocations.
static clap_process_status plugin_process(const clap_plugin_t* plugin,
                                          const clap_process_t* process) {
    if (!process || process->audio_inputs_count == 0 || process->audio_outputs_count == 0)
        return CLAP_PROCESS_SLEEP;

    const clap_audio_buffer_t* in_bus  = &process->audio_inputs[0];
    const clap_audio_buffer_t* out_bus = &process->audio_outputs[0];

    // Stack-bounded channel pointer fill — NO new[]/delete[] in RT path.
    const uint32_t in_ch  = std::min(in_bus->channel_count,  MAX_CHANNELS);
    const uint32_t out_ch = std::min(out_bus->channel_count, MAX_CHANNELS);

    for (uint32_t c = 0; c < in_ch;  ++c) s_input_ptrs[c]  = in_bus->data32[c];
    for (uint32_t c = 0; c < out_ch; ++c) s_output_ptrs[c] = out_bus->data32[c];

    get_instance(plugin)->processor.processBlock(
        s_input_ptrs,
        s_output_ptrs,
        static_cast<int32_t>(process->frames_count),
        static_cast<int32_t>(in_ch),
        static_cast<int32_t>(out_ch)
    );

    return CLAP_PROCESS_CONTINUE;
}

static const void* plugin_get_extension(const clap_plugin_t* /*plugin*/, const char* id);
static void plugin_on_main_thread(const clap_plugin_t* /*plugin*/) {}

// =============================================================================
// Extension: CLAP_EXT_PARAMS
// =============================================================================

// param_id mapping:
//   0 = Dry/Wet Mix  [0.0, 1.0]
//   1 = Bypass       [0.0, 1.0] — stepped

static uint32_t params_count(const clap_plugin_t* /*plugin*/) { return 2; }

static bool params_get_info(const clap_plugin_t* /*plugin*/,
                            uint32_t index,
                            clap_param_info_t* info) {
    if (!info) return false;
    std::memset(info, 0, sizeof(*info));

    switch (index) {
        case 0:
            info->id            = 0;
            info->flags         = CLAP_PARAM_IS_AUTOMATABLE;
            info->min_value     = 0.0;
            info->max_value     = 1.0;
            info->default_value = 0.5;
            info->cookie        = nullptr;
            std::snprintf(info->name,   sizeof(info->name),   "Dry/Wet Mix");
            std::snprintf(info->module, sizeof(info->module), "Mix");
            return true;

        case 1:
            info->id            = 2;  // maps to param_id 2 = bypass
            info->flags         = CLAP_PARAM_IS_AUTOMATABLE |
                                  CLAP_PARAM_IS_STEPPED |
                                  CLAP_PARAM_IS_BYPASS;
            info->min_value     = 0.0;
            info->max_value     = 1.0;
            info->default_value = 0.0;
            info->cookie        = nullptr;
            std::snprintf(info->name,   sizeof(info->name),   "Bypass");
            std::snprintf(info->module, sizeof(info->module), "Mix");
            return true;

        default:
            return false;
    }
}

static bool params_get_value(const clap_plugin_t* plugin,
                             clap_id param_id, double* out_value) {
    if (!out_value) return false;
    *out_value = get_instance(plugin)->processor.getParamValue(param_id);
    return true;
}

static bool params_value_to_text(const clap_plugin_t* /*plugin*/,
                                 clap_id param_id, double value,
                                 char* out_buf, uint32_t buf_cap) {
    if (param_id == 2)
        std::snprintf(out_buf, buf_cap, "%s", value >= 0.5 ? "On" : "Off");
    else
        std::snprintf(out_buf, buf_cap, "%.2f", value);
    return true;
}

static bool params_text_to_value(const clap_plugin_t* /*plugin*/,
                                 clap_id /*param_id*/,
                                 const char* text, double* out_value) {
    if (!text || !out_value) return false;
    *out_value = std::atof(text);
    return true;
}

static void params_flush(const clap_plugin_t* plugin,
                          const clap_input_events_t* in_events,
                          const clap_output_events_t* /*out_events*/) {
    if (!in_events) return;
    const uint32_t count = in_events->size(in_events);
    std::vector<ParamSnapshot> updates;
    updates.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const clap_event_header_t* hdr = in_events->get(in_events, i);
        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
            updates.push_back({ev->param_id, ev->value});
        }
    }
    if (!updates.empty()) {
        get_instance(plugin)->processor.processParamUpdates(updates);
    }
}

static const clap_plugin_params_t s_params_ext = {
    params_count,
    params_get_info,
    params_get_value,
    params_value_to_text,
    params_text_to_value,
    params_flush
};

// =============================================================================
// Extension: CLAP_EXT_AUDIO_PORTS
// =============================================================================

static uint32_t audio_ports_count(const clap_plugin_t* /*plugin*/, bool /*is_input*/) {
    return 1;
}

static bool audio_ports_get(const clap_plugin_t* /*plugin*/,
                            uint32_t index, bool /*is_input*/,
                            clap_audio_port_info_t* info) {
    if (index != 0 || !info) return false;
    std::memset(info, 0, sizeof(*info));
    info->id            = 0;
    info->channel_count = 2;
    info->flags         = CLAP_AUDIO_PORT_IS_MAIN;
    info->port_type     = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    std::snprintf(info->name, sizeof(info->name), "Main");
    return true;
}

static const clap_plugin_audio_ports_t s_audio_ports_ext = {
    audio_ports_count,
    audio_ports_get
};

// =============================================================================
// get_extension dispatch
// =============================================================================

static const void* plugin_get_extension(const clap_plugin_t* /*plugin*/, const char* id) {
    if (std::strcmp(id, CLAP_EXT_PARAMS)      == 0) return &s_params_ext;
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &s_audio_ports_ext;
    return nullptr;
}

// =============================================================================
// Plugin factory
// =============================================================================

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* /*factory*/) {
    return 1;
}

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(
    const clap_plugin_factory_t* /*factory*/, uint32_t index) {
    return (index == 0) ? &s_descriptor : nullptr;
}

static const clap_plugin_t* factory_create_plugin(
    const clap_plugin_factory_t* /*factory*/,
    const clap_host_t*            host,
    const char*                   plugin_id) {

    if (std::strcmp(plugin_id, s_descriptor.id) != 0) return nullptr;

    auto* instance = new MambaPluginInstance();
    if (host) {
        instance->host = host;
    }
    auto* clap_plugin = new clap_plugin_t{};

    clap_plugin->desc            = &s_descriptor;
    clap_plugin->plugin_data     = instance;
    clap_plugin->init            = plugin_init;
    clap_plugin->destroy         = plugin_destroy;
    clap_plugin->activate        = plugin_activate;
    clap_plugin->deactivate      = plugin_deactivate;
    clap_plugin->start_processing = plugin_start_processing;
    clap_plugin->stop_processing  = plugin_stop_processing;
    clap_plugin->reset           = plugin_reset;
    clap_plugin->process         = plugin_process;
    clap_plugin->get_extension   = plugin_get_extension;
    clap_plugin->on_main_thread  = plugin_on_main_thread;

    return clap_plugin;
}

static const clap_plugin_factory_t s_plugin_factory = {
    factory_get_plugin_count,
    factory_get_plugin_descriptor,
    factory_create_plugin
};

// =============================================================================
// CLAP entry point (DSO entry)
// =============================================================================

static bool entry_init(const char* /*plugin_path*/)  { return true; }
static void entry_deinit(void) {}

static const void* entry_get_factory(const char* factory_id) {
    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0)
        return &s_plugin_factory;
    return nullptr;
}

extern "C" {
CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    entry_init,
    entry_deinit,
    entry_get_factory
};
}
