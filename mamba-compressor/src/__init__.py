"""
MambaCompressor: Context-Aware Dynamic Audio Processing

A professional-grade audio compressor using Mamba SSMs for temporal reasoning
and differentiable DSP for interpretable signal processing.
"""

__version__ = "0.1.0"
__author__ = "Lead Audio AI Architect"
__email__ = "audio.ai@example.com"

from src.data.blind_dataset import BlindAudioDataset, SyntheticCorruptor
from src.losses.crest_factor import CrestFactorLoss
from src.losses.lufs_loss import LUFSLoss
from src.losses.spectral_loss import MultiResolutionSTFTLoss
from src.models.context_compressor import ContextAwareCompressor
from src.models.dsp_layer import DifferentiableDSPCompressor
from src.models.mamba_tower import MambaContextTower
from src.models.mel_encoder import MelEncoder
from src.utils.audio import load_audio, mel_spectrogram, save_audio
from src.utils.metrics import (
    compute_crest_factor,
    compute_lufs,
    compute_spectral_flatness,
)

__all__ = [
    # Models
    "ContextAwareCompressor",
    "MambaContextTower",
    "DifferentiableDSPCompressor",
    "MelEncoder",
    # Losses
    "MultiResolutionSTFTLoss",
    "CrestFactorLoss",
    "LUFSLoss",
    # Data
    "SyntheticCorruptor",
    "BlindAudioDataset",
    # Utilities
    "load_audio",
    "save_audio",
    "mel_spectrogram",
    "compute_lufs",
    "compute_crest_factor",
    "compute_spectral_flatness",
]
