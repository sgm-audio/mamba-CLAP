"""Context-Aware Compressor: Full pipeline combining Mamba SSM + Differentiable DSP."""

import torch
import torch.nn as nn
from typing import Optional

from src.models.mel_encoder import MelEncoder
from src.models.mamba_tower import MambaContextTower
from src.models.parameter_head import CompressorParameterHead
from src.models.dsp_layer import DifferentiableDSPCompressor


class ContextAwareCompressor(nn.Module):
    """
    Full pipeline: Mel → Context → Parameters → DSP → Output
    
    At inference, the Mamba state carries the full musical history.
    The model decides compression based on what it has HEARD, not just
    what it is hearing right now.
    
    Args:
        n_mels: Number of mel bands (default: 128)
        d_model: Model dimension (default: 256)
        n_mamba_layers: Number of Mamba layers (default: 6)
        d_state: SSM state size (default: 32)
        d_conv: Convolution kernel size (default: 4)
        expand: Expansion factor (default: 2)
        sample_rate: Audio sample rate in Hz (default: 44100)
        hop_length: Hop length for Mel extraction (default: 512)
        lookahead_frames: Lookahead frames for anticipatory compression (default: 4)
        min_gain_db: Minimum gain in dB (default: -40.0)
        max_gain_db: Maximum gain in dB (default: 0.0)
    """
    
    def __init__(
        self,
        n_mels: int = 128,
        d_model: int = 256,
        n_mamba_layers: int = 6,
        d_state: int = 32,
        d_conv: int = 4,
        expand: int = 2,
        sample_rate: int = 44100,
        hop_length: int = 512,
        lookahead_frames: int = 4,
        min_gain_db: float = -40.0,
        max_gain_db: float = 0.0,
    ):
        super().__init__()
        
        self.encoder = MelEncoder(
            n_mels=n_mels,
            d_model=d_model,
        )
        
        self.context = MambaContextTower(
            d_model=d_model,
            n_layers=n_mamba_layers,
            d_state=d_state,
            d_conv=d_conv,
            expand=expand,
        )
        
        self.params = CompressorParameterHead(
            d_model=d_model,
            min_gain_db=min_gain_db,
            max_gain_db=max_gain_db,
        )
        
        self.dsp = DifferentiableDSPCompressor(
            sample_rate=sample_rate,
            hop_length=hop_length,
            lookahead_frames=lookahead_frames,
        )
    
    def forward(
        self,
        audio: torch.Tensor,      # (B, T_samples) — normalized [-1, 1]
        mel: torch.Tensor,        # (B, n_mels, T_frames) — log-mel, norm [-1, 1]
        return_params: bool = False,
    ) -> torch.Tensor | tuple[torch.Tensor, dict]:
        """
        Forward pass.
        
        Args:
            audio: Input audio tensor
            mel: Mel spectrogram tensor
            return_params: If True, also return compressor parameters
        
        Returns:
            Compressed audio tensor, optionally with parameter dictionary
        """
        context_emb = self.encoder(mel)          # (B, T, d_model)
        context_emb = self.context(context_emb)  # (B, T, d_model) — temporally informed
        params = self.params(context_emb)        # dict of (B, T) envelopes
        output = self.dsp(audio, params)
        
        if return_params:
            return output, params
        return output
    
    @classmethod
    def from_config(cls, config) -> "ContextAwareCompressor":
        """Create model from configuration object."""
        return cls(
            n_mels=config.audio.n_mels,
            d_model=config.mamba.d_model,
            n_mamba_layers=config.mamba.n_layers,
            d_state=config.mamba.d_state,
            d_conv=config.mamba.d_conv,
            expand=config.mamba.expand,
            sample_rate=config.audio.sample_rate,
            hop_length=config.audio.hop_length,
            lookahead_frames=config.compressor.lookahead_frames,
            min_gain_db=config.compressor.min_gain_db,
            max_gain_db=config.compressor.max_gain_db,
        )
