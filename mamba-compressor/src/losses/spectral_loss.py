"""Multi-Resolution STFT Loss for spectral fidelity."""

import torch
import torch.nn as nn
import torch.nn.functional as F
from typing import Tuple, List


class MultiResolutionSTFTLoss(nn.Module):
    """
    Multi-Resolution Short-Time Fourier Transform Loss.
    
    Computes L1 loss between log-magnitude spectrograms at multiple
    FFT resolutions to capture both fine and coarse spectral details.
    
    Args:
        fft_sizes: List of FFT sizes (default: [512, 1024, 2048])
        hop_factor: Hop length as fraction of FFT size (default: 0.25)
        window: Window function name (default: "hann")
    """
    
    def __init__(
        self,
        fft_sizes: Tuple[int, ...] = (512, 1024, 2048),
        hop_factor: float = 0.25,
        window: str = "hann",
    ):
        super().__init__()
        self.fft_sizes = fft_sizes
        self.hop_factor = hop_factor
        self.window = window
    
    def _stft(
        self,
        x: torch.Tensor,
        n_fft: int,
        hop_length: int,
        window: torch.Tensor,
    ) -> torch.Tensor:
        """Compute STFT and return log-magnitude."""
        # Complex STFT
        spec = torch.stft(
            x,
            n_fft,
            hop_length,
            window=window,
            return_complex=True,
            center=True,
            pad_mode="reflect",
        )
        
        # Magnitude → log-magnitude
        mag = spec.abs()
        log_mag = torch.log1p(mag)
        
        return log_mag
    
    def forward(self, pred: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
        """
        Compute multi-resolution STFT loss.
        
        Args:
            pred: Predicted audio (B, T_samples)
            target: Target audio (B, T_samples)
        
        Returns:
            Scalar loss value
        """
        loss = 0.0
        num_resolutions = len(self.fft_sizes)
        
        device = pred.device
        
        for n_fft in self.fft_sizes:
            hop_length = int(n_fft * self.hop_factor)
            
            # Create window
            if self.window == "hann":
                window = torch.hann_window(n_fft, device=device)
            else:
                raise ValueError(f"Unknown window type: {self.window}")
            
            # Compute log-magnitude spectrograms
            pred_log_mag = self._stft(pred, n_fft, hop_length, window)
            target_log_mag = self._stft(target, n_fft, hop_length, window)
            
            # L1 loss
            loss += F.l1_loss(pred_log_mag, target_log_mag)
        
        return loss / num_resolutions
