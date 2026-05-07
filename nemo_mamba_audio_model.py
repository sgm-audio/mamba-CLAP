try:
    from pytorch_lightning import Trainer
    from nemo.core.classes import ModelPT
    from nemo.core.neural_types import NeuralType, MelSpectrogramType
    from omegaconf import DictConfig
    NEMO_AVAILABLE = True
except ImportError:
    NEMO_AVAILABLE = False

import torch

from mamba_ssm.modules import MambaSpectrogramPredictor

if NEMO_AVAILABLE:
    class NeMoMambaAudioModel(ModelPT):
        def __init__(self, cfg: DictConfig, trainer: Trainer = None):
            super().__init__(cfg=cfg, trainer=trainer)

            self.model = MambaSpectrogramPredictor(
                n_mels=cfg.n_mels,
                d_model=cfg.d_model,
                n_layer=cfg.n_layer
            )

            self.loss_fn = torch.nn.L1Loss()

        def forward(self, input_signal, input_signal_length):
            return self.model(input_signal)

        def training_step(self, batch, batch_idx):
            audio_features, targets, _ = batch
            predictions = self(audio_features)

            loss = self.loss_fn(predictions, targets)

            self.log("train_loss", loss, sync_dist=True)
            return loss

        def setup_training_data(self, train_data_config):
            raise NotImplementedError("setup_training_data must be implemented by subclass or configured via NeMo")

        def setup_validation_data(self, val_data_config):
            raise NotImplementedError("setup_validation_data must be implemented by subclass or configured via NeMo")


if __name__ == "__main__" and NEMO_AVAILABLE:
    from omegaconf import OmegaConf

    cfg = OmegaConf.create({
        "n_mels": 80,
        "d_model": 512,
        "n_layer": 8
    })

    trainer = None

    try:
        model = NeMoMambaAudioModel(cfg=cfg, trainer=trainer)
        print("NeMoMambaAudioModel instantiated successfully!")

        batch_size = 4
        mels = 80
        time_steps = 100
        dummy_input = torch.randn(batch_size, mels, time_steps)
        dummy_lengths = torch.full((batch_size,), time_steps, dtype=torch.long)

        output = model(dummy_input, dummy_lengths)
        print(f"Input shape: {dummy_input.shape}")
        print(f"Output shape: {output.shape}")
    except Exception as e:
        print(f"Error during NeMoMambaAudioModel test: {e}")
elif __name__ == "__main__":
    print("NeMo is not available. Skipping NeMoMambaAudioModel test.")
    print("To test the core MambaSpectrogramPredictor, run: python mamba_spectrogram_predictor.py")