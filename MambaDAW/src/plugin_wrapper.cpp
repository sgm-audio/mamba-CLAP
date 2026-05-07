#include <clap/clap.h>
#include <clap/clap-helpers.h>
#include "plugin_processor.h"
#include <iostream>
#include <cstring>

// Global processor instance (in a real plugin, this would be per-instance)
static MambaProcessor* g_processor = nullptr;

// CLAP plugin descriptor
static const clap_plugin_descriptor_t CLAP_DESCRIPTOR = {
    .clap_version = {CLAP_VERSION_MAJOR, CLAP_VERSION_MINOR},
    .plugin_id = "com.mambabased.MambaDAW",
    .name = "MambaDAW",
    .vendor = "Mamba Based",
    .url = "https://github.com/state-spaces/mamba",
    .manual_url = "",
    .support_url = "",
    .version = "1.0.0",
    .description = "High-performance AI audio processing plugin using Mamba SSM",
    .features = (const char* const[]){
        CLAP_PLUGIN_FEATURE_INSTRUMENT,
        CLAP_PLUGIN_FEATURE_AI,
        nullptr
    }
};

// Plugin factory
static const clap_plugin_factory_t CLAP_FACTORY = {
    // plugin_get_count
    [](const clap_plugin_t*) -> uint32_t {
        return 1;
    },
    // plugin_get_descriptor
    [](const clap_plugin_t*, uint32_t index) -> const clap_plugin_descriptor_t* {
        if (index == 0)
            return &CLAP_DESCRIPTOR;
        return nullptr;
    },
    // plugin_create
    [](const clap_plugin_t* plugin, const clap_host_t* host, const clap_plugin_descriptor_t* descriptor) -> bool {
        // Create processor instance
        g_processor = new MambaProcessor();
        if (!g_processor) {
            return false;
        }
        
        // Store host for logging
        // In a more complete implementation, we'd store the host pointer
        
        return true;
    },
    // plugin_destroy
    [](const clap_plugin_t* plugin, const clap_plugin_t* plugin_instance) {
        if (g_processor) {
            delete g_processor;
            g_processor = nullptr;
        }
    }
};

// Plugin entry point
extern "C" {
    const clap_plugin_entry_t* clap_entry(const clap_host_t* host) {
        static const clap_plugin_entry_t entry = {
            .plugin_init = nullptr,
            .plugin_deinit = nullptr,
            .plugin_get_factory = [](const clap_plugin_t* plugin, const char* factory_id) -> const void* {
                if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
                    return &CLAP_FACTORY;
                }
                return nullptr;
            }
        };
        return &entry;
    }
};

// Helper functions for the plugin instance
static void plugin_process(clap_plugin_t* plugin,
                          const clap_process_t* process) {
    if (!g_processor) return;
    
    // Activate if needed (simplified - in reality we'd check process flags)
    static bool activated = false;
    if (!activated && process->audio_inputs_count > 0 && process->audio_outputs_count > 0) {
        g_processor->activate(44100, process->audio_inputs[0].channels_count, process->audio_outputs[0].channels_count);
        activated = true;
    }
    
    // Process audio
    if (process->audio_inputs_count > 0 && process->audio_outputs_count > 0) {
        const clap_audio_buffer* input = &process->audio_inputs[0];
        const clap_audio_buffer* output = &process->audio_outputs[0];
        
        // Convert to format expected by our processor
        float** input_channels = new float*[input->channels_count];
        float** output_channels = new float*[output->channels_count];
        
        for (uint32_t c = 0; c < input->channels_count; ++c) {
            input_channels[c] = input->data32[c];
            output_channels[c] = output->data32[c];
        }
        
        g_processor->processBlock(
            input_channels,
            output_channels,
            process->frames_count,
            input->channels_count,
            output->channels_count
        );
        
        delete[] input_channels;
        delete[] output_channels;
    }
}

// Parameter handling
static double plugin_param_get_value(const clap_plugin_t* plugin, uint32_t param_id) {
    if (g_processor) {
        return g_processor->getParamValue(param_id);
    }
    return 0.0;
}

static void plugin_param_set_value(clap_plugin_t* plugin, uint32_t param_id, double value) {
    if (g_processor) {
        g_processor->setParamValue(param_id, value);
    }
}

