"""Audio quality metrics."""

import torch
import numpy as np
from typing import Optional


def compute_lufs(
    audio: torch.Tensor,
    sample_rate: int = 44100,
) -> torch.Tensor:
    """
    Compute Integrated LUFS (simplified ITU-R BS.1770-4).
    
    Args:
        audio: Audio tensor (B, T_samples)
        sample_rate: Sample rate in Hz
    
    Returns:
        LUFS values (B,)
    """
    # Mean square (energy)
    mean_square = audio.pow(2).mean(dim=-1)
    
    # Convert to dB FS (simplified K-weighting)
    lufs = -0.691 + 10 * torch.log10(mean_square.clamp(min=1e-10))
    
    return lufs


def compute_crest_factor(
    audio: torch.Tensor,
    dim: int = -1,
) -> torch.Tensor:
    """
    Compute crest factor (peak / RMS).
    
    Args:
        audio: Audio tensor
        dim: Dimension to reduce over
    
    Returns:
        Crest factor values
    """
    peak = audio.abs().max(dim=dim, keepdim=True).values
    rms = audio.pow(2).mean(dim=dim, keepdim=True).sqrt()
    
    return (peak / (rms + 1e-8)).squeeze(dim)


def compute_spectral_flatness(
    audio: torch.Tensor,
    n_fft: int = 2048,
    hop_length: int = 512,
    sample_rate: int = 44100,
) -> torch.Tensor:
    """
    Compute spectral flatness (Wiener entropy).
    
    Spectral flatness measures how noise-like vs tone-like a signal is.
    Value of 1.0 = white noise, 0.0 = pure tone.
    
    Args:
        audio: Audio tensor (B, T_samples)
        n_fft: FFT size
        hop_length: Hop length
        sample_rate: Sample rate
    
    Returns:
        Spectral flatness values per frame
    """
    # STFT
    spec = torch.stft(
        audio,
        n_fft,
        hop_length,
        window=torch.hann_window(n_fft, device=audio.device),
        return_complex=True,
        center=True,
        pad_mode="reflect",
    ).abs()
    
    # Geometric mean / Arithmetic mean
    log_spec = torch.log(spec.clamp(min=1e-10))
    geometric_mean = torch.exp(log_spec.mean(dim=-2))
    arithmetic_mean = spec.mean(dim=-2)
    
    flatness = geometric_mean / (arithmetic_mean + 1e-8)
    
    return flatness.mean(dim=-1)  # Average over frames


def compute_thd(
    audio: torch.Tensor,
    fundamental_freq: float,
    sample_rate: int = 44100,
    n_harmonics: int = 5,
) -> torch.Tensor:
    """
    Compute Total Harmonic Distortion (THD).
    
    Args:
        audio: Audio tensor (B, T_samples)
        fundamental_freq: Fundamental frequency in Hz
        sample_rate: Sample rate
        n_harmonics: Number of harmonics to consider
    
    Returns:
        THD values (ratio, not dB)
    """
    # Simple DFT-based THD estimation
    n_fft = 4096
    spec = torch.fft.rfft(audio, n=n_fft, dim=-1).abs()
    
    freq_bins = torch.fft.rfftfreq(n_fft, 1/sample_rate, device=audio.device)
    
    thd_values = []
    for b in range(audio.shape[0]):
        # Find fundamental and harmonics
        fundamental_power = 0.0
        harmonic_power = 0.0
        
        for h in range(1, n_harmonics + 1):
            target_freq = fundamental_freq * h
            bin_idx = (freq_bins - target_freq).abs().argmin()
            power = spec[b, bin_idx] ** 2
            
            if h == 1:
                fundamental_power = power
            else:
                harmonic_power += power
        
        thd = torch.sqrt(harmonic_power / (fundamental_power + 1e-10))
        thd_values.append(thd)
    
    return torch.stack(thd_values)
