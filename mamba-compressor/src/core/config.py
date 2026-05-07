"""Configuration management using OmegaConf and dataclasses."""

import os
from dataclasses import dataclass, field
from typing import Optional, Tuple, List
from omegaconf import OmegaConf, DictConfig


@dataclass
class AudioConfig:
    """Audio processing parameters."""
    sample_rate: int = 44100
    n_fft: int = 2048
    hop_length: int = 512
    n_mels: int = 128
    f_min: float = 0.0
    f_max: Optional[float] = None


@dataclass
class MambaConfig:
    """Mamba SSM architecture parameters."""
    d_model: int = 256
    n_layers: int = 6
    d_state: int = 32
    d_conv: int = 4
    expand: int = 2
    dropout: float = 0.1


@dataclass
class CompressorConfig:
    """Compressor parameter ranges."""
    min_gain_db: float = -40.0
    max_gain_db: float = 0.0
    min_attack_ms: float = 0.1
    max_attack_ms: float = 100.0
    min_release_ms: float = 10.0
    max_release_ms: float = 2000.0
    lookahead_frames: int = 4


@dataclass
class LossConfig:
    """Loss function weights and parameters."""
    waveform_weight: float = 1.0
    spectral_weight: float = 1.0
    crest_factor_weight: float = 0.5
    lufs_weight: float = 0.3
    target_lufs: float = -14.0
    fft_sizes: Tuple[int, ...] = (512, 1024, 2048)


@dataclass
class TrainingConfig:
    """Training hyperparameters."""
    batch_size: int = 8
    num_workers: int = 4
    max_epochs: int = 100
    learning_rate: float = 1e-4
    weight_decay: float = 1e-5
    warmup_steps: int = 1000
    gradient_clip_val: float = 1.0
    precision: str = "16-mixed"
    accumulate_grad_batches: int = 1


@dataclass
class CheckpointConfig:
    """Checkpointing configuration."""
    dirpath: str = "checkpoints"
    save_top_k: int = 3
    monitor: str = "val_loss"
    mode: str = "min"
    every_n_epochs: int = 5


@dataclass
class Config:
    """Main configuration container."""
    audio: AudioConfig = field(default_factory=AudioConfig)
    mamba: MambaConfig = field(default_factory=MambaConfig)
    compressor: CompressorConfig = field(default_factory=CompressorConfig)
    loss: LossConfig = field(default_factory=LossConfig)
    training: TrainingConfig = field(default_factory=TrainingConfig)
    checkpoint: CheckpointConfig = field(default_factory=CheckpointConfig)
    
    # Metadata
    project_name: str = "mamba-compressor"
    version: str = "0.1.0"
    seed: int = 42
    device: str = "cuda"
    
    def to_dict(self) -> dict:
        """Convert to dictionary."""
        return OmegaConf.to_container(OmegaConf.structured(self), resolve=True)
    
    def save(self, path: str) -> None:
        """Save configuration to YAML file."""
        os.makedirs(os.path.dirname(path), exist_ok=True)
        OmegaConf.save(OmegaConf.structured(self), path)
    
    @classmethod
    def load(cls, path: str) -> "Config":
        """Load configuration from YAML file."""
        cfg = OmegaConf.load(path)
        return OmegaConf.to_object(cfg)
    
    @classmethod
    def from_dict(cls, data: dict) -> "Config":
        """Create configuration from dictionary."""
        return OmegaConf.to_object(OmegaConf.create(data))


def load_config(path: str) -> Config:
    """Load configuration from YAML file."""
    return Config.load(path)


def save_config(cfg: Config, path: str) -> None:
    """Save configuration to YAML file."""
    cfg.save(path)


def get_default_config() -> Config:
    """Get default configuration."""
    return Config()
