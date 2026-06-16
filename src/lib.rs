pub mod mamba_ssm_state;
pub mod prompt_system;
pub mod crossfade;
pub mod ring_buffer;
pub mod parameter_gate;

// Re-export key types for easier use
pub use mamba_ssm_state::{
    MambaState, SharedMambaState
};

pub use prompt_system::{
    PromptSystem, PromptSystemConfig, ParameterUpdateAction,
    TransportInfo, AudioFormat, KeySignature, ParamId, ParamValue, AudioFeatures
};

pub use crossfade::CrossfadeBuffer;

pub use ring_buffer::RingBuffer;

pub use parameter_gate::{
    ParamPromptGate, ParameterUpdateAction as ParamGateAction
};
