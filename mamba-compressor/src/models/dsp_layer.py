"""Differentiable DSP compressor with IIR ballistics and lookahead."""

import torch
import torch.nn as nn
import torch.nn.functional as F


class DifferentiableDSPCompressor(nn.Module):
    """
    Applies time-varying gain to audio in the time domain.
    
    The gain envelope from the parameter head is:
      1. Upsampled from frame-rate to sample-rate (linear interp)
      2. Smoothed with attack/release time constants (IIR ballistics)
      3. Applied as sample-accurate gain multiplication
    
    This keeps the gradient path clean: audio → gain → output is
    fully differentiable through the ballistic filter.
    
    Args:
        sample_rate: Audio sample rate in Hz (default: 44100)
        hop_length: Hop length for frame-to-sample conversion (default: 512)
        lookahead_frames: Number of frames for lookahead buffer (default: 4)
    """
    
    def __init__(
        self,
        sample_rate: int = 44100,
        hop_length: int = 512,
        lookahead_frames: int = 4,
    ):
        super().__init__()
        self.sample_rate = sample_rate
        self.hop_length = hop_length
        self.lookahead_frames = lookahead_frames
        self.lookahead_samples = lookahead_frames * hop_length
    
    def _ms_to_coeff(self, ms: torch.Tensor) -> torch.Tensor:
        """
        Convert time constant in ms to IIR smoothing coefficient.
        
        Args:
            ms: Time constant in milliseconds
        
        Returns:
            Smoothing coefficient in [0, 1]
        """
        samples = ms * self.sample_rate / 1000.0
        return torch.exp(-1.0 / (samples.clamp(min=1e-6)))
    
    def _apply_ballistics(
        self,
        gain_linear: torch.Tensor,   # (B, T_samples)
        attack_coeff: torch.Tensor,  # (B, T_samples)
        release_coeff: torch.Tensor, # (B, T_samples)
    ) -> torch.Tensor:
        """
        Per-sample IIR gain smoothing.
        
        Attack when gain is falling (compression engaging).
        Release when gain is rising (compression releasing).
        
        NOTE: This loop is the bottleneck. Replace with a CUDA scan kernel
        for production. For training it's fine.
        
        Args:
            gain_linear: Linear gain envelope
            attack_coeff: Attack smoothing coefficients
            release_coeff: Release smoothing coefficients
        
        Returns:
            Smoothed gain envelope
        """
        B, T = gain_linear.shape
        smoothed = torch.zeros_like(gain_linear)
        prev = torch.ones(B, device=gain_linear.device)
        
        for t in range(T):
            g = gain_linear[:, t]
            a = attack_coeff[:, t]
            r = release_coeff[:, t]
            
            # Choose coefficient based on direction
            coeff = torch.where(g < prev, a, r)
            prev = coeff * prev + (1 - coeff) * g
            smoothed[:, t] = prev
        
        return smoothed
    
    def forward(
        self,
        audio: torch.Tensor,            # (B, T_samples)
        params: dict[str, torch.Tensor],
    ) -> torch.Tensor:
        """
        Apply compression to audio.
        
        Args:
            audio: Input audio tensor (B, T_samples)
            params: Dictionary with keys:
                - gain_db: (B, T_frames) gain envelope in dB
                - attack_ms: (B, T_frames) attack times in ms
                - release_ms: (B, T_frames) release times in ms
        
        Returns:
            Compressed audio tensor (B, T_samples)
        """
        B = audio.shape[0]
        T_samples = audio.shape[1]
        T_frames = params["gain_db"].shape[1]
        
        # dB → linear gain
        gain_linear = 10.0 ** (params["gain_db"] / 20.0)  # (B, T_frames)
        
        # Upsample gain envelope: frame-rate → sample-rate
        gain_upsampled = F.interpolate(
            gain_linear.unsqueeze(1),
            size=T_samples,
            mode="linear",
            align_corners=False,
        ).squeeze(1)  # (B, T_samples)
        
        # Apply lookahead: shift gain envelope forward
        if self.lookahead_samples > 0:
            gain_upsampled = F.pad(
                gain_upsampled[:, :-self.lookahead_samples],
                (self.lookahead_samples, 0),
                mode="replicate",
            )
        
        # Upsample attack/release coefficients
        attack_coeff = self._ms_to_coeff(
            F.interpolate(
                params["attack_ms"].unsqueeze(1),
                size=T_samples,
                mode="linear",
                align_corners=False,
            ).squeeze(1)
        )
        release_coeff = self._ms_to_coeff(
            F.interpolate(
                params["release_ms"].unsqueeze(1),
                size=T_samples,
                mode="linear",
                align_corners=False,
            ).squeeze(1)
        )
        
        # Apply IIR ballistics
        smoothed_gain = self._apply_ballistics(
            gain_upsampled, attack_coeff, release_coeff
        )
        
        # Apply gain
        return audio * smoothed_gain
