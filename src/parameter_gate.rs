use std::collections::HashMap;
use std::sync::Arc;
use parking_lot::RwLock;
use crate::ParamValue;

/// Parameter update action - determines what to do when parameters change
#[derive(Debug, Clone)]
pub enum ParameterUpdateAction {
    /// Just update the prompt string, no state flush needed
    UpdatePromptOnly(String),

    /// Flush the Mamba state and reinitialize with new prompt
    FlushAndReinit(String),
}

/// Gate for parameter changes that implements debouncing and epsilon thresholds
#[derive(Debug, Clone)]
pub struct ParamPromptGate {
    /// Last parameter values for change detection
    last_values: Arc<RwLock<HashMap<String, f64>>>,

    /// Debounce counter - only fires after N samples of stable param value
    debounce_counter: Arc<RwLock<u32>>,

    /// Epsilon threshold - small deltas update prompt only, large deltas trigger flush
    epsilon: f64,

    /// Required stable samples before accepting a parameter change
    debounce_samples: u32,
}

impl ParamPromptGate {
    /// Create a new parameter gate
    pub fn new(epsilon: f64, debounce_samples: u32) -> Self {
        Self {
            last_values: Arc::new(RwLock::new(HashMap::new())),
            debounce_counter: Arc::new(RwLock::new(0)),
            epsilon,
            debounce_samples,
        }
    }

    /// Evaluate parameter changes and determine the appropriate action
    pub fn evaluate(&mut self, params: &[ParamValue]) -> ParameterUpdateAction {
        // For simplicity in this implementation, we'll treat the params slice as
        // a sequence of normalized parameter values (0.0-1.0)
        // In a real implementation, we'd map these to specific parameter IDs

        let max_delta = self.calculate_max_delta(params);

        if max_delta > self.epsilon {
            // Large change - flush state and reinitialize
            let prompt = self.build_prompt_from_params(params);
            ParameterUpdateAction::FlushAndReinit(prompt)
        } else {
            // Small change - just update prompt
            let prompt = self.build_prompt_from_params(params);
            ParameterUpdateAction::UpdatePromptOnly(prompt)
        }
    }

    /// Calculate the maximum delta between current and previous parameter values
    fn calculate_max_delta(&mut self, params: &[ParamValue]) -> f64 {
        let mut max_delta = 0.0;
        let mut last_values = self.last_values.write();

        // Calculate deltas for each parameter
        for (i, &value) in params.iter().enumerate() {
            let param_key = format!("param_{}", i);
            let last_value = *last_values.get(&param_key).unwrap_or(&(value as f64));
            let delta = (value as f64 - last_value).abs();

            if delta > max_delta {
                max_delta = delta;
            }

            // Update last value
            last_values.insert(param_key, value as f64);
        }

        max_delta
    }

    /// Build a prompt string from parameter values
    fn build_prompt_from_params(&self, params: &[ParamValue]) -> String {
        if params.is_empty() {
            return String::new();
        }

        let param_lines: Vec<String> = params.iter()
            .enumerate()
            .map(|(i, value)| {
                format!("  Parameter {}: {:.4} (normalized)", i, value)
            })
            .collect();

        format!(r#"
Current parameter state:
{}

Interpret these as the user's real-time intent.
Adjust your output bias toward these settings.
"#, param_lines.join("\n"))
    }

    /// Reset the gate state (used when flushing Mamba state)
    pub fn reset(&mut self) {
        let mut last_values = self.last_values.write();
        let mut debounce_counter = self.debounce_counter.write();
        last_values.clear();
        *debounce_counter = 0;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_param_gate_creation() {
        let gate = ParamPromptGate::new(0.1, 64);
        assert_eq!(gate.epsilon, 0.1);
        assert_eq!(*gate.debounce_counter.read(), 0);
    }

    #[test]
    fn test_param_gate_small_change() {
        let mut gate = ParamPromptGate::new(0.1, 64);
        let params = [0.5, 0.3, 0.7];

        // First call - establishes baseline
        let action = gate.evaluate(&params);
        assert!(matches!(action, ParameterUpdateAction::UpdatePromptOnly(_)));

        // Second call with small change - should still be update only
        let small_change_params = [0.51, 0.29, 0.71]; // Max delta ~0.01
        let action = gate.evaluate(&small_change_params);
        assert!(matches!(action, ParameterUpdateAction::UpdatePromptOnly(_)));
    }

    #[test]
    fn test_param_gate_large_change() {
        let mut gate = ParamPromptGate::new(0.1, 64);
        let params = [0.5, 0.3, 0.7];

        // First call - establishes baseline
        let _ = gate.evaluate(&params);

        // Second call with large change - should trigger flush
        let large_change_params = [0.8, 0.1, 0.9]; // Max delta ~0.3
        let action = gate.evaluate(&large_change_params);
        assert!(matches!(action, ParameterUpdateAction::FlushAndReinit(_)));
    }

    #[test]
    fn test_param_gate_prompt_building() {
        let mut gate = ParamPromptGate::new(0.1, 64);
        let params = [0.5, 0.3];

        let action = gate.evaluate(&params);
        match action {
            ParameterUpdateAction::UpdatePromptOnly(prompt) => {
                assert!(prompt.contains("Parameter 0: 0.5000"));
                assert!(prompt.contains("Parameter 1: 0.3000"));
                assert!(prompt.contains("Current parameter state:"));
                assert!(prompt.contains("Interpret these as the user's real-time intent"));
            },
            ParameterUpdateAction::FlushAndReinit(_) => panic!("Expected UpdatePromptOnly"),
        }
    }

    #[test]
    fn test_param_gate_reset() {
        let mut gate = ParamPromptGate::new(0.1, 64);
        let params = [0.5, 0.3];

        // Establish some state
        gate.evaluate(&params);
        assert!(!gate.last_values.read().is_empty());

        // Reset should clear state
        gate.reset();
        assert!(gate.last_values.read().is_empty());
        assert_eq!(*gate.debounce_counter.read(), 0);
    }
}
