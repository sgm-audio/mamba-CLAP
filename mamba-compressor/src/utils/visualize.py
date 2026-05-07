"""Visualization utilities for audio and model outputs."""

import torch
import numpy as np
from typing import Optional, Union
import matplotlib.pyplot as plt


def plot_gain_envelope(
    gain_db: Union[torch.Tensor, np.ndarray],
    title: str = "Gain Reduction Envelope",
    save_path: Optional[str] = None,
    figsize: tuple = (12, 4),
) -> plt.Figure:
    """
    Plot gain reduction envelope over time.
    
    Args:
        gain_db: Gain values in dB (T,) or (B, T)
        title: Plot title
        save_path: Path to save figure (optional)
        figsize: Figure size
    
    Returns:
        Matplotlib figure
    """
    if isinstance(gain_db, torch.Tensor):
        gain_db = gain_db.detach().cpu().numpy()
    
    if gain_db.ndim == 1:
        gain_db = gain_db[np.newaxis, :]
    
    fig, ax = plt.subplots(figsize=figsize)
    
    for i, gain in enumerate(gain_db):
        ax.plot(gain, label=f"Batch {i}" if gain_db.shape[0] > 1 else "Gain")
    
    ax.set_xlabel("Frame")
    ax.set_ylabel("Gain (dB)")
    ax.set_title(title)
    ax.grid(True, alpha=0.3)
    ax.axhline(y=0, color='r', linestyle='--', alpha=0.5, label="0 dB (no gain)")
    ax.legend()
    
    plt.tight_layout()
    
    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches='tight')
    
    return fig


def plot_waveform(
    audio: Union[torch.Tensor, np.ndarray],
    sample_rate: int = 44100,
    title: str = "Waveform",
    save_path: Optional[str] = None,
    figsize: tuple = (12, 3),
) -> plt.Figure:
    """
    Plot audio waveform.
    
    Args:
        audio: Audio samples (T,) or (B, T)
        sample_rate: Sample rate in Hz
        title: Plot title
        save_path: Path to save figure (optional)
        figsize: Figure size
    
    Returns:
        Matplotlib figure
    """
    if isinstance(audio, torch.Tensor):
        audio = audio.detach().cpu().numpy()
    
    if audio.ndim == 1:
        audio = audio[np.newaxis, :]
    
    fig, ax = plt.subplots(figsize=figsize)
    
    time_axis = np.arange(audio.shape[-1]) / sample_rate
    
    for i, wav in enumerate(audio):
        ax.plot(time_axis, wav, label=f"Channel {i}" if audio.shape[0] > 1 else "Audio")
    
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Amplitude")
    ax.set_title(title)
    ax.grid(True, alpha=0.3)
    
    if audio.shape[0] > 1:
        ax.legend()
    
    plt.tight_layout()
    
    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches='tight')
    
    return fig


def plot_spectrogram(
    audio: Union[torch.Tensor, np.ndarray],
    sample_rate: int = 44100,
    n_fft: int = 2048,
    hop_length: int = 512,
    n_mels: int = 128,
    title: str = "Spectrogram",
    save_path: Optional[str] = None,
    figsize: tuple = (12, 4),
) -> plt.Figure:
    """
    Plot Mel spectrogram.
    
    Args:
        audio: Audio samples (T,) or (B, T)
        sample_rate: Sample rate in Hz
        n_fft: FFT size
        hop_length: Hop length
        n_mels: Number of Mel bands
        title: Plot title
        save_path: Path to save figure (optional)
        figsize: Figure size
    
    Returns:
        Matplotlib figure
    """
    import librosa
    
    if isinstance(audio, torch.Tensor):
        audio = audio.detach().cpu().numpy()
    
    if audio.ndim > 1:
        audio = audio.mean(axis=0)  # Average channels
    
    # Compute Mel spectrogram
    mel_spec = librosa.feature.melspectrogram(
        y=audio,
        sr=sample_rate,
        n_fft=n_fft,
        hop_length=hop_length,
        n_mels=n_mels,
    )
    log_mel = librosa.power_to_db(mel_spec, ref=np.max)
    
    fig, ax = plt.subplots(figsize=figsize)
    
    img = librosa.display.specshow(
        log_mel,
        sr=sample_rate,
        hop_length=hop_length,
        x_axis='time',
        y_axis='mel',
        ax=ax,
    )
    
    ax.set_title(title)
    plt.colorbar(img, format='%+2.0f dB', ax=ax)
    plt.tight_layout()
    
    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches='tight')
    
    return fig


def plot_comparison(
    original: Union[torch.Tensor, np.ndarray],
    processed: Union[torch.Tensor, np.ndarray],
    sample_rate: int = 44100,
    title: str = "Original vs Processed",
    save_path: Optional[str] = None,
    figsize: tuple = (14, 6),
) -> plt.Figure:
    """
    Plot side-by-side comparison of original and processed audio.
    
    Args:
        original: Original audio
        processed: Processed audio
        sample_rate: Sample rate
        title: Plot title
        save_path: Path to save figure (optional)
        figsize: Figure size
    
    Returns:
        Matplotlib figure
    """
    if isinstance(original, torch.Tensor):
        original = original.detach().cpu().numpy()
    if isinstance(processed, torch.Tensor):
        processed = processed.detach().cpu().numpy()
    
    fig, axes = plt.subplots(2, 1, figsize=figsize)
    
    # Waveforms
    time_orig = np.arange(len(original)) / sample_rate
    time_proc = np.arange(len(processed)) / sample_rate
    
    axes[0].plot(time_orig, original, 'blue', alpha=0.7, label='Original')
    axes[0].set_ylabel("Amplitude")
    axes[0].set_title(f"{title} - Waveform")
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()
    
    axes[1].plot(time_proc, processed, 'red', alpha=0.7, label='Processed')
    axes[1].set_xlabel("Time (s)")
    axes[1].set_ylabel("Amplitude")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend()
    
    plt.tight_layout()
    
    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches='tight')
    
    return fig
