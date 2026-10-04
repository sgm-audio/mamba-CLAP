# MambaCompressor — repository guidance

> **Status (2026-10-04):** experimental Python research code; not a release-ready processor. The training, checkpoint, inference, audio-quality, and real-time paths are not end-to-end verified. See [`../docs/PROJECT_STATUS.md`](../docs/PROJECT_STATUS.md) and [`README.md`](README.md).

## Verified repository shape

- `src/models/context_compressor.py`: model composition; `ContextAwareCompressor.from_config()` exists.
- `src/models/mamba_tower.py` and `src/models/dsp_layer.py`: experimental sequence-model and differentiable-DSP components.
- `scripts/train_blind.py`: only confirmed training entry point. It accepts `--config`, `--gpus`, `--epochs`, `--batch_size`, and `--resume`; data settings come from YAML. The script has not been run end to end in the audit environment.
- No verified pretrained checkpoint loader, `scripts/infer.py`, or paired fine-tuning script is present. The model forward path requires audio and mel inputs; do not add examples that assume `from_pretrained()`.

## Development guidance

- Keep model and signal-processing changes testable and record tensor shapes, sample rate, and configuration.
- Do not label the custom context tower as upstream Mamba-3 unless the exact upstream implementation is used and verified.
- Keep weight/checkpoint I/O out of any proposed audio callback; no current integration is certified for real-time use.
- Treat CUDA/Triton optimization, streaming inference, multiband processing, paired fine-tuning, and evaluation tooling as future research tasks, not delivered features.
- Before making quality/performance claims, add reproducible training/inference fixtures, baselines, objective metrics, and blind listening results.
