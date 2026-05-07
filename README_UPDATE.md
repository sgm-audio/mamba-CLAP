# MambaFX CLAP Plugin - Implementation Summary

This document summarizes the implementation of the MambaFX CLAP audio effects plugin that integrates PyTorch and Mamba-3 for context-aware audio processing.

## Overview

The MambaFX plugin is a CLAP audio effect that uses the Mamba-3 State Space Model to provide context-aware audio effects (reverb, compression, saturation, EQ) that adapt to long-term musical context (60+ seconds).

## Key Features

1. **Context-Aware Processing**: Uses Mamba-3 SSM to maintain and evolve contextual understanding over time
2. **Multiple Effect Types**: 
   - Context-Aware Reverb
   - Intelligent Compressor
   - Adaptive Saturation
   - Dynamic EQ
3. **CLAP 1.0 Compliant**: Standard plugin interface for modern DAWs
4. **Flexible Deployment**: CPU-only implementation with clear path to GPU acceleration
5. **Real-Time Capable**: Designed for low-latency audio processing in DAWs

## Implementation Details

### Core Components

1. **MambaClapPlugin**: Main CLAP plugin interface handling host communication and audio processing callbacks
2. **STFTProcessor**: Converts between time-domain audio and mel-spectrogram domain
3. **MambaWrapper**: Encapsulates Mamba-3 model inference with state maintenance for streaming processing
4. **Effect Decoders**: Map Mamba output to effect-specific parameters

### Audio Processing Pipeline

```
Audio Input → STFT (Mel-Spectrogram) → Mamba-3 Processing → Effect Decoding → ISTFT → Audio Output
```

### State Management

The plugin maintains Mamba's internal state across audio blocks to enable true temporal context processing. State is reset on host transport seeks or when explicitly requested by the user.

### Effect Processing

Each effect type uses a dedicated decoder network (currently simple linear layers) that maps the Mamba-3 output to effect-specific parameters:
- Reverb: decay time, pre-delay, diffusion, wet/dry mix
- Compression: ratio, attack, release, threshold
- Saturation: drive, tone
- EQ: 5-band gain controls

## Building the Plugin

### Dependencies

1. **CLAP**: https://github.com/free-audio/clap
2. **LibTorch**: PyTorch C++ frontend
3. **C++17 compatible compiler**

### Build Instructions

```bash
# Set required environment variables
export CLAP_PATH=/path/to/clap/installation
export TORCH_INSTALL_PATH=/path/to/libtorch

# Build
cd clap-plugin
mkdir build && cd build
cmake ..
make
```

### Environment Variables

- `MAMBA_MODEL_PATH`: Path to the TorchScript Mamba-3 model (default: ./models/mamba3_audio_effects.pt)
- `CLAP_PATH`: Installation directory of CLAP library
- `TORCH_INSTALL_PATH`: Installation directory of LibTorch

## Model Requirements

The plugin expects a TorchScript model that accepts input of shape `[batch, seq_len, n_mels]` and returns processed features of the same shape. For stateful processing, the model should return a tuple `(output, new_state)` where `state` is a list of tensors representing the updated model state.

A model conversion script is provided at `convert_model.py` to create a placeholder model.

## Testing

A simple test program is provided at `test_plugin.cpp` that verifies basic plugin creation and initialization.

## Usage in DAWs

1. Copy the generated `.clap` file to your CLAP plugin directory (typically `~/.clap/` or `/usr/local/lib/clap/`)
2. Scan for new plugins in your DAW (Reaper recommended for initial testing)
3. Insert MambaFX on an audio track
4. Adjust parameters:
   - Effect Type: Select between reverb, compression, saturation, EQ
   - Mix: Dry/wet balance
   - Context Length: How much historical context to consider (10-120 seconds)

## Limitations and Future Work

1. **Model Quality**: Currently uses a placeholder model; real audio-effect-specific Mamba-3 models need to be trained
2. **Effect Decoders**: Uses simple linear layers; more sophisticated decoders would improve quality
3. **Latency**: Block-based processing introduces inherent latency (approximately one block size)
4. **GPU Acceleration**: Currently CPU-only; CUDA implementation planned for future versions

## License

This implementation is part of the Mamba Based repository and is subject to the repository's licensing terms.