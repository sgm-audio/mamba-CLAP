import torch
from mamba_ssm.modules import MambaSpectrogramPredictor

if __name__ == "__main__":
    batch_size = 4
    mels = 80
    time_steps = 1000

    model = MambaSpectrogramPredictor()
    if torch.cuda.is_available():
        model = model.cuda()
        dummy_audio_features = torch.randn(batch_size, mels, time_steps).cuda()
    else:
        dummy_audio_features = torch.randn(batch_size, mels, time_steps)

    output = model(dummy_audio_features)
    print(f"Input shape: {dummy_audio_features.shape}")
    print(f"Output shape: {output.shape}")