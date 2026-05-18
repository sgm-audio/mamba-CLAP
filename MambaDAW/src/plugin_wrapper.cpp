#include <clap/clap.h>
#include <clap/factory/plugin-factory.h>
#include <clap/ext/params.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/state.h>
#include <clap/ext/note-ports.h>
#include <clap/ext/timer-support.h>
#include <clap/ext/latency.h>
#include <clap/ext/log.h>
#include <clap/ext/tail.h>
#include "plugin_instance.h"
#include "parameter_gate.h"
#include "dsp/effect_params.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

#ifdef MAMBA_ENABLE_GUI
#include <clap/ext/gui.h>
#include "gui/gui_bridge.h"
#endif

static constexpr uint32_t MAX_CHANNELS = 8u;
static float* s_input_ptrs[MAX_CHANNELS] = {};
static float* s_output_ptrs[MAX_CHANNELS] = {};

static constexpr clap_id kGuiTimerId = 0x4D424152;
static constexpr uint32_t kGuiTimerPeriodMs = 33;

static const char* const s_features[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    nullptr
};

static const clap_plugin_descriptor_t s_descriptor = {
    CLAP_VERSION_INIT,
    "com.mambabased.MambaDAW",
    "MambaDAW",
    "Mamba Based",
    "https://github.com/state-spaces/mamba",
    "", "", "1.0.0",
    "Real-time Mamba SSM audio processing (state-centric, CPU-only)",
    s_features
};

// =============================================================================
// clap_plugin_t callbacks
// =============================================================================

static bool plugin_init(const clap_plugin_t* plugin) {
	auto* inst = get_instance(plugin);

	if (inst->host && !clap_version_is_compatible(inst->host->clap_version)) {
		char errbuf[256];
		std::snprintf(errbuf, sizeof(errbuf),
			"MambaDAW: incompatible host CLAP version %d.%d.%d (plugin built for %d.%d.%d) — init failed",
			inst->host->clap_version.major,
			inst->host->clap_version.minor,
			inst->host->clap_version.revision,
			CLAP_VERSION_MAJOR, CLAP_VERSION_MINOR, CLAP_VERSION_REVISION);
		inst->log(CLAP_LOG_ERROR, errbuf);
		return false;
	}

	const char* model_path = std::getenv("MAMBA_MODEL_PATH");
    if (model_path && model_path[0] != '\0') {
        if (!inst->processor.loadModel(std::string(model_path))) {
            char errbuf[256];
            std::snprintf(errbuf, sizeof(errbuf),
                "MambaDAW: model load failed from MAMBA_MODEL_PATH='%s' — plugin will bypass",
                model_path);
            inst->log(CLAP_LOG_ERROR, errbuf);
        }
    }
    if (inst->host) {
		const clap_host_timer_support_t* host_timer =
			static_cast<const clap_host_timer_support_t*>(
				inst->host->get_extension(inst->host, CLAP_EXT_TIMER_SUPPORT));
		if (host_timer && host_timer->register_timer) {
			clap_id tid = CLAP_INVALID_ID;
			if (host_timer->register_timer(inst->host, kGuiTimerPeriodMs, &tid))
				inst->gui_timer_id = tid;
		}
	}
	return true;
}

