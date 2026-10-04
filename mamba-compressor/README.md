# MambaCompressor — research prototype

**Status (2026-10-04): experimental Python model/training work; not a production audio processor.** No validated pretrained checkpoint, user study, paired-quality evaluation, real-time streaming plug-in, or sub-5-ms benchmark is established here. Any 60-second-context or performance statement is a research goal until measured. See [`../docs/PROJECT_STATUS.md`](../docs/PROJECT_STATUS.md).

## Research question

Can a sequence model predict useful compressor parameters from audio context, with a differentiable DSP stage applying those parameters? The code explores a mel encoder, Mamba-style context tower, parameter head, and compressor DSP layer. Musical anticipation, professional quality, phase behavior, and effective memory duration are not established by the architecture alone.

Mamba-family methods have favorable sequence-length scaling, but that does not prove an audio deadline, quality, or useful memory duration. Measure those against conventional DSP and simpler model baselines.

## Code present

- `src/models/context_compressor.py`: PyTorch model composition.
- `src/models/mamba_tower.py`: context model.
- `src/models/dsp_layer.py`: differentiable compressor layer.
- `src/data/` and `src/losses/`: synthetic/unpaired data and training losses.
- `scripts/train_blind.py`: experimental blind-pretraining script.
- `configs/blind_pretrain.yaml`: training configuration.

No pretrained checkpoint loader (`from_pretrained`) or validated shipped model is present. `ContextAwareCompressor.from_config()` exists, but a complete trained-checkpoint inference example has not been verified. The former quick-start snippets that called `from_pretrained()` or omitted the required mel input have been removed.

## Training status

The checked-in training entry point accepts `--config`, `--gpus`, `--epochs`, `--batch_size`, and `--resume`. It reads the data directory and most settings from the YAML configuration. PyTorch execution and training have not been verified in the current audit environment. There is no checked-in `finetune_paired.py`, `evaluate.py`, or `export_onnx.py` corresponding to older README examples.

Before presenting a result as a model capability, record the checkpoint, dataset/licensing, seed/configuration, baseline, evaluation metrics, and listening protocol. No real-time plug-in integration is established by this Python package.

## Current objectives (not completed features)

- Validate the data and training pipeline on a reproducible dataset.
- Add model/checkpoint loading and deterministic inference tests.
- Compare output quality against conventional compressors and simple learned baselines.
- Measure streaming latency and state behavior, then decide whether a plug-in runtime is feasible.
- Implement paired fine-tuning and export only after the base model contract is stable.

Do not advertise “professional-grade,” “60-second musical memory,” “sub-5-ms,” “real-time,” or “production-ready” performance without the corresponding evidence.
