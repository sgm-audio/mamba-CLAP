use std::collections::HashMap;
use std::sync::{Arc};
use serde::{Serialize, Deserialize};
use std::error::Error;
use crate::mamba_ssm_state::{MambaState, SharedMambaState};

/// Represents a single layer in the prompt hierarchy
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PromptLayer {
    /// The prompt text content
    pub content: String,

    /// When this layer was last updated (for debugging/tracing)
    pub timestamp: u64,

    /// Whether this layer is active (some layers may be conditionally active)
    pub active: bool,

    /// Priority for layer ordering (lower numbers = higher priority)
    pub priority: usize,
}

impl PromptLayer {
    pub fn new<S: Into<String>>(content: S, priority: usize) -> Self {
        Self {
            content: content.into(),
            timestamp: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_secs(),
            active: true,
            priority,
        }
    }

    pub fn is_empty(&self) -> bool {
        self.content.trim().is_empty()
    }

    pub fn update_content<S: Into<String>>(&mut self, content: S) {
        self.content = content.into();
        self.timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs();
    }
}

/// The complete multi-layer prompt system
#[derive(Debug, Clone)]
pub struct PromptSystem {
    /// Layer 0: Foundation prompt (compile-time, immutable)
    pub foundation: PromptLayer,

    /// Layer 1: Plugin role prompt (init-time, semi-stable)
    pub plugin_role: PromptLayer,

    /// Layer 2: Session context prompt (transport-aware)
    pub session_context: PromptLayer,

    /// Layer 3: Parameter prompt (real-time, gate-locked)
    pub parameter: PromptLayer,

    /// Layer 4: Frame context (per-inference-chunk)
    pub frame_context: PromptLayer,

    /// Thread-safe shared state for the Mamba SSM state
    pub mamba_state: SharedMambaState,

    /// Configuration for the prompt system
    pub config: PromptSystemConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PromptSystemConfig {
    /// Maximum tokens allowed in the final assembled prompt
    pub max_tokens: usize,

    /// Chunk size for audio processing (samples)
    pub chunk_size: usize,

    /// Sample rate for audio processing
    pub sample_rate: usize,

    /// Latency budget in milliseconds
    pub latency_budget_ms: f32,

    /// Crossfade duration in samples for prompt flushes
    pub crossfade_samples: usize,

    /// Parameter change debounce threshold (samples)
    pub param_debounce_samples: usize,

    /// Parameter change epsilon threshold for triggering flush vs update
    pub param_epsilon: f32,
}

impl Default for PromptSystemConfig {
    fn default() -> Self {
        Self {
            max_tokens: 512,
            chunk_size: 2048,
            sample_rate: 44100,
            latency_budget_ms: 5.0,
            crossfade_samples: 128,
            param_debounce_samples: 64,
            param_epsilon: 0.1,
        }
    }
}

impl PromptSystem {
    /// Create a new prompt system with default configuration
    pub fn new<S: Into<String>>(foundation_content: S, plugin_role_content: S) -> Self {
        Self {
            foundation: PromptLayer::new(foundation_content, 0), // Highest priority
            plugin_role: PromptLayer::new(plugin_role_content, 1),
            session_context: PromptLayer::new("", 2), // Initially empty
            parameter: PromptLayer::new("", 3), // Initially empty
            frame_context: PromptLayer::new("", 4), // Initially empty
            mamba_state: SharedMambaState::default(),
            config: PromptSystemConfig::default(),
        }
    }

    /// Update the session context layer (call on transport changes, BPM changes, etc.)
    pub fn update_session_context(&mut self, transport_info: &TransportInfo, audio_format: &AudioFormat) {
        let prompt = self.build_session_prompt(transport_info, audio_format);
        self.session_context.update_content(prompt.clone());
        self.session_context.active = !prompt.trim().is_empty();
    }

    /// Update the parameter layer (called from audio thread on parameter changes)
    pub fn update_parameter_prompt(&mut self, params: &HashMap<ParamId, ParamValue>) -> ParameterUpdateAction {
        // Simple implementation - in reality this would use the gating logic from the architecture doc
        let prompt = self.build_parameter_prompt(params);
        let should_flush = self.should_flush_for_params(params);

        if should_flush {
            self.parameter.update_content(prompt.clone());
            ParameterUpdateAction::FlushAndReinit(prompt)
        } else {
            self.parameter.update_content(prompt.clone());
            ParameterUpdateAction::UpdatePromptOnly(prompt)
        }
    }

