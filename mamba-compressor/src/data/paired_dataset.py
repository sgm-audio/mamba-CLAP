"""Paired audio dataset for supervised fine-tuning."""

import torch
from torch.utils.data import Dataset, DataLoader
from pathlib import Path
from typing import List, Union, Tuple
import soundfile as sf
import numpy as np
import random


class PairedAudioDataset(Dataset):
    """
    Dataset for supervised fine-tuning with (dry, wet) paired data.
    
    Expects directory structure:
        data_dir/
            dry/
                track1.wav
                track2.wav
                ...
            wet/
                track1.wav
                track2.wav
                ...
    
    Args:
        data_dir: Root directory containing dry/ and wet/ subdirectories
        duration_sec: Duration of audio clips in seconds
        sample_rate: Target sample rate
    """
    
    def __init__(
        self,
        data_dir: Union[str, Path],
        duration_sec: float = 10.0,
        sample_rate: int = 44100,
    ):
        super().__init__()
        self.data_dir = Path(data_dir)
        self.duration_sec = duration_sec
        self.sample_rate = sample_rate
        
        self.dry_dir = self.data_dir / "dry"
        self.wet_dir = self.data_dir / "wet"
        
        if not self.dry_dir.exists():
            raise ValueError(f"Dry directory not found: {self.dry_dir}")
        if not self.wet_dir.exists():
            raise ValueError(f"Wet directory not found: {self.wet_dir}")
        
        # Get list of matching pairs
        self.dry_files = sorted(list(self.dry_dir.glob("*.wav")))
        self.wet_files = []
        
        for dry_file in self.dry_files:
            wet_file = self.wet_dir / dry_file.name
            if wet_file.exists():
                self.wet_files.append(wet_file)
            else:
                print(f"Warning: No matching wet file for {dry_file.name}")
        
        if not self.wet_files:
            raise ValueError("No matching dry/wet pairs found")
        
        self.n_samples = int(duration_sec * sample_rate)
    
    def __len__(self) -> int:
        return len(self.dry_files)
    
    def _load_audio(self, file_path: Path) -> np.ndarray:
        """Load and preprocess audio file."""
        audio, sr = sf.read(file_path, dtype=np.float32)
        
        # Convert to mono if stereo
        if audio.ndim > 1:
            audio = audio.mean(axis=1)
        
        # Random crop or pad
        if len(audio) < self.n_samples:
            audio = np.pad(audio, (0, self.n_samples - len(audio)))
        else:
            start = random.randint(0, len(audio) - self.n_samples)
            audio = audio[start:start + self.n_samples]
        
        return audio
    
    def __getitem__(self, idx: int) -> Tuple[torch.Tensor, torch.Tensor]:
        """Load paired (dry, wet) audio."""
        dry_file = self.dry_files[idx]
        wet_file = self.wet_files[idx]
        
        # Load both files
        dry_audio = self._load_audio(dry_file)
        wet_audio = self._load_audio(wet_file)
        
        # Convert to tensors
        dry_tensor = torch.from_numpy(dry_audio)
        wet_tensor = torch.from_numpy(wet_audio)
        
        # Normalize together to preserve relative levels
        max_val = max(dry_tensor.abs().max(), wet_tensor.abs().max())
        dry_tensor = dry_tensor / (max_val + 1e-8)
        wet_tensor = wet_tensor / (max_val + 1e-8)
        
        return dry_tensor, wet_tensor


def create_paired_dataloader(
    data_dir: Union[str, Path],
    batch_size: int = 8,
    num_workers: int = 4,
    duration_sec: float = 10.0,
    sample_rate: int = 44100,
    shuffle: bool = True,
) -> DataLoader:
    """
    Create a DataLoader for paired fine-tuning.
    
    Args:
        data_dir: Directory containing dry/ and wet/ subdirectories
        batch_size: Batch size
        num_workers: Number of data loading workers
        duration_sec: Duration of audio clips
        sample_rate: Sample rate
        shuffle: Whether to shuffle dataset
    
    Returns:
        Configured DataLoader
    """
    dataset = PairedAudioDataset(
        data_dir=data_dir,
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
