"""LUFS Loss for loudness normalization targeting."""

import torch
import torch.nn as nn
import torch.nn.functional as F


def compute_lufs(
    audio: torch.Tensor,
    sample_rate: int = 44100,
    block_size_ms: float = 400.0,
) -> torch.Tensor:
    """
    Compute Integrated LUFS (Loudness Units Full Scale) using ITU-R BS.1770-4.
    
    Simplified implementation for training purposes. For broadcast-compliant
    measurements, use the `pyloudnorm` library.
    
    Args:
        audio: Audio tensor (B, T_samples)
        sample_rate: Sample rate in Hz
        block_size_ms: Block size in milliseconds
    
    Returns:
        LUFS values (B,)
    """
    # K-weighting filter (ITU-R BS.1770-4)
    # H(s) = (s^2 + 38.9) / ((s^2 + 1.03) * (s^2 + 137))
    # Discretized via bilinear transform
    
    # Simplified: apply approximate K-filter in frequency domain
    # This is a placeholder - replace with proper IIR filter for production
    
    # Mean square (energy)
    mean_square = audio.pow(2).mean(dim=-1)
    
    # Convert to dB FS
    lufs = -0.691 + 10 * torch.log10(mean_square.clamp(min=1e-10))
    
    return lufs


class LUFSLoss(nn.Module):
    """
    LUFS Loss for loudness normalization.
    
    Penalizes deviation from target integrated loudness.
    Useful for training models to hit streaming platform targets
    (-14 LUFS Spotify, -16 LUFS Apple, -23 LUFS EBU R128).
    
    Args:
        target_lufs: Target integrated loudness in LUFS (default: -14.0)
        sample_rate: Sample rate in Hz (default: 44100)
    """
    
    def __init__(
        self,
        target_lufs: float = -14.0,
        sample_rate: int = 44100,
    ):
        super().__init__()
        self.target_lufs = target_lufs
        self.sample_rate = sample_rate
    
    def forward(self, pred: torch.Tensor) -> torch.Tensor:
        """
        Compute LUFS loss.
        
        Args:
            pred: Predicted audio (B, T_samples)
        
        Returns:
            Scalar loss value (MSE between measured and target LUFS)
        """
        measured_lufs = compute_lufs(pred, self.sample_rate)
        
        # MSE loss against target
        loss = F.mse_loss(measured_lufs, measured_lufs.new_full((1,), self.target_lufs))
        
        return loss
