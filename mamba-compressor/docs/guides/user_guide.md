# MambaCompressor user guide — research prototype

> **Status (2026-10-04):** This package contains experimental PyTorch model/training code, not a released processor. Dependency installation, end-to-end training, checkpoint inference, audio-quality results, and real-time performance have not been validated in this audit. No pretrained `from_pretrained()` API or verified inference CLI is provided. See [`../../../docs/PROJECT_STATUS.md`](../../../docs/PROJECT_STATUS.md) and [`../../README.md`](../../README.md).

## What is present

The code explores a context-model/parameter-head/differentiable-DSP structure. `scripts/train_blind.py` is the only training entry point currently confirmed in the tree; its parser accepts `--config`, `--gpus`, `--epochs`, `--batch_size`, and `--resume`. The data directory and other settings are read from the YAML config. The script has not been run end to end in this environment.

No checked-in `scripts/infer.py` or `scripts/finetune_paired.py` supports the older commands formerly shown here. Do not follow those examples or assume that a pretrained checkpoint is available.

## Research workflow (planned)

1. Establish a reproducible environment and dataset, including license and attribution checks.
2. Run the blind-pretraining script after validating its data path, dependencies, model shapes, and checkpoint behavior.
3. Add deterministic checkpoint save/load and inference tests before publishing an audio example.
4. Compare against conventional compressors and simpler learned baselines using documented objective tests and blind listening.
5. Consider paired fine-tuning and plug-in/export work only after the model and audio-quality contract is stable.

## Claims requiring evidence

There is no verified sub-5-ms latency, effective 60-second context, GPU memory figure, professional mastering quality, anticipation of musical transitions, or real-time streaming behavior. Do not treat historical benchmark tables, GPU training-time estimates, or marketing language as measurements. Publish such claims only with the revision, checkpoint, data split, hardware, measurement protocol, and evaluation artifacts.
