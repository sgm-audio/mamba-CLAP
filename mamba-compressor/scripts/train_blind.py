#!/usr/bin/env python3
"""Blind pretraining script for MambaCompressor."""

import argparse
import torch
import pytorch_lightning as pl
from pytorch_lightning.callbacks import ModelCheckpoint, LearningRateMonitor
from pytorch_lightning.loggers import TensorBoardLogger
from omegaconf import OmegaConf

from src.models.context_compressor import ContextAwareCompressor
from src.data.blind_dataset import create_blind_dataloader, SyntheticCorruptor
from src.losses.spectral_loss import MultiResolutionSTFTLoss
from src.losses.crest_factor import CrestFactorLoss
from src.losses.lufs_loss import LUFSLoss
from src.core.config import Config, load_config
from src.core.logging import setup_logger


class MambaCompressorLitModule(pl.LightningModule):
    """PyTorch Lightning wrapper for MambaCompressor."""
    
    def __init__(self, config):
        super().__init__()
        self.save_hyperparameters(OmegaConf.to_container(config))
        self.config = config
        
        # Model
        self.model = ContextAwareCompressor.from_config(config)
        
        # Losses
        self.stft_loss = MultiResolutionSTFTLoss(
            fft_sizes=config.loss.fft_sizes
        )
        self.crest_loss = CrestFactorLoss()
        self.lufs_loss = LUFSLoss(
            target_lufs=config.loss.target_lufs,
            sample_rate=config.audio.sample_rate,
        )
        
        # Corruption for blind training
        self.corruptor = SyntheticCorruptor(
            sample_rate=config.audio.sample_rate,
            ratio_range=tuple(config.corruption.ratio_range),
            attack_range_ms=tuple(config.corruption.attack_range_ms),
            release_range_ms=tuple(config.corruption.release_range_ms),
            threshold_range_db=tuple(config.corruption.threshold_range_db),
        )
    
    def forward(self, audio, mel):
        return self.model(audio, mel)
    
    def training_step(self, batch, batch_idx):
        corrupted, clean = batch
        
        # Extract Mel spectrogram from clean audio
        mel = self._extract_mel(clean)
        
        # Model prediction: reconstruct clean from corrupted
        output, params = self.model(corrupted, mel, return_params=True)
        
        # Compute losses
        l_waveform = self.hparams.loss.waveform_weight * torch.nn.functional.l1_loss(output, clean)
        l_spectral = self.hparams.loss.spectral_weight * self.stft_loss(output, clean)
        l_crest = self.hparams.loss.crest_factor_weight * self.crest_loss(output, clean)
        l_lufs = self.hparams.loss.lufs_weight * self.lufs_loss(output)
        
        loss = l_waveform + l_spectral + l_crest + l_lufs
        
        # Log metrics
        self.log("train_loss", loss, prog_bar=True)
        self.log("train_waveform", l_waveform)
        self.log("train_spectral", l_spectral)
        self.log("train_crest", l_crest)
        self.log("train_lufs", l_lufs)
        
        # Log gain statistics
        self.log("gain_mean", params["gain_db"].mean())
        self.log("gain_std", params["gain_db"].std())
        
        return loss
    
    def validation_step(self, batch, batch_idx):
        corrupted, clean = batch
        mel = self._extract_mel(clean)
        output = self.model(corrupted, mel)
        
        l_waveform = torch.nn.functional.l1_loss(output, clean)
        l_spectral = self.stft_loss(output, clean)
        loss = l_waveform + l_spectral
        
        self.log("val_loss", loss, prog_bar=True)
        return loss
    
    def configure_optimizers(self):
        optimizer = torch.optim.AdamW(
            self.parameters(),
            lr=self.hparams.training.learning_rate,
            weight_decay=self.hparams.training.weight_decay,
        )
        
        scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(
            optimizer,
            T_max=self.hparams.training.max_epochs,
        )
        
        return {
            "optimizer": optimizer,
            "lr_scheduler": {
                "scheduler": scheduler,
                "monitor": "val_loss",
                "interval": "epoch",
            },
        }
    
    def _extract_mel(self, audio):
        """Extract Mel spectrogram from audio."""
        try:
            import torchaudio.transforms as T
            mel_transform = T.MelSpectrogram(
                sample_rate=self.hparams.audio.sample_rate,
                n_fft=self.hparams.audio.n_fft,
                hop_length=self.hparams.audio.hop_length,
                n_mels=self.hparams.audio.n_mels,
            ).to(audio.device)
            
            mel_specs = []
            for i in range(audio.shape[0]):
                mel = mel_transform(audio[i])
                mel = torch.log1p(mel)
                mel_specs.append(mel)
            
            return torch.stack(mel_specs)
        except ImportError:
            # Fallback would use librosa (slower)
            raise RuntimeError("torchaudio required for Mel extraction")


def main():
    parser = argparse.ArgumentParser(description="Train MambaCompressor (blind)")
    parser.add_argument("--config", type=str, default="configs/blind_pretrain.yaml")
    parser.add_argument("--gpus", type=str, default="0")
    parser.add_argument("--epochs", type=int, default=None)
    parser.add_argument("--batch_size", type=int, default=None)
    parser.add_argument("--resume", type=str, default=None)
    args = parser.parse_args()
    
    # Load config
    config = load_config(args.config)
    
    # Override from CLI
    if args.epochs:
        config.training.max_epochs = args.epochs
    if args.batch_size:
        config.training.batch_size = args.batch_size
    
    # Setup logging
    logger = setup_logger(__name__, log_file=f"{config.checkpoint.dirpath}/train.log")
    logger.info(f"Starting blind pretraining...")
    logger.info(f"Config: {OmegaConf.to_yaml(config)}")
    
    # Set seed
    pl.seed_everything(config.seed)
    
    # DataLoaders
    train_loader = create_blind_dataloader(
        audio_dir=config.data.audio_dir,
        batch_size=config.training.batch_size,
        num_workers=config.training.num_workers,
        duration_sec=config.data.duration_sec,
        sample_rate=config.audio.sample_rate,
        shuffle=config.data.shuffle,
    )
    
    # Model
    model = MambaCompressorLitModule(config)
    
    # Callbacks
    checkpoint_callback = ModelCheckpoint(
        dirpath=config.checkpoint.dirpath,
        filename="mamba-compressor-{epoch:02d}-{val_loss:.4f}",
        monitor=config.checkpoint.monitor,
        mode=config.checkpoint.mode,
        save_top_k=config.checkpoint.save_top_k,
        every_n_epochs=config.checkpoint.every_n_epochs,
    )
    
    lr_monitor = LearningRateMonitor(logging_interval="step")
    
    # Trainer
    trainer = pl.Trainer(
        accelerator="gpu" if torch.cuda.is_available() else "cpu",
        devices=args.gpus,
        max_epochs=config.training.max_epochs,
        precision=config.training.precision,
        gradient_clip_val=config.training.gradient_clip_val,
        accumulate_grad_batches=config.training.accumulate_grad_batches,
        callbacks=[checkpoint_callback, lr_monitor],
        logger=TensorBoardLogger("logs/", name="mamba-compressor-blind"),
        resume_from_checkpoint=args.resume,
    )
    
    # Train
    trainer.fit(model, train_loader)
    
    logger.info("Training complete!")
    logger.info(f"Best checkpoint: {checkpoint_callback.best_model_path}")


if __name__ == "__main__":
    main()