// State handling
static bool plugin_state_save(const clap_plugin_t* plugin, clap_ostream* stream) {
    // Simplified state saving - in practice we'd save more parameters
    if (!g_processor) return false;
    
    float dry_wet = g_processor->getParamValue(0);
    int model_type = static_cast<int>(g_processor->getParamValue(1));
    bool bypass = static_cast<bool>(g_processor->getParamValue(2));
    
    // Write state to stream
    if (!clap_ostream_write(stream, &dry_wet, sizeof(dry_wet))) return false;
    if (!clap_ostream_write(stream, &model_type, sizeof(model_type))) return false;
    if (!clap_ostream_write(stream, &bypass, sizeof(bypass))) return false;
    
    return true;
}

static bool plugin_state_load(const clap_plugin_t* plugin, clap_istream* stream) {
    // Simplified state loading
    if (!g_processor) return false;
    
    float dry_wet;
    int model_type;
    bool bypass;
    
    // Read state from stream
    if (!clap_istream_read(stream, &dry_wet, sizeof(dry_wet))) return false;
    if (!clap_istream_read(stream, &model_type, sizeof(model_type))) return false;
    if (!clap_istream_read(stream, &bypass, sizeof(bypass))) return false;
    
    g_processor->setParamValue(0, dry_wet);
    g_processor->setParamValue(1, model_type);
    g_processor->setParamValue(2, bypass);
    
    return true;
}

// Activation/deactivation
static void plugin_activate(clap_plugin_t* plugin) {
    // Activation handled in process for simplicity
}

static void plugin_deactivate(clap_plugin_t* plugin) {
    if (g_processor) {
        g_processor->deactivate();
    }
}

// Reset
static void plugin_reset(clap_plugin_t* plugin) {
    if (g_processor) {
        // Reset would be implemented here
    }
}

// Main plugin implementation
static const clap_plugin_t CLAP_PLUGIN = {
    .plugin_size = sizeof(clap_plugin_t),
    .clap_version = {CLAP_VERSION_MAJOR, CLAP_VERSION_MINOR},
    .plugin_init = nullptr,
    .plugin_deinit = nullptr,
    .plugin_get_descriptor = nullptr, // Handled by factory
    .plugin_get_guid = nullptr,
    .plugin_get_factory = nullptr,    // Handled by factory
    .process = plugin_process,
    .audio_ports_get_count = nullptr,
    .audio_ports_get_info = nullptr,
    .audio_ports_set_active = nullptr,
    .plugin_get_param_count = [](const clap_plugin_t* plugin) -> uint32_t {
        return 3; // Dry/Wet, Model Type, Bypass
    },
    .plugin_get_param_info = [](const clap_plugin_t* plugin, uint32_t index, clap_param_info* info) -> bool {
        if (!info) return false;
        switch (index) {
            case 0: // Dry/Wet Mix
                strcpy(info->id, "dry_wet");
                strcpy(info->name, "Dry/Wet Mix");
                info->min_value = 0.0;
                info->default_value = 0.5;
                info->max_value = 1.0;
                info->is_automatable = true;
                strcpy(info->module, "Mix");
                strcpy(info->unit, "%");
                info->flags = CLAP_PARAM_IS_BYPASS;
                break;
            case 1: // Model Type
                strcpy(info->id, "model_type");
                strcpy(info->name, "Model Type");
                info->min_value = 0.0;
                info->default_value = 0.0;
                info->max_value = 0.0; // Only one model for now
                info->is_automatable = false;
                strcpy(info->module, "Model");
                strcpy(info->unit, "");
                info->flags = 0;
                break;
            case 2: // Bypass
                strcpy(info->id, "bypass");
                strcpy(info->name, "Bypass");
                info->min_value = 0.0;
                info->default_value = 0.0;
                info->max_value = 1.0;
                info->is_automatable = true;
                strcpy(info->module, "Mix");
                strcpy(info->unit, "");
                info->flags = CLAP_PARAM_IS_BYPASS;
                break;
            default:
                return false;
        }
        return true;
    },
    .plugin_get_param_value = plugin_param_get_value,
    .plugin_set_param_value = plugin_set_param_value,
    .plugin_get_latency = [](const clap_plugin_t* plugin) -> uint32_t {
        return 0; // No latency reported for simplicity
    },
    .plugin_get_tail_size = [](const clap_plugin_t* plugin) -> uint32_t {
        return 0; // No tail
    },
    .plugin_start_processing = nullptr,
    .plugin_stop_processing = nullptr,
    .plugin_state_save = plugin_state_save,
    .plugin_state_load = plugin_state_load,
    .plugin_activate = plugin_activate,
    .plugin_deactivate = plugin_deactivate,
    .plugin_reset = plugin_reset
};