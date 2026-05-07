"""Mel spectrogram encoder for audio semantic embedding."""

import torch
import torch.nn as nn
from typing import Optional


class MelEncoder(nn.Module):
    """
    Converts log-mel spectrogram to d_model dimensional embeddings.
    
    Input:  (B, n_mels, T) — log-mel, normalized to [-1, 1]
    Output: (B, T, d_model)
    
    Args:
        n_mels: Number of mel bands (default: 128)
        d_model: Embedding dimension (default: 256)
        dropout: Dropout rate (default: 0.1)
    """
    
    def __init__(
        self,
        n_mels: int = 128,
        d_model: int = 256,
        dropout: float = 0.1,
    ):
        super().__init__()
        self.n_mels = n_mels
        self.d_model = d_model
        
        self.proj = nn.Sequential(
            nn.Linear(n_mels, d_model),
            nn.GELU(),
            nn.Dropout(dropout),
            nn.LayerNorm(d_model),
        )
    
    def forward(self, x: torch.Tensor) -> torch.Tensor:
        """
        Forward pass.
        
        Args:
            x: Input tensor of shape (B, n_mels, T)
        
        Returns:
            Embedded tensor of shape (B, T, d_model)
        """
        # (B, n_mels, T) → (B, T, n_mels) → (B, T, d_model)
        x = x.transpose(1, 2)
        return self.proj(x)