static void plugin_destroy(const clap_plugin_t* plugin) {
	auto* inst = get_instance(plugin);
	if (inst->gui_timer_id != CLAP_INVALID_ID && inst->host) {
		const clap_host_timer_support_t* host_timer =
			static_cast<const clap_host_timer_support_t*>(
				inst->host->get_extension(inst->host, CLAP_EXT_TIMER_SUPPORT));
		if (host_timer && host_timer->unregister_timer)
			host_timer->unregister_timer(inst->host, inst->gui_timer_id);
	}
	delete inst;
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

static bool plugin_start_processing(const clap_plugin_t* /*plugin*/) { return true; }
static void plugin_stop_processing(const clap_plugin_t* /*plugin*/) {}

static void plugin_reset(const clap_plugin_t* plugin) {
    get_instance(plugin)->processor.resetState();
}

static clap_process_status plugin_process(const clap_plugin_t* plugin,
                                           const clap_process_t* process) {
  if (!process || process->audio_inputs_count == 0 || process->audio_outputs_count == 0)
    return CLAP_PROCESS_SLEEP;

  const clap_audio_buffer_t* in_bus = &process->audio_inputs[0];
  const clap_audio_buffer_t* out_bus = &process->audio_outputs[0];

  const uint32_t in_ch = std::min(in_bus->channel_count, MAX_CHANNELS);
  const uint32_t out_ch = std::min(out_bus->channel_count, MAX_CHANNELS);

  for (uint32_t c = 0; c < in_ch; ++c) s_input_ptrs[c] = in_bus->data32[c];
  for (uint32_t c = 0; c < out_ch; ++c) s_output_ptrs[c] = out_bus->data32[c];

  auto* inst = get_instance(plugin);

  if (process->transport) {
    const clap_event_transport_t* t = process->transport;
    double bpm = (t->flags & CLAP_TRANSPORT_HAS_TEMPO) ? t->tempo : 0.0;
    double tsig_num = static_cast<double>(t->tsig_num);
    double tsig_denom = static_cast<double>(t->tsig_denom);
    double pos = (t->flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE)
                 ? t->song_pos_seconds : 0.0;
    bool is_playing = (t->flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
    bool is_looping = (t->flags & CLAP_TRANSPORT_IS_LOOP_ACTIVE) != 0;
    bool has_seeked = false;
    if (is_playing && inst->last_was_playing) {
            double expected_advance = static_cast<double>(process->frames_count) / inst->processor.sampleRate();
      double delta = pos - inst->last_transport_pos;
      if (delta < 0.0 || delta > expected_advance * 3.0)
        has_seeked = true;
    }
    inst->last_transport_pos = pos;
    inst->last_was_playing = is_playing;
    inst->processor.updateSessionFromTransport(bpm, tsig_num, tsig_denom, pos,
                                                is_playing, is_looping, has_seeked);
  }

  inst->processor.processBlock(
    s_input_ptrs, s_output_ptrs,
    static_cast<int32_t>(process->frames_count),
    static_cast<int32_t>(in_ch),
    static_cast<int32_t>(out_ch)
  );

  if (inst->processor.hasDeferredReinit()) {
    const clap_host_t* host = inst->host;
    if (host && host->request_callback)
      host->request_callback(host);
  }

  return CLAP_PROCESS_CONTINUE;
}

static const void* plugin_get_extension(const clap_plugin_t* /*plugin*/, const char* id);
static void plugin_on_main_thread(const clap_plugin_t* plugin) {
  if (!plugin) return;
  auto* inst = get_instance(plugin);
  inst->processor.handleDeferredReinit();
}

// =============================================================================
// CLAP_EXT_PARAMS — 12 parameters
// =============================================================================

struct ParamDef {
    clap_id id;
    const char* name;
    const char* module;
    double min_value;
    double max_value;
    double default_value;
    bool is_stepped;
    bool is_bypass;
};

static const ParamDef s_param_defs[kNumClapParams] = {
  { 0, "Dry/Wet Mix", "Mix", 0.0, 1.0, 0.5, false, false },
  { 1, "Effect Mode", "Mode", 0.0, 1.0, 0.0, true, false },
  { 2, "Bypass", "Mix", 0.0, 1.0, 0.0, true, true },
  { 3, "Input Gain", "Gain", 0.0, 2.0, 1.0, false, false },
  { 4, "Output Gain", "Gain", 0.0, 2.0, 1.0, false, false },
  { 5, "Context Length", "Inference", 10.0, 120.0, 60.0, false, false },
  { 6, "EQ Wet", "EQ", 0.0, 1.0, 1.0, false, false },
  { 7, "Comp Wet", "Compressor",0.0, 1.0, 1.0, false, false },
  { 8, "Sat Wet", "Saturator", 0.0, 1.0, 0.0, false, false },
  { 9, "Rev Wet", "Reverb", 0.0, 1.0, 0.15, false, false },
  { 10, "Manual Comp Thresh", "Compressor",-60.0,0.0, -20.0, false, false },
  { 11, "Manual Rev Decay", "Reverb", 0.1, 8.0, 1.5, false, false },
    { 12, "Effect Type",         "Mode",         0.0,    4.0,   0.0, true,  false },
    { 13, "Reset Context",       "Inference",    0.0,    1.0,   0.0, true,  false },
    // Per-section manual enable flags (14–17)
    { 14, "EQ Manual Enable",    "EQ Manual",    0.0,    1.0,   0.0, true,  false },
    { 15, "Comp Manual Enable",  "Comp Manual",  0.0,    1.0,   0.0, true,  false },
    { 16, "Sat Manual Enable",   "Sat Manual",   0.0,    1.0,   0.0, true,  false },
    { 17, "Rev Manual Enable",   "Rev Manual",   0.0,    1.0,   0.0, true,  false },
    // Manual EQ params (18–24)
    { 18, "EQ Lo Gain",          "EQ Manual",  -12.0,  12.0,   0.0, false, false },
    { 19, "EQ Lo Freq",          "EQ Manual",   20.0, 800.0, 200.0, false, false },
    { 20, "EQ Mid Gain",         "EQ Manual",  -12.0,  12.0,   0.0, false, false },
    { 21, "EQ Mid Freq",         "EQ Manual",  200.0,5000.0,1000.0, false, false },
    { 22, "EQ Mid Q",            "EQ Manual",    0.3,   8.0,   1.0, false, false },
    { 23, "EQ Hi Gain",          "EQ Manual",  -12.0,  12.0,   0.0, false, false },
    { 24, "EQ Hi Freq",          "EQ Manual", 2000.0,20000.0,8000.0,false, false },
    // Manual compressor extra (25–28)
    { 25, "Comp Ratio",          "Comp Manual",  1.0,  20.0,   2.0, false, false },
    { 26, "Comp Attack",         "Comp Manual",  0.1, 100.0,  10.0, false, false },
    { 27, "Comp Release",        "Comp Manual", 10.0,1000.0, 100.0, false, false },
    { 28, "Comp Makeup",         "Comp Manual",  0.0,  24.0,   0.0, false, false },
    // Manual saturator (29–30)
    { 29, "Sat Drive",           "Sat Manual",   0.0,   1.0,   0.3, false, false },
    { 30, "Sat Tone",            "Sat Manual",  -1.0,   1.0,   0.0, false, false },
    // Manual reverb extra (31–32)
    { 31, "Rev PreDelay",        "Rev Manual",   0.0, 100.0,  20.0, false, false },
    { 32, "Rev Diffusion",       "Rev Manual",   0.0,   1.0,   0.5, false, false },
};

static uint32_t params_count(const clap_plugin_t* /*plugin*/) {
    return kNumClapParams;
}

static bool params_get_info(const clap_plugin_t* /*plugin*/,
                            uint32_t index,
                            clap_param_info_t* info) {
    if (!info || index >= kNumClapParams) return false;
    const auto& def = s_param_defs[index];
    std::memset(info, 0, sizeof(*info));
    info->id = def.id;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    if (def.is_stepped) info->flags |= CLAP_PARAM_IS_STEPPED;
    if (def.is_bypass)  info->flags |= CLAP_PARAM_IS_BYPASS;
    info->min_value = def.min_value;
    info->max_value = def.max_value;
    info->default_value = def.default_value;
    info->cookie = nullptr;
    std::snprintf(info->name, sizeof(info->name), "%s", def.name);
    std::snprintf(info->module, sizeof(info->module), "%s", def.module);
    return true;
}

static bool params_get_value(const clap_plugin_t* plugin,
                             clap_id param_id, double* out_value) {
    if (!out_value || param_id >= kNumClapParams) return false;
    *out_value = get_instance(plugin)->processor.getParamValue(param_id);
    return true;
}

static bool params_value_to_text(const clap_plugin_t* /*plugin*/,
  clap_id param_id, double value,
  char* out_buf, uint32_t buf_cap) {
  if (!out_buf || buf_cap == 0) return false;
    if (param_id == 2) {
        std::snprintf(out_buf, buf_cap, "%s", value >= 0.5 ? "On" : "Off");
    } else if (param_id == 1) {
        std::snprintf(out_buf, buf_cap, "%s", value >= 0.5 ? "Manual" : "Auto");
    } else if (param_id == 5) {
        std::snprintf(out_buf, buf_cap, "%.1f s", value);
    } else if (param_id == 12) {
        static const char* kEffectTypeNames[] = {"All", "EQ", "Compressor", "Saturator", "Reverb"};
        int idx = static_cast<int>(value + 0.5);
        if (idx < 0 || idx > 4) idx = 0;
        std::snprintf(out_buf, buf_cap, "%s", kEffectTypeNames[idx]);
    } else if (param_id == 10) {
        std::snprintf(out_buf, buf_cap, "%.1f dB", value);
    } else if (param_id == 11) {
        std::snprintf(out_buf, buf_cap, "%.2f s", value);
    } else if (param_id == 13) {
        std::snprintf(out_buf, buf_cap, "%s", value >= 0.5 ? "Reset" : "—");
    } else {
        std::snprintf(out_buf, buf_cap, "%.2f", value);
    }
  return true;
}

static bool params_text_to_value(const clap_plugin_t* /*plugin*/,
 clap_id param_id,
 const char* text, double* out_value) {
 if (!text || !out_value) return false;
 switch (param_id) {
 case 1:
  if (std::strcmp(text, "Manual") == 0 || std::strcmp(text, "1") == 0) { *out_value = 1.0; return true; }
  if (std::strcmp(text, "Auto") == 0 || std::strcmp(text, "0") == 0) { *out_value = 0.0; return true; }
  break;
 case 2:
  if (std::strcmp(text, "On") == 0 || std::strcmp(text, "1") == 0) { *out_value = 1.0; return true; }
  if (std::strcmp(text, "Off") == 0 || std::strcmp(text, "0") == 0) { *out_value = 0.0; return true; }
  break;
 case 12: {
  static const char* kNames[] = {"All", "EQ", "Compressor", "Saturator", "Reverb"};
  for (int i = 0; i < 5; ++i) {
   if (std::strcmp(text, kNames[i]) == 0) { *out_value = static_cast<double>(i); return true; }
  }
  break;
 }
 case 13:
  if (std::strcmp(text, "Reset") == 0 || std::strcmp(text, "1") == 0) { *out_value = 1.0; return true; }
  if (std::strcmp(text, "—") == 0 || std::strcmp(text, "--") == 0 || std::strcmp(text, "0") == 0) { *out_value = 0.0; return true; }
  break;
 default:
  break;
 }
 *out_value = std::atof(text);
 return true;
}

static void params_flush(const clap_plugin_t* plugin,
 const clap_input_events_t* in_events,
 const clap_output_events_t* /*out_events*/) {
 if (!in_events) return;
 const uint32_t count = in_events->size(in_events);
 static constexpr uint32_t kMaxFlushEvents = 64;
 ParamSnapshot updates[kMaxFlushEvents];
 uint32_t n = 0;
 for (uint32_t i = 0; i < count && n < kMaxFlushEvents; ++i) {
  const clap_event_header_t* hdr = in_events->get(in_events, i);
  if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
   const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
   updates[n++] = {ev->param_id, ev->value};
  }
 }
 if (n > 0) {
  get_instance(plugin)->processor.processParamUpdates(updates, n);
 }
}

static const clap_plugin_params_t s_params_ext = {
    params_count, params_get_info, params_get_value,
    params_value_to_text, params_text_to_value, params_flush
};

// =============================================================================
// CLAP_EXT_AUDIO_PORTS
// =============================================================================

static uint32_t audio_ports_count(const clap_plugin_t* /*plugin*/, bool /*is_input*/) {
    return 1;
}

static bool audio_ports_get(const clap_plugin_t* /*plugin*/,
 uint32_t index, bool is_input,
 clap_audio_port_info_t* info) {
 if (index != 0 || !info) return false;
 std::memset(info, 0, sizeof(*info));
 info->id = is_input ? 0 : 1;
 info->channel_count = 2;
 info->flags = CLAP_AUDIO_PORT_IS_MAIN;
 info->port_type = CLAP_PORT_STEREO;
 info->in_place_pair = CLAP_INVALID_ID;
 std::snprintf(info->name, sizeof(info->name), "%s", is_input ? "Input" : "Output");
 return true;
}

static const clap_plugin_audio_ports_t s_audio_ports_ext = {
	audio_ports_count, audio_ports_get
};

// =============================================================================
// CLAP_EXT_NOTE_PORTS — 0 note ports (audio effect only)
// =============================================================================

static uint32_t note_ports_count(const clap_plugin_t* /*plugin*/, bool /*is_input*/) {
	return 0;
}

static bool note_ports_get(const clap_plugin_t* /*plugin*/,
	uint32_t /*index*/, bool /*is_input*/,
	clap_note_port_info_t* /*info*/) {
	return false;
}

static const clap_plugin_note_ports_t s_note_ports_ext = {
	note_ports_count, note_ports_get
};

// =============================================================================
// CLAP_EXT_TIMER_SUPPORT — GUI refresh timer
// =============================================================================

static void timer_on_timer(const clap_plugin_t* plugin, clap_id timer_id) {
    if (!plugin || timer_id != kGuiTimerId) return;
    auto* inst = get_instance(plugin);
    if (inst->processor.clipDetected()) {
        inst->log(CLAP_LOG_WARNING, "MambaDAW: output clipping detected — reduce output gain");
        inst->processor.clearClipFlag();
        const clap_host_t* host = inst->host;
        if (host && host->request_callback)
            host->request_callback(host);
    }
    static bool s_was_overloaded = false;
    bool is_overloaded = inst->processor.isCpuOverloaded();
    if (is_overloaded && !s_was_overloaded) {
        char msg[128];
        std::snprintf(msg, sizeof(msg),
            "MambaDAW: CPU overload detected (%.0f%% of RT budget) — bypassing DSP to prevent glitches",
            static_cast<double>(inst->processor.cpuLoad()) * 100.0);
        inst->log(CLAP_LOG_WARNING, msg);
    }
    if (!is_overloaded && s_was_overloaded) {
        inst->log(CLAP_LOG_INFO, "MambaDAW: CPU load recovered — resuming DSP processing");
    }
    s_was_overloaded = is_overloaded;
}

static const clap_plugin_timer_support_t s_timer_support_ext = {
timer_on_timer
};

// =============================================================================
// CLAP_EXT_STATE
// =============================================================================

static bool state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) {
    auto state = get_instance(plugin)->processor.saveState();
    if (state.empty()) return true;
    int64_t written = stream->write(stream, state.data(), static_cast<uint32_t>(state.size()));
    return written == static_cast<int64_t>(state.size());
}

