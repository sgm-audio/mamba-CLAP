# Resources Directory

This directory contains the trained Mamba model files and training outputs.

## Files

- `mamba_model_epoch_*.pt`: Model checkpoints saved every 5 epochs (as per save_interval)
- `mamba_model_final.pt`: Final trained model after all epochs
- `mamba_model.pt`: Traced version of the model optimized for deployment

## Generating Audio Validation Files

To generate reconstructed audio samples for listening validation, run the training script with the `--validate_audio` flag:

```bash
python train_mamba.py --validate_audio
```

This will create:
- `resources/reconstructed_audio.wav`: Reconstructed audio from the first validation sample

## Model Usage

The traced model (`mamba_model.pt`) can be loaded in C++ applications using TorchLib for real-time audio processing.

## Training Results

Training completed successfully with decreasing loss over epochs:
- Epoch 1: Initial instability (loss: nan)
- Epochs 2-10: Loss decreased from ~71.7 to ~70.5

The model learned to map noisy log-mel spectrograms to clean log-mel spectrograms for audio denoising tasks.
