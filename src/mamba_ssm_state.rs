use std::hash::{Hash, Hasher};
use std::sync::Arc;
use serde::{Serialize, Deserialize};
use std::error::Error;

/// Mamba-SSM state structure that maintains the recurrent hidden state across audio chunks
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MambaState {
    /// The recurrent hidden state (h) of the Mamba-SSM
    /// In a real implementation, this would be a tensor, but for now we'll use a placeholder
    pub hidden_state: Vec<f32>,
    
    /// Convolutional state for causal convolutions
    pub conv_state: Option<Vec<f32>>,
    
    /// Time step counter for position embeddings
    pub time_step: usize,
    
    /// Layer index (for multi-layer SSMs)
    pub layer_idx: usize,
    
    /// Device information (CPU/GPU placeholder)
    pub device: String,
}

impl MambaState {
    /// Create a new zero-initialized Mamba state
    pub fn zeros(
        batch_size: usize, 
        d_model: usize, 
        d_conv: usize, 
        n_layers: usize,
        device: &str
    ) -> Result<Self, Box<dyn Error>> {
        // Hidden state: [batch, d_model, n_layers] flattened
        let hidden_state_len = batch_size * d_model * n_layers;
        let hidden_state = vec![0.0f32; hidden_state_len];
        
        // Convolutional state: [batch, d_conv, d_model, n_layers] flattened
        let conv_state = if d_conv > 0 {
            let conv_state_len = batch_size * d_conv * d_model * n_layers;
            Some(vec![0.0f32; conv_state_len])
        } else {
            None
        };
        
        Ok(Self {
            hidden_state,
            conv_state,
            time_step: 0,
            layer_idx: 0,
            device: device.to_string(),
        })
    }
    
    /// Create a new state from existing tensors (for serialization/deserialization)
    pub fn from_tensors(
        hidden_state: Vec<f32>,
        conv_state: Option<Vec<f32>>,
        time_step: usize,
        layer_idx: usize,
        device: String,
    ) -> Self {
        Self {
            hidden_state,
            conv_state,
            time_step,
            layer_idx,
            device,
        }
    }
    
    /// Reset the state to zeros while preserving dimensions
    pub fn reset(&mut self) -> Result<(), Box<dyn Error>> {
        self.hidden_state.iter_mut().for_each(|x| *x = 0.0);
        if let Some(ref mut conv_state) = self.conv_state {
            conv_state.iter_mut().for_each(|x| *x = 0.0);
        }
        self.time_step = 0;
        Ok(())
    }
    
    /// Increment the time step counter
    pub fn increment_time_step(&mut self, steps: usize) {
        self.time_step += steps;
    }
    
    /// Get the current time step
    pub fn get_time_step(&self) -> usize {
        self.time_step
    }
    
    /// Clone the state (deep copy)
    pub fn clone_state(&self) -> Self {
        Self {
            hidden_state: self.hidden_state.clone(),
            conv_state: self.conv_state.clone(),
            time_step: self.time_step,
            layer_idx: self.layer_idx,
            device: self.device.clone(),
        }
    }
    
    /// Serialize state to bytes for storage/transmission
    pub fn serialize(&self) -> Result<Vec<u8>, Box<dyn Error>> {
        Ok(serde_json::to_vec(self)?)
    }
    
    /// Deserialize state from bytes
    pub fn deserialize(buf: &[u8]) -> Result<Self, Box<dyn Error>> {
        Ok(serde_json::from_slice(buf)?)
    }
    
    /// Validate state dimensions match expected shapes
    pub fn validate_dimensions(&self, _expected_d_model: usize, _expected_n_layers: usize) -> Result<(), Box<dyn Error>> {
        // For simplicity, we're just checking that we have reasonable data
        if self.hidden_state.is_empty() {
            return Err(Box::new(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "Hidden state is empty"
            )));
        }
        
        // Validate conv state if present
        if let Some(ref conv_state) = self.conv_state {
            if conv_state.is_empty() {
                return Err(Box::new(std::io::Error::new(
                    std::io::ErrorKind::InvalidData,
                    "Conv state is empty but expected data"
                )));
            }
        }
        
        Ok(())
    }
}

// Hash implementation for use in caches/maps
impl Hash for MambaState {
    fn hash<H: Hasher>(&self, state: &mut H) {
        // Hash the essential components
        self.time_step.hash(state);
        self.layer_idx.hash(state);
        self.device.hash(state);
        self.hidden_state.len().hash(state);
        
        if let Some(ref conv_state) = self.conv_state {
            conv_state.len().hash(state);
        }
    }
}