static bool state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) {
  std::vector<uint8_t> buf;
  buf.reserve(4096);
  constexpr uint32_t kChunkSize = 4096;
  while (true) {
    size_t old_size = buf.size();
    buf.resize(old_size + kChunkSize);
    int64_t n = stream->read(stream, buf.data() + old_size, kChunkSize);
    if (n <= 0) {
      buf.resize(old_size);
      break;
    }
    buf.resize(old_size + static_cast<size_t>(n));
  }
    if (buf.empty()) return false;
    auto* inst = get_instance(plugin);
    bool ok = inst->processor.loadState(buf.data(), static_cast<uint32_t>(buf.size()));
    if (!ok) {
        inst->log(CLAP_LOG_ERROR, "MambaDAW: failed to restore plugin state");
    }
    if (inst->processor.checksumMismatchDetected()) {
        inst->log(CLAP_LOG_WARNING, "MambaDAW: model config checksum mismatch in saved state — Mamba state reset to defaults (model dimensions may have changed)");
    }
    if (!inst->processor.isModelLoaded()) {
        inst->log(CLAP_LOG_WARNING, "MambaDAW: no model loaded after state restore — plugin will bypass");
    }
    return ok;
}

static const clap_plugin_state_t s_state_ext = {
    state_save, state_load
};

