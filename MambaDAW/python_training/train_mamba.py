#!/usr/bin/env python3
"""
Training script for MambaDAW audio processing model.
This script creates a synthetic dataset and trains a simplified Mamba model
for audio-to-audio translation tasks (e.g., noise reduction, style transfer).
"""

import argparse
import os
import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
import librosa
from tqdm import tqdm

# Import the Mamba model definition (we'll define it here for compatibility)
class MambaLayer(nn.Module):
    def __init__(self, d_model, d_state=16, d_conv=4, expand=2):
        super().__init__()
        self.d_model = d_model
        self.d_state = d_state
        self.d_conv = d_conv
        self.expand = expand
        self.d_inner = expand * d_model
        
        # Input projection
        self.in_proj = nn.Linear(d_model, self.d_inner * 2, bias=False)
        # Output projection
        self.out_proj = nn.Linear(self.d_inner, d_model, bias=False)
        
        # Depthwise convolution
        self.conv1d = nn.Conv1d(
            in_channels=self.d_inner,
            out_channels=self.d_inner,
            kernel_size=d_conv,
            groups=self.d_inner,
            padding=d_conv - 1,
            bias=False
        )
        
        # SSM parameters
        self.A_log = nn.Parameter(torch.randn(self.d_inner, d_state))
        self.B = nn.Parameter(torch.randn(self.d_inner, d_state))
        self.C = nn.Parameter(torch.randn(d_state, self.d_inner))
        self.D = nn.Parameter(torch.randn(self.d_inner))
        
        # State
        self.register_buffer('state', torch.zeros(1, self.d_inner, d_state))
        
    def forward(self, x):
        # x: [batch, seq, d_model]
        batch, seq_len, _ = x.shape
        
        # Initialize state if needed
        if self.state.shape[0] != batch:
            self.state = torch.zeros(batch, self.d_inner, self.d_state, device=x.device)
        
        # Input projection
        xz = self.in_proj(x)  # [batch, seq, 2 * d_inner]
        x_branch, z_branch = torch.split(xz, self.d_inner, dim=-1)
        
        # Convolution
        x_transposed = x_branch.transpose(1, 2)  # [batch, d_inner, seq]
        conv_out = self.conv1d(x_transposed)  # [batch, d_inner, seq + d_conv - 1]
        conv_out = conv_out[:, :, :seq_len]  # Remove extra padding
        conv_out = torch.nn.functional.silu(conv_out)
        conv_out = conv_out.transpose(1, 2)  # [batch, seq, d_inner]
        
        # Gating
        gate = torch.nn.functional.silu(z_branch)
        gated_out = conv_out * gate
        
        # SSM recurrence (simplified step-wise)
        ssm_out = torch.zeros_like(gated_out)
        for t in range(seq_len):
            x_t = gated_out[:, t, :]  # [batch, d_inner]
            
            # Discretize A and B (Euler method)
            dt = 1.0
            A = -torch.exp(self.A_log)  # [d_inner, d_state]
            dA = torch.eye(self.d_state, device=x.device) + dt * A.T  # [d_state, d_state]
            dB = dt * self.B.T  # [d_state, d_inner]
            
            # State update: state' = A * state + B * x
            # We'll do a simplified version
            new_state = torch.zeros_like(self.state)
            for i in range(self.d_inner):
                state_i = self.state[:, i, :]  # [batch, d_state]
                x_i = x_t[:, i]  # [batch]
                A_i = A[i, :]  # [d_state]
                B_i = B[i, :]  # [d_state]
                
                # state_i_new = state_i * A_i + x_i * B_i
                new_state[:, i, :] = state_i * A_i + x_i.unsqueeze(-1) * B_i
            
            self.state = new_state
            
            # Compute output: y = C * state + D * x
            y = torch.zeros_like(x_t)
            for i in range(self.d_inner):
                state_i = self.state[:, i, :]  # [batch, d_state]
                C_i = self.C[:, i]  # [d_state]
                y += state_i * C_i  # [batch]
                y += self.D[i] * x_t[:, i]  # [batch]
            
            ssm_out[:, t, :] = y
        
        # Output projection
        output = self.out_proj(ssm_out)  # [batch, seq, d_model]
        return output

class MambaModel(nn.Module):
    def __init__(self, d_model, n_layers=4, d_state=16, d_conv=4, expand=2):
        super().__init__()
        self.d_model = d_model
        self.n_layers = n_layers
        
        # Input normalization
        self.input_norm = nn.LayerNorm(d_model)
        
        # Mamba layers
        self.layers = nn.ModuleList([
            MambaLayer(d_model, d_state, d_conv, expand)
            for _ in range(n_layers)
        ])
        
        # Output normalization
        self.output_norm = nn.LayerNorm(d_model)
        
    def forward(self, x):
        # x: [batch, seq, d_model]
        x = self.input_norm(x)
        
        # Pass through layers with residual connections
        for layer in self.layers:
            residual = x
            x = layer(x)
            x = x + residual
            
        x = self.output_norm(x)
        return x

