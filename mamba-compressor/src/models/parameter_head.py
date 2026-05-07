"""Compressor parameter head for generating time-varying envelopes."""

import torch
import torch.nn as nn


class CompressorParameterHead(nn.Module):
    """
    Maps context embeddings to time-varying compressor parameters.
    
    Outputs per-frame:
      - gain_db:     direct gain reduction envelope [-40, 0] dB
      - attack_ms:   time constant for gain decrease [0.1, 100] ms
      - release_ms:  time constant for gain recovery [10, 2000] ms
    
    All outputs are smooth (sigmoid-bounded) — no hard clipping artifacts.
    
    Args:
        d_model: Input embedding dimension (default: 256)
        min_gain_db: Minimum gain in dB (default: -40.0)
        max_gain_db: Maximum gain in dB (default: 0.0)
        min_attack_ms: Minimum attack time in ms (default: 0.1)
        max_attack_ms: Maximum attack time in ms (default: 100.0)
        min_release_ms: Minimum release time in ms (default: 10.0)
        max_release_ms: Maximum release time in ms (default: 2000.0)
    """
    
    def __init__(
        self,
        d_model: int = 256,
        min_gain_db: float = -40.0,
        max_gain_db: float = 0.0,
        min_attack_ms: float = 0.1,
        max_attack_ms: float = 100.0,
        min_release_ms: float = 10.0,
        max_release_ms: float = 2000.0,
    ):
        super().__init__()
        self.min_gain_db = min_gain_db
        self.max_gain_db = max_gain_db
        self.min_attack_ms = min_attack_ms
        self.max_attack_ms = max_attack_ms
        self.min_release_ms = min_release_ms
        self.max_release_ms = max_release_ms
        
        self.head = nn.Sequential(
            nn.Linear(d_model, d_model // 2),
            nn.GELU(),
            nn.LayerNorm(d_model // 2),
            nn.Linear(d_model // 2, 3),  # [gain_db, attack, release]
        )
    
    def forward(self, context: torch.Tensor) -> dict[str, torch.Tensor]:
        """
        Generate compressor parameters from context embeddings.
        
        Args:
            context: Context tensor of shape (B, T, d_model)
        
        Returns:
            Dictionary with keys:
                - gain_db: (B, T) tensor in dB
                - attack_ms: (B, T) tensor in milliseconds
                - release_ms: (B, T) tensor in milliseconds
        """
        raw = self.head(context)  # (B, T, 3)
        
        # Gain: sigmoid-bounded to [min_gain_db, max_gain_db]
        gain_db = (
            torch.sigmoid(raw[..., 0])
            * (self.max_gain_db - self.min_gain_db)
            + self.min_gain_db
        )  # (B, T) in dB
        
        # Attack: sigmoid-bounded to [min_attack_ms, max_attack_ms]
        attack_ms = (
            torch.sigmoid(raw[..., 1])
            * (self.max_attack_ms - self.min_attack_ms)
            + self.min_attack_ms
        )  # (B, T) in ms
        
        # Release: sigmoid-bounded to [min_release_ms, max_release_ms]
        release_ms = (
            torch.sigmoid(raw[..., 2])
            * (self.max_release_ms - self.min_release_ms)
            + self.min_release_ms
        )  # (B, T) in ms
        
        return {
            "gain_db": gain_db,
            "attack_ms": attack_ms,
            "release_ms": release_ms,
        }
