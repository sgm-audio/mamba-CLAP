import torch
import torch.nn as nn
import torch.nn.functional as F
import torchaudio
import numpy as np
from typing import Tuple, Optional

from mamba_ssm.modules import MambaSpectrogramPredictor


def stft_magnitude(y: torch.Tensor,
                   n_fft: int = 1024,
                   hop_length: int = 256,
                   win_length: Optional[int] = None) -> torch.Tensor:
    if win_length is None:
        win_length = n_fft

    if y.dim() == 2:
        magnitudes = []
        for i in range(y.shape[0]):
            spec = torchaudio.transforms.Spectrogram(
                n_fft=n_fft,
                hop_length=hop_length,
                win_length=win_length,
                power=1.0,
                center=True,
                pad_mode="reflect",
            )(y[i : i + 1])
            magnitudes.append(spec)
        return torch.cat(magnitudes, dim=0)
    else:
        if y.dim() == 1:
            y = y.unsqueeze(0)
        spec = torchaudio.transforms.Spectrogram(
            n_fft=n_fft,
            hop_length=hop_length,
            win_length=win_length,
            power=1.0,
            center=True,
            pad_mode="reflect",
        )(y)
        return spec


def multi_resolution_stft_loss(
    y_pred: torch.Tensor,
    y_true: torch.Tensor,
    fft_sizes: list = [1024, 2048, 512],
    hop_sizes: list = [256, 512, 128],
    win_lengths: list = [1024, 2048, 512],
    alpha: float = 0.1,
) -> torch.Tensor:
    if len(fft_sizes) != len(hop_sizes) or len(fft_sizes) != len(win_lengths):
        raise ValueError("All lists must have the same length")

    loss = 0.0
    for fft_size, hop_size, win_length in zip(fft_sizes, hop_sizes, win_lengths):
        pred_mag = stft_magnitude(
            y_pred, n_fft=fft_size, hop_length=hop_size, win_length=win_length
        )
        true_mag = stft_magnitude(
            y_true, n_fft=fft_size, hop_length=hop_size, win_length=win_length
        )

        sc_loss = torch.norm(true_mag - pred_mag, p="fro") / torch.norm(
            true_mag, p="fro"
        )

        mag_loss = F.l1_loss(pred_mag, true_mag)

        loss += sc_loss + alpha * mag_loss

    return loss / len(fft_sizes)


class AudioProcessor:
    def __init__(
        self,
        sample_rate: int = 22050,
        n_mels: int = 80,
        n_fft: int = 1024,
        hop_length: int = 256,
        win_length: Optional[int] = None,
        fmin: int = 0,
        fmax: Optional[int] = None,
    ):
        self.sample_rate = sample_rate
        self.n_mels = n_mels
        self.n_fft = n_fft
        self.hop_length = hop_length
        self.win_length = win_length if win_length is not None else n_fft
        self.fmin = fmin
        self.fmax = fmax if fmax is not None else sample_rate // 2

        self.resampler = torchaudio.transforms.Resample(
            orig_freq=sample_rate, new_freq=sample_rate
        )
        self.mel_spectrogram = torchaudio.transforms.MelSpectrogram(
            sample_rate=sample_rate,
            n_fft=n_fft,
            hop_length=hop_length,
            win_length=self.win_length,
            f_min=fmin,
            f_max=fmax,
            n_mels=n_mels,
            power=2.0,
        )
        self.amplitude_to_db = torchaudio.transforms.AmplitudeToDB(top_db=80.0)

    def load_audio(self, file_path: str) -> Tuple[torch.Tensor, int]:
        waveform, sr = torchaudio.load(file_path)
        return waveform, sr

    def preprocess_audio(self, waveform: torch.Tensor, sr: int) -> torch.Tensor:
        if waveform.shape[0] > 1:
            waveform = torch.mean(waveform, dim=0, keepdim=True)

        if sr != self.sample_rate:
            resampler = torchaudio.transforms.Resample(
                orig_freq=sr, new_freq=self.sample_rate
            )
            waveform = resampler(waveform)

        waveform = waveform / (torch.max(torch.abs(waveform)) + 1e-8)

        return waveform

    def waveform_to_melspectrogram(self, waveform: torch.Tensor) -> torch.Tensor:
        if waveform.dim() == 2:
            waveform = waveform.unsqueeze(0)
        elif waveform.dim() == 1:
            waveform = waveform.unsqueeze(0).unsqueeze(0)

        mel_spec = self.mel_spectrogram(waveform)
        mel_spec_db = self.amplitude_to_db(mel_spec)

        mel_spec_norm = (mel_spec_db + 40) / 40

        return mel_spec_norm

    def melspectrogram_to_waveform(
        self, mel_spec: torch.Tensor, n_iter: int = 32
    ) -> torch.Tensor:
        mel_spec_db = mel_spec * 40 - 40

        mel_spec_amp = torchaudio.functions.DB_to_amplitude(
            mel_spec_db, ref=1.0, power=2.0
        )

        spec_gram = torchaudio.functions.inverse_mel_scale(
            mel_spec_amp,
            n_stft=self.n_fft // 2 + 1,
            n_mels=self.n_mels,
            sample_rate=self.sample_rate,
            f_min=self.fmin,
            f_max=self.fmax,
        )

        waveform = torchaudio.transforms.GriffinLim(
            n_fft=self.n_fft,
            hop_length=self.hop_length,
            win_length=self.win_length,
            power=2.0,
            n_iter=n_iter,
        )(spec_gram)

        return waveform