def synthetic_audio_dataset(num_samples=1000, seq_len=16000, sample_rate=16000):
    """
    Generate synthetic audio data for training.
    Creates pairs of (noisy audio, clean audio) where:
    - Clean audio: sum of sine waves with random frequencies
    - Noise: Gaussian noise + occasional bursts
    - Noisy audio: clean + noise
    """
    clean_audios = []
    noisy_audios = []
    
    for _ in range(num_samples):
        # Generate clean audio: sum of sine waves
        t = np.linspace(0, seq_len / sample_rate, seq_len, dtype=np.float32)
        clean = np.zeros(seq_len, dtype=np.float32)
        
        # Add 2-5 sine waves with random frequencies and amplitudes
        num_sines = np.random.randint(2, 6)
        for _ in range(num_sines):
            freq = np.random.uniform(50, 800)  # Hz
            amp = np.random.uniform(0.1, 0.5)
            clean += amp * np.sin(2 * np.pi * freq * t)
        
        # Normalize
        clean = clean / (np.max(np.abs(clean)) + 1e-8)
        
        # Generate noise
        noise = np.random.normal(0, 0.1, seq_len).astype(np.float32)
        
        # Add occasional noise bursts
        if np.random.random() < 0.3:
            burst_start = np.random.randint(0, seq_len - 1000)
            burst_len = np.random.randint(100, 1000)
            noise[burst_start:burst_start + burst_len] *= np.random.uniform(2, 5)
        
        # Generate noisy audio
        noisy = clean + noise * np.random.uniform(0.1, 0.5)  # Vary noise level
        
        # Normalize again
        noisy = noisy / (np.max(np.abs(noisy)) + 1e-8)
        
        clean_audios.append(clean)
        noisy_audios.append(noisy)
    
    return np.array(noisy_audios), np.array(clean_audios)

def audio_to_log_mel_spectrogram(audio, n_fft=2048, hop_length=512, n_mels=80, sample_rate=16000):
    """
    Convert audio to log-mel spectrogram matching the C++ frontend.
    """
    # Compute STFT
    stft = librosa.stft(audio, n_fft=n_fft, hop_length=hop_length, window='hann')
    magnitude = np.abs(stft)
    
    # Convert to mel scale
    mel_basis = librosa.filters.mel(sr=sample_rate, n_fft=n_fft, n_mels=n_mels)
    mel_spec = np.dot(mel_basis, magnitude)
    
    # Log compression
    log_mel_spec = np.log(np.maximum(mel_spec, 1e-5))
    
    return log_mel_spec.T  # Return [time, mel_bins]

def log_mel_spectrogram_to_audio(log_mel_spec, n_fft=2048, hop_length=512, sample_rate=16000, n_iter=32):
    """
    Convert log-mel spectrogram back to audio using Griffin-Lim.
    Note: This is approximate and for training purposes only.
    """
    # Convert from log mel to mel
    mel_spec = np.exp(log_mel_spec.T)  # [mel_bins, time]
    
    # Approximate inverse mel to linear magnitude
    # We use a simple pseudo-inverse (not perfect but works for training)
    mel_basis = librosa.filters.mel(sr=sample_rate, n_fft=n_fft, n_mels=log_mel_spec.shape[1])
    mel_pinv = np.linalg.pinv(mel_basis)
    magnitude = np.dot(mel_pinv, mel_spec)
    
    # Griffin-Lim to reconstruct phase
    audio = librosa.griffinlim(
        magnitude, 
        n_iter=n_iter, 
        hop_length=hop_length,
        win_length=n_fft,
        window='hann'
    )
    
    return audio