    /// Update the frame context layer (called per inference chunk)
    pub fn update_frame_context(&mut self, features: &AudioFeatures) {
        let prompt = self.build_frame_prompt(features);
        self.frame_context.update_content(prompt.clone());
        self.frame_context.active = !prompt.trim().is_empty();
    }

    /// Assemble the complete prompt from all active layers in priority order
    pub fn assemble_prompt(&self) -> String {
        let mut layers: Vec<&PromptLayer> = vec![
            &self.foundation,
            &self.plugin_role,
            &self.session_context,
            &self.parameter,
            &self.frame_context,
        ];

        // Filter out inactive layers and sort by priority
        layers.retain(|layer| layer.active);
        layers.sort_by_key(|layer| layer.priority);

        // Join with separator
        layers.iter()
            .map(|layer| layer.content.as_str())
            .filter(|s| !s.trim().is_empty())
            .collect::<Vec<&str>>()
            .join("\n\n---\n\n")
    }

    /// Get the current Mamba state (thread-safe)
    pub fn get_mamba_state(&self) -> parking_lot::RwLockReadGuard<MambaState> {
        self.mamba_state.read()
    }

    /// Get a mutable reference to the Mamba state (thread-safe)
    pub fn get_mamba_state_mut(&self) -> parking_lot::RwLockWriteGuard<MambaState> {
        self.mamba_state.write()
    }

    /// Reset the Mamba state to zero (used on session changes)
    pub fn flush_mamba_state(&self, d_model: usize, d_conv: usize, n_layers: usize) -> Result<(), Box<dyn Error>> {
        let mut state = self.get_mamba_state_mut();
        *state = MambaState::zeros(1, d_model, d_conv, n_layers, &state.device)?;
        Ok(())
    }

    // Helper methods to build each layer's prompt content

    fn build_session_prompt(&self, transport: &TransportInfo, format: &AudioFormat) -> String {
        format!(r#"
Session context:
- BPM: {bpm:.2}
- Time signature: {ts_num}/{ts_denom}
- Sample rate: {sample_rate} Hz
- Buffer size: {buffer_size} samples ({buffer_ms:.2}ms)
- Transport position: {position_bars:.3} bars
- Key: {key} {mode}
- Loop: {loop_active}

Recalibrate your temporal expectations to this session.
All feature windows are relative to buffer boundaries at this sample rate.
"#,
            bpm = transport.bpm,
            ts_num = transport.tsig_num,
            ts_denom = transport.tsig_denom,
            sample_rate = format.sample_rate,
            buffer_size = format.frames_count,
            buffer_ms = (format.frames_count as f64 / format.sample_rate as f64) * 1000.0,
            position_bars = transport.song_pos_beats,
            key = transport.key_signature.root(),
            mode = transport.key_signature.mode(),
            loop_active = transport.is_looping,
        )
    }

    fn build_parameter_prompt(&self, params: &HashMap<ParamId, ParamValue>) -> String {
        if params.is_empty() {
            return String::new();
        }

        let param_lines: Vec<String> = params.iter()
            .map(|(id, value)| {
                format!("  {}: {:.4} (normalized)", id, value)
            })
            .collect();

        format!(r#"
Current parameter state:
{}

Interpret these as the user's real-time intent.
Adjust your output bias toward these settings.
"#, param_lines.join("\n"))
    }

    fn build_frame_prompt(&self, features: &AudioFeatures) -> String {
        format!(r#"
Audio frame context:
- RMS: {rms:.4} ({rms_db:.1} dBFS)
- Peak: {peak:.4} ({peak_db:.1} dBFS)
- Crest factor: {crest:.2}
- Spectral centroid: {centroid:.1} Hz
- Spectral flatness: {flatness:.4}
- Zero crossing rate: {zcr:.4}
- Onset detected: {onset}
- Estimated pitch: {pitch:.1} Hz (confidence: {pitch_conf:.2})
- Harmonic-to-noise ratio: {hnr:.1} dB

Analyze this frame relative to session context and parameter intent.
"#,
            rms = features.rms,
            rms_db = 20.0 * features.rms.log10(),
            peak = features.peak,
            peak_db = 20.0 * features.peak.log10(),
            crest = features.peak / features.rms.max(1e-10),
            centroid = features.spectral_centroid,
            flatness = features.spectral_flatness,
            zcr = features.zero_crossing_rate,
            onset = features.onset_detected,
            pitch = features.fundamental_freq.unwrap_or(0.0),
            pitch_conf = features.pitch_confidence.unwrap_or(0.0),
            hnr = features.hnr_db,
        )
    }

    /// Determine if parameter changes should trigger a state flush vs just prompt update
    fn should_flush_for_params(&self, _params: &HashMap<ParamId, ParamValue>) -> bool {
        // Simplified implementation - in reality this would compare against previous values
        // and apply the epsilon/gating logic from the architecture doc
        false
    }
}

/// Action to take when parameters are updated
#[derive(Debug, Clone)]
pub enum ParameterUpdateAction {
    /// Just update the prompt string, no state flush needed
    UpdatePromptOnly(String),

