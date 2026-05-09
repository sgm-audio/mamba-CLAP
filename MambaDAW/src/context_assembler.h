#pragma once

#include <string>
#include <cstdint>
#include <atomic>

/**
 * @file context_assembler.h
 * @brief Assembles the 5-layer DSP context architecture for Mamba inference.
 *
 * Real-time safety rule: assembleContext() is NOT RT-safe — it does string
 * allocations and must only be called from the inference (non-RT) thread.
 *
 * Layers:
 *   L0: Foundation Prompt (compile-time constant, baked into binary)
 *   L1: Plugin Role Prompt (init-time, describes what this plugin does)
 *   L2: Session Context Prompt (session state, can trigger SSM flush)
 *   L3: Parameter Prompt (built from ParameterGate, may trigger flush)
 *   L4: Frame Context (extracted per audio buffer, sidechain features)
 */

// Forward declaration for the real-time feature extractor
struct AudioFeatures;

/**
 * @brief Immutable foundation prompt (L0).
 *
 * Baked into binary. Specifies domain constraints, output contract, token
 * budget, and latency expectations.
 */
static constexpr const char* FOUNDATION_PROMPT = R"F0(
You are an embedded audio DSP inference engine operating inside a CLAP plugin.
Your outputs are consumed directly by audio signal processing code.
Constraints:
- ALL numerical outputs must be normalized to [-1.0, 1.0] unless unit is specified
- Never output prose, markdown, or explanations
- Output format is always: JSON with keys defined by the plugin role prompt
- Token budget is fixed at 512; truncate features, never truncate structure
- Latency budget: 5ms per inference step
)F0";

/**
 * @brief Plugin role descriptor (L1).
 *
 * Describes the plugin's purpose, input/output contract, and parameter space.
 * Constructed at init-time from plugin metadata.
 */
struct PluginRole {
    std::string name;            // Plugin name (e.g., "MambaFX")
    std::string id;              // CLAP plugin ID
    std::string vendor;          // Vendor name
    std::string version;         // Plugin version
    std::string description;     // Human-readable description
    std::string input_type;      // e.g., "mono audio, 32-bit float, time-domain"
    std::string output_schema;   // e.g., "{\"gain_db\": float, \"transient_probability\": float}"
    std::string param_summary;   // Parameter ranges and meanings
    std::string null_output;     // Zero-action fallback (JSON)

    std::string build_prompt() const {
        return
            "Plugin role: " + name + " (" + id + ")\n"
            "Vendor: " + vendor + "\n"
            "Version: " + version + "\n\n"
            "This plugin performs: " + description + "\n\n"
            "Input signal type: " + input_type + "\n"
            "Output contract: " + output_schema + "\n"
            "Parameter space: " + param_summary + "\n\n"
            "When uncertain, output the zero-action result: " + null_output;
    }
};

/**
 * @brief Session context (L2).
 *
 * Captures transport state, sample rate, buffer size, key, time signature.
 * Changes to this layer SSM state flush + full context rebuild.
 */
struct SessionContext {
    double bpm = 120.0;
    double tsig_num = 4.0;
    double tsig_denom = 4.0;
    double sample_rate = 44100.0;
    uint32_t buffer_size = 256;
    double position_bars = 0.0;
    int key_root = 0;     // 0=C, 1=Db, ... 11=B
    int key_mode = 0;     // 0=major, 1=minor (simplified)
    bool loop_active = false;

    std::string build_prompt() const {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "Session context:\n"
            "- BPM: %.2f\n"
            "- Time signature: %.0f/%.0f\n"
            "- Sample rate: %.0f Hz\n"
            "- Buffer size: %u samples (%.2fms)\n"
            "- Transport position: %.3f bars\n"
            "- Key: %d %s\n"
            "- Loop: %s\n\n"
            "Recalibrate your temporal expectations to this session.\n"
            "All feature windows are relative to buffer boundaries at this sample rate.",
            bpm, tsig_num, tsig_denom, sample_rate, buffer_size,
            (buffer_size / sample_rate) * 1000.0, position_bars,
            key_root, (key_mode == 0 ? "major" : "minor"),
            loop_active ? "active" : "inactive"
        );
        return std::string(buf);
    }
};

/**
 * @brief Parameter prompt (L3).
 *
 * Built from current plugin parameters. Does NOT own the values — it's a view.
 * The ParameterGate decides when to flush based on debounce and epsilon.
 */
struct ParameterPrompt {
    // Values are normalized [0,1] or as specified by plugin
    struct ParamValue {
        const char* name;
        double value;
    };

    std::vector<ParamValue> params;

    std::string build_prompt() const {
        std::string s = "Current parameter state:\n";
        for (const auto& p : params) {
            char line[128];
            std::snprintf(line, sizeof(line), "  %s: %.4f (normalized)\n", p.name, p.value);
            s += line;
        }
        s += "\nInterpret these as the user's real-time intent.\n";
        s += "Adjust your output bias toward these settings.\n";
        return s;
    }
};

/**
 * @brief Assembler for the layered context.
 *
 * Exists on the inference thread. Not thread-safe for concurrent calls, but
 * can be used single-threaded by an inference worker.
 */
class ContextAssembler {
public:
    ContextAssembler(const PluginRole& role, const SessionContext& session)
        : role_(role), session_(session) {}

    /**
     * @brief Set current parameter prompt layer (L3). Optional.
     */
    void set_parameter_prompt(const ParameterPrompt& prompt) {
        param_prompt_ = &prompt;
    }

    /**
     * @brief Assemble full context: L0 + L1 + L2 + L3 + L4.
     *
     * Returns a single string with layers separated by "---" delimiters.
     * L4 (frame context) is generated from features using provided callback.
     *
     * @param frame_features_func Callback that returns L4 string for current frame.
     * @return Full context string
     */
    std::string assemble_context(const std::string& (*frame_features_func)(const AudioFeatures&)) const {
        std::string ctx;
        ctx += FOUNDATION_PROMPT;
        ctx += "\n\n---\n\n";
        ctx += role_.build_prompt();
        ctx += "\n\n---\n\n";
        ctx += session_.build_prompt();
        if (param_prompt_) {
            ctx += param_prompt_->build_prompt();
            ctx += "\n\n---\n\n";
        }
        if (frame_features_func) {
            AudioFeatures dummy; // FIXME: caller must provide actual features
            ctx += frame_features_func(dummy);
        }
        return ctx;
    }

    /**
     * @brief Rebuild session context (L2). Call when transport changes.
     */
    void update_session(const SessionContext& session) {
        session_ = session;
    }

private:
    const PluginRole& role_;
    SessionContext session_;
    const ParameterPrompt* param_prompt_ = nullptr; // optional, set by inference thread
};