class TrainingLoss(nn.Module):
    def __init__(
        self,
        l1_weight: float = 1.0,
        stft_weight: float = 1.0,
        fft_sizes: list = [1024, 2048, 512],
        hop_sizes: list = [256, 512, 128],
        win_lengths: list = [1024, 2048, 512],
    ):
        super().__init__()
        self.l1_weight = l1_weight
        self.stft_weight = stft_weight
        self.fft_sizes = fft_sizes
        self.hop_sizes = hop_sizes
        self.win_lengths = win_lengths
        self.l1_loss = nn.L1Loss()

    def forward(
        self,
        pred_melspec: torch.Tensor,
        true_melspec: torch.Tensor,
        pred_waveform: Optional[torch.Tensor] = None,
        true_waveform: Optional[torch.Tensor] = None,
        audio_processor: Optional[AudioProcessor] = None,
    ) -> Tuple[torch.Tensor, dict]:
        l1_loss = self.l1_loss(pred_melspec, true_melspec)

        stft_loss = torch.tensor(0.0, device=pred_melspec.device)

        if (pred_waveform is not None and true_waveform is not None) or (
            audio_processor is not None
        ):
            if pred_waveform is None and audio_processor is not None:
                pred_waveform_list = []
                true_waveform_list = []

                for i in range(pred_melspec.shape[0]):
                    pred_mel = pred_melspec[i]
                    true_mel = true_melspec[i]

                    pred_wav = audio_processor.melspectrogram_to_waveform(
                        pred_mel.unsqueeze(0)
                    ).squeeze(0)
                    true_wav = audio_processor.melspectrogram_to_waveform(
                        true_mel.unsqueeze(0)
                    ).squeeze(0)

                    pred_waveform_list.append(pred_wav)
                    true_waveform_list.append(true_wav)

                pred_waveform = torch.stack(pred_waveform_list)
                true_waveform = torch.stack(true_waveform_list)

            if pred_waveform is not None and true_waveform is not None:
                stft_loss = multi_resolution_stft_loss(
                    pred_waveform,
                    true_waveform,
                    fft_sizes=self.fft_sizes,
                    hop_sizes=self.hop_sizes,
                    win_lengths=self.win_lengths,
                )

        total_loss = self.l1_weight * l1_loss + self.stft_weight * stft_loss

        loss_dict = {
            "l1_loss": l1_loss.item(),
            "stft_loss": stft_loss.item(),
            "total_loss": total_loss.item(),
        }

        return total_loss, loss_dict


if __name__ == "__main__":
    print("Testing enhanced Mamba audio model...")

    audio_processor = AudioProcessor(sample_rate=22050, n_mels=80)
    print(f"Audio processor initialized with sample_rate={audio_processor.sample_rate}")

    model = MambaSpectrogramPredictor(n_mels=80, d_model=512, n_layer=4)
    print(f"Mamba spectrogram predictor initialized")

    batch_size = 2
    channels = 1
    n_mels = 80
    time_steps = 100

    dummy_melspec = torch.randn(batch_size, channels, n_mels, time_steps)
    print(f"Input mel-spectrogram shape: {dummy_melspec.shape}")

    output = model(dummy_melspec)
    print(f"Output mel-spectrogram shape: {output.shape}")

    loss_fn = TrainingLoss(l1_weight=1.0, stft_weight=0.5)
    loss, loss_dict = loss_fn(output, dummy_melspec, audio_processor=audio_processor)
    print(f"Loss: {loss.item():.4f}")
    print(f"Loss components: {loss_dict}")

    print("All tests completed successfully!")