    /// Flush the Mamba state and reinitialize with new prompt
    FlushAndReinit(String),
}

/// Transport information from the host
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TransportInfo {
    pub bpm: f64,
    pub tsig_num: i32,   // time signature numerator
    pub tsig_denom: i32, // time signature denominator
    pub song_pos_beats: f64, // position in beats
    pub key_signature: KeySignature,
    pub is_looping: bool,
}

/// Audio format information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioFormat {
    pub sample_rate: usize,
    pub frames_count: usize, // buffer size in samples
    pub channels: usize,
}

/// Key signature information
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum KeySignature {
    C, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B,
}

impl KeySignature {
    pub fn root(&self) -> &'static str {
        match self {
            KeySignature::C => "C",
            KeySignature::Cs => "C#",
            KeySignature::D => "D",
            KeySignature::Ds => "D#",
            KeySignature::E => "E",
            KeySignature::F => "F",
            KeySignature::Fs => "F#",
            KeySignature::G => "G",
            KeySignature::Gs => "G#",
            KeySignature::A => "A",
            KeySignature::As => "A#",
            KeySignature::B => "B",
        }
    }

    pub fn mode(&self) -> &'static str {
        // Simplified - in reality this would track major/minor/etc
        "maj"
    }
}

/// Parameter identifier
#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub struct ParamId {
    pub name: String,
    pub id: u32, // CLAP parameter ID
}

impl std::fmt::Display for ParamId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name)
    }
}

/// Parameter value (normalized 0.0-1.0)
pub type ParamValue = f32;

/// Audio features extracted per frame
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AudioFeatures {
    pub rms: f32,
    pub peak: f32,
    pub spectral_centroid: f32,
    pub spectral_flatness: f32,
    pub zero_crossing_rate: f32,
    pub onset_detected: bool,
    pub fundamental_freq: Option<f32>,
    pub pitch_confidence: Option<f32>,
    pub hnr_db: f32,
}

