use std::sync::Arc;
use parking_lot::RwLock;

/// Crossfade buffer for smooth transitions between Mamba states
#[derive(Debug, Clone)]
pub struct CrossfadeBuffer {
    /// Buffer holding the previous output for crossfading
    pub buffer: Vec<f32>,

    /// Current position in the crossfade
    pub position: usize,

    /// Total length of the crossfade
    pub length: usize,

    /// Whether crossfading is currently active
    pub active: bool,
}

impl CrossfadeBuffer {
    /// Create a new crossfade buffer
    pub fn new(length: usize) -> Self {
        Self {
            buffer: vec![0.0; length],
            position: 0,
            length,
            active: false,
        }
    }

    /// Start a crossfade with the given output
    pub fn start(&mut self, output: &[f32]) {
        // Copy the output into our buffer for crossfading
        self.buffer.clear();
        self.buffer.extend_from_slice(output);
        self.position = 0;
        self.active = true;
    }

    /// Apply crossfade between dry signal and buffered wet signal
    /// Returns the mixed output
    pub fn apply(&mut self, dry: &[f32], wet: &[f32], output: &mut [f32]) {
        if !self.active || self.position >= self.length {
            // No active crossfade, just copy wet signal
            output.copy_from_slice(wet);
            return;
        }

        // Calculate crossfade curve (linear for simplicity)
        let progress = self.position as f32 / self.length as f32;
        let dry_gain = 1.0 - progress;
        let wet_gain = progress;

        // Mix dry and wet signals
        for i in 0..output.len().min(dry.len()).min(wet.len()) {
            output[i] = dry[i] * dry_gain + wet[i] * wet_gain;
        }

        // Advance position
        self.position += 1;

        // Check if crossfade is complete
        if self.position >= self.length {
            self.active = false;
        }
    }

    /// Reset the crossfade buffer to the state `new(length)` returns
    pub fn reset(&mut self) {
        self.buffer.clear();
        self.buffer.resize(self.length, 0.0);
        self.position = 0;
        self.active = false;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_crossfade_creation() {
        let cf = CrossfadeBuffer::new(128);
        assert_eq!(cf.length, 128);
        assert!(!cf.active);
        assert_eq!(cf.position, 0);
        assert_eq!(cf.buffer.len(), 128);
    }

    #[test]
    fn test_crossfade_start() {
        let mut cf = CrossfadeBuffer::new(4);
        let input = [1.0, 2.0, 3.0, 4.0];
        cf.start(&input);

        assert!(cf.active);
        assert_eq!(cf.position, 0);
        assert_eq!(cf.buffer, vec![1.0, 2.0, 3.0, 4.0]);
    }

    fn assert_close(actual: &[f32], expected: &[f32]) {
        assert_eq!(actual.len(), expected.len());
        for (a, e) in actual.iter().zip(expected) {
            assert!((a - e).abs() < 1e-6, "got {:?}, expected {:?}", actual, expected);
        }
    }

    /// Linear fade over `length` calls: on the call made at position p the
    /// wet gain is p / length and the dry gain is 1 - p / length (they sum to
    /// 1). After `length` calls the fade is inactive and output is pure wet.
    #[test]
    fn test_crossfade_apply() {
        let mut cf = CrossfadeBuffer::new(4);
        let wet_signal = [1.0, 2.0, 3.0, 4.0];
        cf.start(&wet_signal);

        let dry_signal = [0.0, 0.0, 0.0, 0.0];
        let mut output = [0.0; 4];

        // Position 0: wet gain 0.0 (all dry)
        cf.apply(&dry_signal, &wet_signal, &mut output);
        assert_close(&output, &[0.0, 0.0, 0.0, 0.0]);
        assert_eq!(cf.position, 1);

        // Position 1: wet gain 0.25
        cf.apply(&dry_signal, &wet_signal, &mut output);
        assert_close(&output, &[0.25, 0.5, 0.75, 1.0]);
        assert_eq!(cf.position, 2);

        // Position 2: wet gain 0.5
        cf.apply(&dry_signal, &wet_signal, &mut output);
        assert_close(&output, &[0.5, 1.0, 1.5, 2.0]);
        assert_eq!(cf.position, 3);

        // Position 3: wet gain 0.75, then the fade ends
        cf.apply(&dry_signal, &wet_signal, &mut output);
        assert_close(&output, &[0.75, 1.5, 2.25, 3.0]);
        assert_eq!(cf.position, 4);
        assert!(!cf.active);

        // After the fade: pure wet
        cf.apply(&dry_signal, &wet_signal, &mut output);
        assert_close(&output, &wet_signal);
    }

    /// Dry and wet gains sum to 1 at every step (constant level for
    /// identical dry and wet input).
    #[test]
    fn test_crossfade_gains_sum_to_one() {
        let mut cf = CrossfadeBuffer::new(8);
        let x = [0.5_f32, -1.0, 2.0];
        cf.start(&x);
        let mut output = [0.0; 3];
        for _ in 0..10 {
            cf.apply(&x, &x, &mut output);
            assert_close(&output, &x);
        }
    }

    #[test]
    fn test_crossfade_reset() {
        let mut cf = CrossfadeBuffer::new(4);
        cf.start(&[1.0, 2.0, 3.0, 4.0]);
        cf.reset();

        assert!(!cf.active);
        assert_eq!(cf.position, 0);
        assert_eq!(cf.buffer.len(), 4);
        for val in &cf.buffer {
            assert_eq!(*val, 0.0);
        }
    }
}
