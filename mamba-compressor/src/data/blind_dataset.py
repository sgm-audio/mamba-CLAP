"""Blind audio dataset with synthetic corruption for self-supervised training."""

import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import Dataset, DataLoader
import random
from pathlib import Path
from typing import Optional, Tuple, List, Union
import soundfile as sf
import numpy as np


class SyntheticCorruptor(nn.Module):
    """
    Applies deliberately misconfigured compression to clean audio.
    
    Output pair: (corrupted, clean) → supervised decompression task.
    
    This is your training signal until paired data arrives.
    
    Args:
        sample_rate: Audio sample rate in Hz (default: 44100)
        ratio_range: Compression ratio range (default: 8.0–20.0, over-compressed)
        attack_range_ms: Attack time range in ms (default: 0.1–2.0, too fast)
        release_range_ms: Release time range in ms (default: 2000–5000, too slow)
        threshold_range_db: Threshold range in dB (default: -30 to -6)
    """
    
    def __init__(
        self,
        sample_rate: int = 44100,
        ratio_range: Tuple[float, float] = (8.0, 20.0),
        attack_range_ms: Tuple[float, float] = (0.1, 2.0),
        release_range_ms: Tuple[float, float] = (2000.0, 5000.0),
        threshold_range_db: Tuple[float, float] = (-30.0, -6.0),
    ):
        super().__init__()
        self.sample_rate = sample_rate
        self.ratio_range = ratio_range
        self.attack_range_ms = attack_range_ms
        self.release_range_ms = release_range_ms
        self.threshold_range_db = threshold_range_db
    
    def _compress(
        self,
        audio: torch.Tensor,
        ratio: float,
        attack_ms: float,
        release_ms: float,
        threshold_db: float,
    ) -> torch.Tensor:
        """Apply compression with given parameters."""
        threshold_linear = 10 ** (threshold_db / 20.0)
        
        # Convert ms to smoothing coefficients
        attack_coeff = torch.exp(torch.tensor(-1.0 / (attack_ms * self.sample_rate / 1000.0)))
        release_coeff = torch.exp(torch.tensor(-1.0 / (release_ms * self.sample_rate / 1000.0)))
        
        gain_env = torch.ones_like(audio)
        prev_gain = torch.tensor(1.0, device=audio.device)
        
        for t in range(len(audio)):
            level = audio[t].abs()
            
            if level > threshold_linear:
                # Above threshold: apply ratio
                excess_db = 20 * torch.log10(level / threshold_linear + 1e-8)
                target_db = excess_db / ratio
                target_gain = (threshold_linear * 10 ** (target_db / 20.0)) / (level + 1e-8)
                coeff = attack_coeff
            else:
                target_gain = torch.tensor(1.0, device=audio.device)
                coeff = release_coeff
            
            prev_gain = coeff * prev_gain + (1 - coeff) * target_gain
            gain_env[t] = prev_gain
        
        return audio * gain_env
    
    def forward(self, audio: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor]:
        """
        Generate corrupted version of clean audio.
        
        Args:
            audio: Clean audio tensor (B, T_samples) or (T_samples,)
        
        Returns:
            Tuple of (corrupted, clean) tensors
        """
        # Randomize corruption params per sample
        ratio = random.uniform(*self.ratio_range)
        attack_ms = random.uniform(*self.attack_range_ms)
        release_ms = random.uniform(*self.release_range_ms)
        threshold_db = random.uniform(*self.threshold_range_db)
        
        # Handle batched input
        if audio.dim() == 1:
            corrupted = self._compress(audio.squeeze(), ratio, attack_ms, release_ms, threshold_db)
            return corrupted.unsqueeze(0), audio.unsqueeze(0)
        else:
            corrupted_list = []
            for i in range(audio.shape[0]):
                corrupted = self._compress(audio[i].squeeze(), ratio, attack_ms, release_ms, threshold_db)
                corrupted_list.append(corrupted)
            return torch.stack(corrupted_list), audio


class BlindAudioDataset(Dataset):
    """
    Dataset for blind pretraining using synthetic corruption.
    
    Loads audio files and applies on-the-fly corruption to create
    (corrupted, clean) pairs for self-supervised learning.
    
    Args:
        audio_dir: Directory containing audio files
        corruptor: SyntheticCorruptor instance
        duration_sec: Duration of audio clips in seconds (default: 10.0)
        sample_rate: Target sample rate (default: 44100)
        extensions: List of valid audio file extensions
    """
    
    def __init__(
        self,
        audio_dir: Union[str, Path],
        corruptor: SyntheticCorruptor,
        duration_sec: float = 10.0,
        sample_rate: int = 44100,
        extensions: List[str] = None,
    ):
        super().__init__()
        self.audio_dir = Path(audio_dir)
        self.corruptor = corruptor
        self.duration_sec = duration_sec
        self.sample_rate = sample_rate
        self.extensions = extensions or [".wav", ".flac", ".mp3"]
        
        # Collect all audio files
        self.audio_files = []
        for ext in self.extensions:
            self.audio_files.extend(self.audio_dir.glob(f"**/*{ext}"))
        
        if not self.audio_files:
            raise ValueError(f"No audio files found in {audio_dir}")
        
        self.n_samples = int(duration_sec * sample_rate)
    
    def __len__(self) -> int:
        return len(self.audio_files)
    
    def __getitem__(self, idx: int) -> Tuple[torch.Tensor, torch.Tensor]:
        """Load audio file and return (corrupted, clean) pair."""
        # Load audio file
        file_path = self.audio_files[idx]
        audio, sr = sf.read(file_path, dtype=np.float32)
        
        # Convert to mono if stereo
        if audio.ndim > 1:
            audio = audio.mean(axis=1)
        
        # Resample if needed (simplified - use librosa for production)
        if sr != self.sample_rate:
            # Placeholder: assume files are already at target SR
            pass
        
        # Random crop or pad
        if len(audio) < self.n_samples:
            # Pad with zeros
            audio = np.pad(audio, (0, self.n_samples - len(audio)))
        else:
            # Random crop
            start = random.randint(0, len(audio) - self.n_samples)
            audio = audio[start:start + self.n_samples]
        
        # Convert to tensor
        audio_tensor = torch.from_numpy(audio)
        
        # Normalize to [-1, 1]
        audio_tensor = audio_tensor / (audio_tensor.abs().max() + 1e-8)
        
        # Apply corruption
        corrupted, clean = self.corruptor(audio_tensor)
        
        return corrupted, clean


def create_blind_dataloader(
    audio_dir: Union[str, Path],
    batch_size: int = 8,
    num_workers: int = 4,
    duration_sec: float = 10.0,
    sample_rate: int = 44100,
    shuffle: bool = True,
) -> DataLoader:
    """
    Create a DataLoader for blind pretraining.
    
    Args:
        audio_dir: Directory containing audio files
        batch_size: Batch size
        num_workers: Number of data loading workers
        duration_sec: Duration of audio clips
        sample_rate: Sample rate
        shuffle: Whether to shuffle dataset
    
    Returns:
        Configured DataLoader
    """
    corruptor = SyntheticCorruptor(sample_rate=sample_rate)
    dataset = BlindAudioDataset(
        audio_dir=audio_dir,
        corruptor=corruptor,
        duration_sec=duration_sec,
        sample_rate=sample_rate,
    )
    
    return DataLoader(
        dataset,
        batch_size=batch_size,
        num_workers=num_workers,
        shuffle=shuffle,
        pin_memory=True,
        drop_last=True,
    )
