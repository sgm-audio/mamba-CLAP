# MambaCompressor: Context-Aware Dynamic Audio Processing

A professional-grade, context-aware dynamic audio processor built on **Mamba Selective State Space Models (SSMs)** for linear-time temporal reasoning and **Differentiable DSP** for interpretable, phase-coherent signal processing.

## 🏗️ Architecture Overview

The system decomposes audio compression into four stages:
1.  **Mel Encoder**: Maps log-Mel spectrograms `(B, n_mels, T)` to semantic embeddings `(B, T, d_model)`.
2.  **Mamba Context Tower**: Stack of Mamba SSM layers that maintain a hidden state encoding musical context across long time scales (up to 60s+).
3.  **Parameter Head**: Emits time-varying compressor envelopes (`gain_db`, `attack_ms`, `release_ms`).
4.  **Differentiable DSP**: Applies IIR smoothing (ballistics) and lookahead buffering to the audio.

## 🚀 Key Commands

### Training (Blind Pre-training)
Uses synthetic corruption to train without paired data.
```bash
python scripts/train_blind.py \
  --data_dir ./datasets/music_corpus \
  --config configs/blind_pretrain.yaml \
  --gpus 0,1,2,3
```

### Inference
Example usage in Python:
```python
from src.models.context_compressor import ContextAwareCompressor
model = ContextAwareCompressor.from_config(config)
output, params = model(audio, mel, return_params=True)
```

## 🛠️ Development Conventions

### Coding Style
- **Type Hinting**: Mandatory for all new functions and methods.
- **Differentiability**: Ensure all DSP operations maintain a clean gradient path (avoid non-differentiable operations like hard clipping without alternatives).
- **Modularity**: New components should follow the existing structure in `src/models/` and `src/losses/`.

### Configuration
- Uses **Hydra (OmegaConf)** for configuration.
- Base config: `configs/blind_pretrain.yaml`.
- Do not hardcode hyperparameters; add them to the YAML and access via `config`.

### Known Bottlenecks (from SELF_ANALYSIS.md)
- **IIR Ballistics Loop**: Currently implemented in Python for-loop in `src/models/dsp_layer.py`. This is a significant bottleneck for real-time inference.
- **Streaming Inference**: Basic state management exists in `MambaContextTower.forward_with_state`, but a full streaming pipeline with ring buffers is not yet implemented.

## 📁 Key Files

- `src/models/context_compressor.py`: Main model entry point.
- `src/models/mamba_tower.py`: Mamba SSM stack for temporal context.
- `src/models/dsp_layer.py`: Differentiable DSP implementation (ballistics & lookahead).
- `src/losses/`: Perceptual loss functions (spectral, crest factor, LUFS).
- `scripts/train_blind.py`: Script for pre-training on synthetic data.
- `configs/blind_pretrain.yaml`: Main configuration for training.

## 📝 Roadmap & TODOs

- [ ] **CUDA Optimization**: Implement Triton/CUDA kernel for `_apply_ballistics` in `dsp_layer.py`.
- [ ] **Streaming Pipeline**: Implement `StreamingInference` class for real-time chunk-based processing.
- [ ] **Multiband Compression**: Extend architecture to support multiple frequency bands.
- [ ] **Paired Fine-tuning**: Implement `scripts/finetune_paired.py` for training on dry/wet datasets.
- [ ] **Evaluation Suite**: Add `scripts/evaluate.py` with perceptual metrics (LRA, True Peak, etc.).