// PartialEq implementation for state comparison
impl PartialEq for MambaState {
    fn eq(&self, other: &Self) -> bool {
        self.time_step == other.time_step &&
        self.layer_idx == other.layer_idx &&
        self.hidden_state == other.hidden_state &&
        self.conv_state == other.conv_state &&
        self.device == other.device
    }
}

impl Eq for MambaState {}

/// Wrapper for thread-safe sharing of Mamba state between audio and inference threads
#[derive(Debug, Clone)]
pub struct SharedMambaState {
    inner: Arc<parking_lot::RwLock<MambaState>>,
}

impl SharedMambaState {
    /// Create a new shared state
    pub fn new(state: MambaState) -> Self {
        Self {
            inner: Arc::new(parking_lot::RwLock::new(state)),
        }
    }
    
    /// Read the current state (blocks briefly)
    pub fn read(&self) -> parking_lot::RwLockReadGuard<MambaState> {
        self.inner.read()
    }
    
    /// Write a new state (blocks briefly)
    pub fn write(&self) -> parking_lot::RwLockWriteGuard<MambaState> {
        self.inner.write()
    }
    
    /// Try to read without blocking
    pub fn try_read(&self) -> Option<parking_lot::RwLockReadGuard<MambaState>> {
        self.inner.try_read()
    }
    
    /// Try to write without blocking
    pub fn try_write(&self) -> Option<parking_lot::RwLockWriteGuard<MambaState>> {
        self.inner.try_write()
    }
}

impl Default for SharedMambaState {
    fn default() -> Self {
        // This should be initialized with proper dimensions elsewhere
        Self::new(MambaState {
            hidden_state: vec![0.0],
            conv_state: None,
            time_step: 0,
            layer_idx: 0,
            device: "cpu".to_string(),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_mamba_state_creation() -> Result<(), Box<dyn Error>> {
        let state = MambaState::zeros(2, 64, 4, 3, "cpu")?;
        assert_eq!(state.hidden_state.len(), 2 * 64 * 3); // batch * d_model * n_layers
        assert!(state.conv_state.is_some());
        assert_eq!(state.conv_state.as_ref().unwrap().len(), 2 * 4 * 64 * 3); // batch * d_conv * d_model * n_layers
        assert_eq!(state.time_step, 0);
        assert_eq!(state.layer_idx, 0);
        assert_eq!(state.device, "cpu");
        Ok(())
    }
    
    #[test]
    fn test_mamba_state_reset() -> Result<(), Box<dyn Error>> {
        let mut state = MambaState::zeros(1, 32, 2, 2, "cpu")?;
        state.hidden_state.iter_mut().for_each(|x| *x = 1.0);
        if let Some(ref mut conv) = state.conv_state {
            conv.iter_mut().for_each(|x| *x = 2.0);
        }
        
        state.reset()?;
        
        // Check that all values are zero
        for val in &state.hidden_state {
            assert_eq!(*val, 0.0);
        }
        
        if let Some(ref conv) = state.conv_state {
            for val in conv {
                assert_eq!(*val, 0.0);
            }
        }
        
        assert_eq!(state.time_step, 0);
        Ok(())
    }
    
    #[test]
    fn test_mamba_state_serialization() -> Result<(), Box<dyn Error>> {
        let mut original = MambaState::zeros(1, 16, 2, 1, "cpu")?;
        original.hidden_state.iter_mut().for_each(|x| *x = 0.5);
        if let Some(ref mut conv) = original.conv_state {
            conv.iter_mut().for_each(|x| *x = 0.3);
        }
        original.time_step = 100;
        original.layer_idx = 1;
        
        let serialized = original.serialize()?;
        let deserialized = MambaState::deserialize(&serialized)?;
        
        assert_eq!(deserialized.time_step, 100);
        assert_eq!(deserialized.layer_idx, 1);
        assert_eq!(deserialized.hidden_state.len(), original.hidden_state.len());
        assert_eq!(deserialized.conv_state.as_ref().unwrap().len(), 
                  original.conv_state.as_ref().unwrap().len());
        assert_eq!(deserialized.device, original.device);
        
        Ok(())
    }
    
    #[test]
    fn test_shared_mamba_state() {
        let state = MambaState::zeros(1, 8, 2, 1, "cpu").unwrap();
        let shared = SharedMambaState::new(state);
        
        // Test read
        {
            let guard = shared.read();
            assert_eq!(guard.time_step, 0);
        }
        
        // Test write
        {
            let mut guard = shared.write();
            guard.time_step = 42;
            assert_eq!(guard.time_step, 42);
        }
        
        // Verify change persisted
        {
            let guard = shared.read();
            assert_eq!(guard.time_step, 42);
        }
    }
}