def train_model(args):
    # Set device
    device = torch.device('cpu')  # Force CPU for compatibility
    print(f"Using device: {device}")
    
    # Create model
    model = MambaModel(
        d_model=args.n_mels,
        n_layers=args.n_layers,
        d_state=args.d_state,
        d_conv=args.d_conv,
        expand=args.expand
    ).to(device)
    
    # Loss and optimizer
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=args.learning_rate)
    
    # Generate synthetic dataset
    print("Generating synthetic dataset...")
    noisy_audios, clean_audios = synthetic_audio_dataset(
        num_samples=args.dataset_size,
        seq_len=args.seq_len,
        sample_rate=args.sample_rate
    )
    
    # Convert to spectrograms
    print("Converting audio to log-mel spectrograms...")
    noisy_specs = []
    clean_specs = []
    
    for i in tqdm(range(len(noisy_audios))):
        noisy_spec = audio_to_log_mel_spectrogram(
            noisy_audios[i],
            n_fft=args.n_fft,
            hop_length=args.hop_length,
            n_mels=args.n_mels,
            sample_rate=args.sample_rate
        )
        clean_spec = audio_to_log_mel_spectrogram(
            clean_audios[i],
            n_fft=args.n_fft,
            hop_length=args.hop_length,
            n_mels=args.n_mels,
            sample_rate=args.sample_rate
        )
        noisy_specs.append(noisy_spec)
        clean_specs.append(clean_spec)
    
    noisy_specs = np.array(noisy_specs)
    clean_specs = np.array(clean_specs)
    
    # Convert to PyTorch tensors
    noisy_tensor = torch.from_numpy(noisy_specs).float().to(device)  # [samples, time, mel_bins]
    clean_tensor = torch.from_numpy(clean_specs).float().to(device)
    
    # Create data loader
    dataset = torch.utils.data.TensorDataset(noisy_tensor, clean_tensor)
    dataloader = torch.utils.data.DataLoader(
        dataset, 
        batch_size=args.batch_size, 
        shuffle=True
    )
    
    # Training loop
    print("Starting training...")
    model.train()
    for epoch in range(args.epochs):
        epoch_loss = 0.0
        num_batches = 0
        
        for batch_noisy, batch_clean in tqdm(dataloader, desc=f"Epoch {epoch+1}/{args.epochs}"):
            optimizer.zero_grad()
            
            # Forward pass
            outputs = model(batch_noisy)
            loss = criterion(outputs, batch_clean)
            
            # Backward pass
            loss.backward()
            optimizer.step()
            
            epoch_loss += loss.item()
            num_batches += 1
        
        avg_loss = epoch_loss / num_batches
        print(f"Epoch {epoch+1}/{args.epochs}, Loss: {avg_loss:.6f}")
        
        # Save checkpoint
        if (epoch + 1) % args.save_interval == 0 or epoch == args.epochs - 1:
            torch.save(model.state_dict(), f"resources/mamba_model_epoch_{epoch+1}.pt")
            print(f"Checkpoint saved: resources/mamba_model_epoch_{epoch+1}.pt")
    
    # Save final model
    torch.save(model.state_dict(), "resources/mamba_model_final.pt")
    print("Final model saved: resources/mamba_model_final.pt")
    
    # Trace the model for deployment
    print("Tracing model for deployment...")
    model.eval()
    example_input = torch.randn(1, 100, args.n_mels).to(device)  # [batch, time, mel_bins]
    traced_model = torch.jit.trace(model, example_input)
    traced_model.save("resources/mamba_model.pt")
    print("Traced model saved: resources/mamba_model.pt")
    
    # Quick validation
    print("Running validation...")
    model.eval()
    with torch.no_grad():
        val_noisy = noisy_tensor[:8]  # First 8 samples
        val_clean = clean_tensor[:8]
        val_outputs = model(val_noisy)
        val_loss = criterion(val_outputs, val_clean)
        print(f"Validation Loss: {val_loss.item():.6f}")
        
        # Convert first sample back to audio for inspection
        if args.validate_audio:
            spec = val_outputs[0].cpu().numpy()  # [time, mel_bins]
            audio_reconstructed = log_mel_spectrogram_to_audio(
                spec,
                n_fft=args.n_fft,
                hop_length=args.hop_length,
                sample_rate=args.sample_rate
            )
            # Save for listening (optional)
            import soundfile as sf
            sf.write("resources/reconstructed_audio.wav", audio_reconstructed, args.sample_rate)
            print("Saved reconstructed audio to resources/reconstructed_audio.wav")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train Mamba model for audio processing")
    
    # Model parameters
    parser.add_argument('--n_mels', type=int, default=80, help='Number of mel bands')
    parser.add_argument('--n_layers', type=int, default=2, help='Number of Mamba layers')
    parser.add_argument('--d_state', type=int, default=16, help='State dimension')
    parser.add_argument('--d_conv', type=int, default=4, help='Convolution kernel size')
    parser.add_argument('--expand', type=int, default=2, help='Expansion factor')
    
    # Training parameters
    parser.add_argument('--epochs', type=int, default=10, help='Number of training epochs')
    parser.add_argument('--batch_size', type=int, default=4, help='Batch size')
    parser.add_argument('--learning_rate', type=float, default=0.001, help='Learning rate')
    parser.add_argument('--dataset_size', type=int, default=100, help='Size of synthetic dataset')
    parser.add_argument('--seq_len', type=int, default=16000, help='Audio sequence length (samples)')
    parser.add_argument('--sample_rate', type=int, default=16000, help='Audio sample rate')
    parser.add_argument('--n_fft', type=int, default=2048, help='FFT size for STFT')
    parser.add_argument('--hop_length', type=int, default=512, help='Hop length for STFT')
    parser.add_argument('--save_interval', type=int, default=5, help='Save checkpoint every N epochs')
    
    # Other options
    parser.add_argument('--validate_audio', action='store_true', help='Validate by reconstructing audio')
    
    args = parser.parse_args()
    
    # Create resources directory
    os.makedirs("resources", exist_ok=True)
    
    # Train model
    train_model(args)