impl Default for AudioFeatures {
    fn default() -> Self {
        Self {
            rms: 0.0,
            peak: 0.0,
            spectral_centroid: 0.0,
            spectral_flatness: 0.0,
            zero_crossing_rate: 0.0,
            onset_detected: false,
            fundamental_freq: None,
            pitch_confidence: None,
            hnr_db: 0.0,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_prompt_system_creation() {
        let ps = PromptSystem::new(
            "Foundation prompt",
            "Plugin role prompt"
        );

        assert!(!ps.foundation.content.is_empty());
        assert!(!ps.plugin_role.content.is_empty());
        assert!(ps.session_context.content.is_empty()); // Initially empty
        assert!(ps.parameter.content.is_empty()); // Initially empty
        assert!(ps.frame_context.content.is_empty()); // Initially empty
    }

    #[test]
    fn test_prompt_assembly() {
        let mut ps = PromptSystem::new(
            "Foundation: You are an audio processor",
            "Role: Transient shaper"
        );

        // Update some layers
        ps.session_context.update_content("Session: BPM 120");
        ps.parameter.update_content("Params: mix 0.7");
        ps.frame_context.update_content("Frame: RMS -20dB");

        let assembled = ps.assemble_prompt();

        assert!(assembled.contains("Foundation: You are an audio processor"));
        assert!(assembled.contains("Role: Transient shaper"));
        assert!(assembled.contains("Session: BPM 120"));
        assert!(assembled.contains("Params: mix 0.7"));
        assert!(assembled.contains("Frame: RMS -20dB"));

        // Check that layers are separated by the divider
        assert!(assembled.contains("\n\n---\n\n"));
    }

    #[test]
    fn test_inactive_layers_are_omitted() {
        let mut ps = PromptSystem::new("Foundation", "Role");

        // Leave session, parameter, and frame empty (inactive by default)
        let assembled = ps.assemble_prompt();

        // Should only contain foundation and role
        assert!(assembled.contains("Foundation"));
        assert!(assembled.contains("Role"));
        // Should not contain the divider since only two active layers
        assert_eq!(assembled.matches("\n\n---\n\n").count(), 1);
    }

    #[test]
    fn test_prompt_layer_ordering() {
        let mut ps = PromptSystem::new("Foundation (prio 0)", "Role (prio 1)");

        // Manually set priorities to test ordering
        ps.foundation.priority = 2;
        ps.plugin_role.priority = 0;
        ps.session_context.priority = 1;

        ps.session_context.update_content("Session");
        ps.foundation.update_content("Foundation");
        ps.plugin_role.update_content("Role");

        let assembled = ps.assemble_prompt();

        // Role (prio 0) should come first
        let role_pos = assembled.find("Role").unwrap();
        let session_pos = assembled.find("Session").unwrap();
        let foundation_pos = assembled.find("Foundation").unwrap();

        assert!(role_pos < session_pos);
        assert!(session_pos < foundation_pos);
    }

    #[test]
    fn test_mamba_state_integration() {
        let ps = PromptSystem::new("Foundation", "Role");

        // Get initial state
        {
            let state = ps.get_mamba_state();
            assert_eq!(state.time_step, 0);
        }

        // Modify state through mutable reference
        {
            let mut state = ps.get_mamba_state_mut();
            state.time_step = 42;
            assert_eq!(state.time_step, 42);
        }

        // Verify change persisted
        {
            let state = ps.get_mamba_state();
            assert_eq!(state.time_step, 42);
        }
    }

    #[test]
    fn test_session_prompt_building() {
        let ps = PromptSystem::new("Foundation", "Role");
        let transport = TransportInfo {
            bpm: 120.0,
            tsig_num: 4,
            tsig_denom: 4,
            song_pos_beats: 8.0,
            key_signature: KeySignature::C,
            is_looping: false,
        };
        let format = AudioFormat {
            sample_rate: 44100,
            frames_count: 2048,
            channels: 2,
        };

        let prompt = ps.build_session_prompt(&transport, &format);

        assert!(prompt.contains("BPM: 120.00"));
        assert!(prompt.contains("Time signature: 4/4"));
        assert!(prompt.contains("Sample rate: 44100 Hz"));
        assert!(prompt.contains("Buffer size: 2048 samples"));
        assert!(prompt.contains("Key: C maj"));
        assert!(prompt.contains("Loop: false"));
    }

    #[test]
    fn test_parameter_prompt_building() {
        let ps = PromptSystem::new("Foundation", "Role");
        let mut params = HashMap::new();
        params.insert(
            ParamId { name: "mix".to_string(), id: 1 },
            0.7
        );
        params.insert(
            ParamId { name: "threshold".to_string(), id: 2 },
            0.3
        );

        let prompt = ps.build_parameter_prompt(&params);

        assert!(prompt.contains("mix: 0.7000"));
        assert!(prompt.contains("threshold: 0.3000"));
        assert!(prompt.contains("Current parameter state:"));
        assert!(prompt.contains("Interpret these as the user's real-time intent"));
    }

    #[test]
    fn test_frame_prompt_building() {
        let ps = PromptSystem::new("Foundation", "Role");
        let features = AudioFeatures {
            rms: 0.1,
            peak: 0.8,
            spectral_centroid: 1000.0,
            spectral_flatness: 0.5,
            zero_crossing_rate: 0.1,
            onset_detected: true,
            fundamental_freq: Some(440.0),
            pitch_confidence: Some(0.9),
            hnr_db: 15.0,
        };

        let prompt = ps.build_frame_prompt(&features);

        assert!(prompt.contains("RMS: 0.1000"));
        assert!(prompt.contains("Peak: 0.8000"));
        assert!(prompt.contains("Spectral centroid: 1000.0 Hz"));
        assert!(prompt.contains("Onset detected: true"));
        assert!(prompt.contains("Estimated pitch: 440.0 Hz"));
        assert!(prompt.contains("Harmonic-to-noise ratio: 15.0 dB"));
        assert!(prompt.contains("Analyze this frame relative to session context"));
    }
}