// =============================================================================
// CLAP_EXT_LATENCY — reports STFT lookahead latency for PDC
// =============================================================================

static uint32_t latency_get(const clap_plugin_t* /*plugin*/) {
    return N_FFT - HOP_SAMPLES;
}

static const clap_plugin_latency_t s_latency_ext = {
    latency_get
};

// =============================================================================
// CLAP_EXT_TAIL — reverb tail duration for offline render
// =============================================================================

static uint32_t tail_get(const clap_plugin_t* plugin) {
    auto* inst = get_instance(plugin);
    double sr = inst->processor.sampleRate();
    if (sr <= 0.0) sr = 44100.0;
    double rev_decay = inst->processor.getParamValue(11);
    return static_cast<uint32_t>(rev_decay * sr);
}

static const clap_plugin_tail_t s_tail_ext = {
    tail_get
};

// =============================================================================
// get_extension dispatch
// =============================================================================

static const void* plugin_get_extension(const clap_plugin_t* /*plugin*/, const char* id) {
	if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &s_params_ext;
	if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &s_audio_ports_ext;
	if (std::strcmp(id, CLAP_EXT_STATE) == 0) return &s_state_ext;
	if (std::strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) return &s_note_ports_ext;
    if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0) return &s_timer_support_ext;
    if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) return &s_latency_ext;
    if (std::strcmp(id, CLAP_EXT_TAIL) == 0) return &s_tail_ext;
