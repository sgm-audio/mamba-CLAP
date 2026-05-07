import torch
import torch.nn as nn

# Mock Mamba layer for testing without actual mamba-ssm
class MockMamba(nn.Module):
    def __init__(self, d_model, d_state=16, d_conv=4, expand=2):
        super().__init__()
        self.d_model = d_model
        # Simple linear transformation as placeholder
        self.linear = nn.Linear(d_model, d_model)
        
    def forward(self, x):
        # x shape: (Batch, Seq_Len, Dim)
        return self.linear(x)

class MambaSpectrogramPredictor(nn.Module):
    def __init__(self, 
                 n_mels: int = 80,       # Berklee standard for speech/music hybrid mel-specs
                 d_model: int = 512,     # Hidden dimension size
                 n_layer: int = 8,       # Number of Mamba blocks
                 d_state: int = 16,      # SSM state expansion factor
                 d_conv: int = 4,        # Local 1D conv width (crucial for transient smearing prevention)
                 expand: int = 2         # Block expansion factor
    ):
        super().__init__()
        self.n_mels = n_mels
        
        # Project raw spectrogram features into the Mamba latent space
        self.input_proj = nn.Sequential(
            nn.Linear(n_mels, d_model),
            nn.GELU(),
            nn.LayerNorm(d_model)
        )
        
        # Stacked Mamba blocks for linear-time global routing
        self.layers = nn.ModuleList([
            MockMamba(
                d_model=d_model,
                d_state=d_state,
                d_conv=d_conv,
                expand=expand
            ) for _ in range(n_layer)
        ])
        
        # Project back to audio feature space
        self.output_proj = nn.Linear(d_model, n_mels)
        
    def forward(self, x):
        """
        Args:
            x: Tensor of shape (Batch, Channels/Mels, Time)
        Returns:
            out: Predicted Tensor of shape (Batch, Channels/Mels, Time)
        """
        # Mamba expects (Batch, Seq_Len, Dim)
        x = x.transpose(1, 2) 
        
        # 1. Input Projection
        hidden = self.input_proj(x)
        
        # 2. Sequential SSM Processing
        # In a real distributed setup, we would add gradient checkpointing here
        for layer in self.layers:
            hidden = layer(hidden)
            
        # 3. Output Projection
        out = self.output_proj(hidden)
        
        # Convert back to audio-native shape (Batch, Channels, Time)
        out = out.transpose(1, 2)
        
        return out

# --- Quick Validation ---
if __name__ == "__main__":
    batch_size = 4
    mels = 80
    time_steps = 1000 # Roughly 10 seconds of audio at a standard hop size
    
    model = MambaSpectrogramPredictor()
    dummy_audio_features = torch.randn(batch_size, mels, time_steps)
    
    # Warmup and test
    output = model(dummy_audio_features)
    print(f"Input shape: {dummy_audio_features.shape}")
    print(f"Output shape: {output.shape}")
    print("Model test successful!")