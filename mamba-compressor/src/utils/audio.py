"""Audio I/O utilities."""

import torch
import numpy as np
from pathlib import Path
from typing import Union, Tuple
import soundfile as sf
import librosa


def load_audio(
    file_path: Union[str, Path],
    target_sr: int = 44100,
    mono: bool = True,
) -> Tuple[torch.Tensor, int]:
    """
    Load audio file and convert to tensor.
    
    Args:
        file_path: Path to audio file
        target_sr: Target sample rate for resampling
        mono: Convert to mono if True
    
    Returns:
        Tuple of (audio_tensor, sample_rate)
    """
    file_path = Path(file_path)
    
    # Load with librosa for resampling support
    audio, sr = librosa.load(
        str(file_path),
        sr=target_sr if target_sr else None,
        mono=mono,
        dtype=np.float32,
    )
    
    # Convert to tensor
    audio_tensor = torch.from_numpy(audio)
    
    # Ensure 2D shape (channels, samples)
    if audio_tensor.dim() == 1:
        audio_tensor = audio_tensor.unsqueeze(0)
    
    return audio_tensor, target_sr


def save_audio(
    file_path: Union[str, Path],
    audio: torch.Tensor,
    sample_rate: int = 44100,
) -> None:
    """
    Save audio tensor to file.
    
    Args:
        file_path: Output file path
        audio: Audio tensor (B, T) or (T,)
        sample_rate: Sample rate in Hz
    """
    file_path = Path(file_path)
    file_path.parent.mkdir(parents=True, exist_ok=True)
    
    # Convert to numpy
    if isinstance(audio, torch.Tensor):
        audio = audio.detach().cpu().numpy()
    
    # Squeeze batch dimension if present
    if audio.ndim == 2 and audio.shape[0] == 1:
        audio = audio.squeeze(0)
    
    # Ensure float32 in [-1, 1] range
    audio = audio.astype(np.float32)
    audio = np.clip(audio, -1.0, 1.0)
    
    # Save
    sf.write(str(file_path), audio, sample_rate)


def mel_spectrogram(
    audio: torch.Tensor,
    n_mels: int = 128,
    n_fft: int = 2048,
    hop_length: int = 512,
    sample_rate: int = 44100,
    f_min: float = 0.0,
    f_max: float = None,
    log_scale: bool = True,
) -> torch.Tensor:
    """
    Compute Mel spectrogram from audio.
    
    Args:
        audio: Audio tensor (B, T_samples) or (T_samples,)
        n_mels: Number of Mel bands
        n_fft: FFT size
        hop_length: Hop length in samples
        sample_rate: Sample rate in Hz
        f_min: Minimum frequency
        f_max: Maximum frequency (default: sr/2)
        log_scale: Apply log scaling
    
    Returns:
        Mel spectrogram tensor (B, n_mels, T_frames)
    """
    # Ensure 2D
    if audio.dim() == 1:
        audio = audio.unsqueeze(0)
    
    B = audio.shape[0]
    
    # Use torchaudio if available, otherwise librosa
    try:
        import torchaudio.transforms as T
        
        mel_transform = T.MelSpectrogram(
            sample_rate=sample_rate,
            n_fft=n_fft,
            hop_length=hop_length,
            n_mels=n_mels,
            f_min=f_min,
            f_max=f_max or sample_rate / 2,
        )
        
        mel_specs = []
        for i in range(B):
            mel = mel_transform(audio[i])
            if log_scale:
                mel = torch.log1p(mel)
            mel_specs.append(mel)
        
        mel_batch = torch.stack(mel_specs)
        
    except ImportError:
        # Fallback to librosa
        mel_specs = []
        for i in range(B):
            mel = librosa.feature.melspectrogram(
                y=audio[i].numpy(),
                sr=sample_rate,
                n_fft=n_fft,
                hop_length=hop_length,
                n_mels=n_mels,
                fmin=f_min,
                fmax=f_max,
            )
            if log_scale:
                mel = np.log1p(mel)
            mel_specs.append(torch.from_numpy(mel))
        
        mel_batch = torch.stack(mel_specs)
    
    return mel_batch
