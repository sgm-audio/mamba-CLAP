# MambaCompressor User Guide

## Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Training](#training)
   - [Blind Pretraining](#blind-pretraining)
   - [Fine-Tuning on Paired Data](#fine-tuning-on-paired-data)
5. [Inference](#inference)
6. [Configuration](#configuration)
7. [Troubleshooting](#troubleshooting)

---

## Introduction

MambaCompressor is a context-aware dynamic audio processor that uses **Mamba Selective State Space Models (SSMs)** to make intelligent compression decisions based on musical context spanning seconds—not just instantaneous amplitude.

### Key Benefits

- **Musical Intelligence**: Learns to anticipate transients, preserve punch, and avoid pumping artifacts
- **Interpretable Outputs**: Gain envelopes are visible and overrideable
- **Real-Time Performance**: Sub-5ms latency on NVIDIA GPUs
- **No Paired Data Required**: Blind pretraining with synthetic corruption

---

## Installation

### Prerequisites

- Python 3.10+
- NVIDIA GPU with CUDA 11.8+ (recommended: RTX 3090/4090 or H100)
- 16GB+ RAM

### Step-by-Step Installation

```bash
# Clone repository
git clone https://github.com/yourusername/mamba-compressor.git
cd mamba-compressor

# Create virtual environment
python -m venv venv
source venv/bin/activate  # Linux/Mac
# or: venv\Scripts\activate  # Windows

# Install dependencies
pip install -r requirements.txt

# Verify installation
python -c "from src import ContextAwareCompressor; print('✓ Installation successful')"
```

### Optional: NVIDIA NeMo Integration

For distributed training on multiple GPUs:

```bash
pip install nemo-toolkit[all]>=1.20.0
```

---

## Quick Start

### Basic Inference

```python
import torch
from src import ContextAwareCompressor, load_audio, save_audio, mel_spectrogram

# Load pretrained model
model = ContextAwareCompressor.from_pretrained("mamba-compressor-base")
model.eval().cuda()

# Load audio
audio, sr = load_audio("input.wav", target_sr=44100)
audio = audio.cuda()

# Extract Mel spectrogram
mel = mel_spectrogram(audio, n_mels=128, hop_length=512, sample_rate=sr)

# Run inference
with torch.no_grad():
    output, params = model(audio, mel, return_params=True)

# Save result
save_audio("output.wav", output, sample_rate=sr)

# Visualize gain envelope
from src.utils.visualize import plot_gain_envelope
plot_gain_envelope(params["gain_db"], save_path="gain_envelope.png")
```

### Command-Line Inference

```bash
python scripts/infer.py \
  --checkpoint checkpoints/mamba-compressor.ckpt \
  --input input.wav \
  --output output.wav
```

---

## Training

### Blind Pretraining

Train without paired (dry/wet) data using synthetic corruption:

```bash
python scripts/train_blind.py \
  --config configs/blind_pretrain.yaml \
  --gpus 0,1,2,3 \
  --batch_size 8 \
  --epochs 100
```

#### How Blind Training Works

1. **Synthetic Corruption**: Apply deliberately bad compression (extreme ratio, wrong attack/release)
2. **Reconstruction Task**: Train model to recover clean audio from corrupted version
3. **Perceptual Losses**: Multi-resolution STFT + crest factor preservation + LUFS targeting

#### Expected Training Time

| GPUs | Dataset Size | Epochs | Time |
|------|-------------|--------|------|
| 1x RTX 4090 | 10 hours | 100 | ~12 hours |
| 4x RTX 4090 | 10 hours | 100 | ~3 hours |
| 8x H100 | 100 hours | 100 | ~6 hours |

### Fine-Tuning on Paired Data

When you have (dry, wet) pairs from professional mastering sessions:

```bash
python scripts/finetune_paired.py \
  --checkpoint checkpoints/blind_pretrain.ckpt \
  --data_dir datasets/dry_wet_pairs \
  --config configs/finetune.yaml \
  --epochs 50
```

#### Directory Structure for Paired Data

```
datasets/dry_wet_pairs/
├── dry/
│   ├── track1.wav
│   ├── track2.wav
│   └── ...
└── wet/
    ├── track1.wav  # Must match dry filename
    ├── track2.wav
    └── ...
```

---

## Inference

### Real-Time Streaming

```python
from src.utils.realtime import StreamingInference

streamer = StreamingInference(
    model_path="checkpoints/finetuned.ckpt",
    chunk_size=2048,      # ~46ms chunks
    lookahead_frames=4,   # 46ms lookahead
)

for audio_chunk in audio_stream:
    processed = streamer.process(audio_chunk)
    output_stream.write(processed)
```

### Batch Processing

```python
from torch.utils.data import DataLoader
from src.data import create_inference_dataloader

loader = create_inference_dataloader(
    audio_dir="input_tracks/",
    batch_size=4,
    num_workers=4,
)

for batch in loader:
    outputs = model(batch)
    # Save outputs...
```

---

## Configuration

### Key Parameters

#### Audio Settings (`audio:`)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `sample_rate` | 44100 | Audio sample rate (Hz) |
| `n_fft` | 2048 | FFT size for spectrograms |
| `hop_length` | 512 | Hop length (~11.6ms at 44.1kHz) |
| `n_mels` | 128 | Number of Mel bands |

#### Mamba Architecture (`mamba:`)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `d_model` | 256 | Model dimension |
| `n_layers` | 6 | Number of Mamba layers |
| `d_state` | 32 | SSM state size (↑ = more memory) |
| `d_conv` | 4 | Convolution kernel size |

#### Compressor Ranges (`compressor:`)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `min_gain_db` | -40.0 | Minimum gain reduction |
| `max_gain_db` | 0.0 | Maximum gain (no boost) |
| `lookahead_frames` | 4 | Lookahead for anticipation (~46ms) |

### Modifying Configurations

```yaml
# configs/custom.yaml
mamba:
  d_state: 64  # Double temporal memory for complex arrangements

compressor:
  lookahead_frames: 8  # More lookahead for predictive compression

loss:
  crest_factor_weight: 1.0  # Stronger dynamic range preservation
```

---

## Troubleshooting

### Common Issues

#### "CUDA out of memory"

**Solution**: Reduce batch size or sequence length:
```yaml
training:
  batch_size: 4  # Was 8
data:
  duration_sec: 5.0  # Was 10.0
```

#### "Mamba SSM not found"

**Solution**: Reinstall mamba-ssm with correct CUDA version:
```bash
pip uninstall mamba-ssm causal-conv1d
pip install mamba-ssm --no-build-isolation
```

#### Training loss not decreasing

**Possible causes**:
1. Learning rate too high → reduce to `1e-5`
2. Corruption too extreme → narrow ratio range
3. Insufficient warmup → increase `warmup_steps` to 2000

#### Output sounds distorted

**Check**:
1. Input normalization (should be [-1, 1])
2. Gain envelope clipping (visualize with `plot_gain_envelope`)
3. Sample rate mismatch between training and inference

### Getting Help

- **GitHub Issues**: https://github.com/yourusername/mamba-compressor/issues
- **Discussions**: https://github.com/yourusername/mamba-compressor/discussions
- **Email**: audio.ai@example.com

---

## Next Steps

1. **Experiment with multiband**: Extend parameter head to output sub/low/mid/high gain
2. **Add EQ module**: Differentiable parametric EQ layer
3. **Deploy as VST**: Use JUCE + libtorch for plugin wrapper
4. **Train on genre-specific data**: Classical, EDM, hip-hop each have different compression aesthetics

---

*"The model will learn to do what took you 20 years to internalize: hear the last 4 seconds of musical context and decide right now whether to clamp, breathe, or get out of the way."*