#ifdef MAMBA_ENABLE_GUI
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) return &s_gui_ext;
#endif
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
    const clap_host_t* host,
    const char* plugin_id) {

    if (std::strcmp(plugin_id, s_descriptor.id) != 0) return nullptr;

    auto* instance = new MambaPluginInstance();
    if (host) {
        instance->host = host;
    }
    auto* clap_plugin = new clap_plugin_t{};

    clap_plugin->desc = &s_descriptor;
    clap_plugin->plugin_data = instance;
    clap_plugin->init = plugin_init;
    clap_plugin->destroy = plugin_destroy;
    clap_plugin->activate = plugin_activate;
    clap_plugin->deactivate = plugin_deactivate;
    clap_plugin->start_processing = plugin_start_processing;
    clap_plugin->stop_processing = plugin_stop_processing;
    clap_plugin->reset = plugin_reset;
    clap_plugin->process = plugin_process;
    clap_plugin->get_extension = plugin_get_extension;
    clap_plugin->on_main_thread = plugin_on_main_thread;

    return clap_plugin;
}

static const clap_plugin_factory_t s_plugin_factory = {
    factory_get_plugin_count, factory_get_plugin_descriptor, factory_create_plugin
};

// =============================================================================
// CLAP entry point
// =============================================================================

static bool entry_init(const char* /*plugin_path*/) { return true; }
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
