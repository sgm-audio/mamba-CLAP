# MambaDAW Implementation Summary

## Overview
This implementation provides a complete skeleton for a CLAP audio plugin named "MambaDAW" designed for high-performance AI audio processing using the Mamba State Space Model (SSM) architecture. The plugin implements a hybrid architecture with C++ real-time processing and Python model training.

## Directory Structure
```
MambaDAW/
├── src/                  # C++ source files
│   ├── audio_frontend.h/cpp   # STFT/iSTFT and feature extraction
│   ├── mamba_layer.h/cpp      # Mamba SSM layer implementation
│   ├── mamba_model.h/cpp      # Stacked Mamba model with residuals
│   ├── model_loader.h/cpp     # TorchScript model loading utility
│   ├── plugin_processor.h/cpp # Audio processing engine
│   ├── plugin_wrapper.cpp     # CLAP plugin wrapper
│   └── main.cpp               # Plugin entry point
├── include/              # Header files (currently empty)
├── python_training/      # Python training scripts
│   └── train_mamba.py    # Model training script
├── build/                # Build directory (generated)
├── resources/            # Resource directory for models/presets
├── CMakeLists.txt        # CMake build configuration
├── requirements.txt      # Python dependencies
└── README.md             # Project documentation
```

## Key Components

### 1. Audio Frontend (`audio_frontend.h/cpp`)
- Implements STFT with Hann window and configurable FFT/hop sizes
- Log-Mel spectrogram conversion with approximate Mel filterbank
- Running mean/variance normalization for features
- Inverse STFT with overlap-add for audio reconstruction
- Real-time safe design with pre-allocated buffers
- Uses input phase for reconstruction (simplified approach)

### 2. Mamba Architecture (`mamba_layer.h/cpp`, `mamba_model.h/cpp`)
- `MambaLayer`: Implements a simplified Mamba block with:
  - Input/output linear projections
  - Depthwise 1D convolution
  - SSM parameters (A, B, C, D)
  - Step-wise state recurrence (simplified scan)
- `MambaModel`: Stacks multiple Mamba layers with:
  - Layer normalization (input/output)
  - Residual connections
  - State management across layers

### 3. Model Loading (`model_loader.h/cpp`)
- Singleton utility for loading TorchScript models
- Thread-safe implementation with mutex
- Error handling and state reset capabilities

### 4. Processing Engine (`plugin_processor.h/cpp`)
- Integrates audio frontend and Mamba model
- Block-based synchronous processing
- Parameter smoothing for dry/wet mix to avoid zipper noise
- Bypass logic when model not loaded or errors occur
- State preservation between audio blocks

### 5. CLAP Wrapper (`plugin_wrapper.cpp`)
- Implements CLAP plugin entry and factory interfaces
- Connects processing engine to CLAP callbacks
- Defines parameters: Dry/Wet Mix, Model Type, Bypass
- Implements state persistence via CLAP stream API
- Uses CLAP host logging for error reporting
- Supports mono/stereo configurations

### 6. Python Training (`python_training/train_mamba.py`)
- Synthetic dataset generation (noisy/clean audio pairs)
- PyTorch implementation of Mamba architecture matching C++
- Audio preprocessing using librosa (STFT, Mel filterbanks)
- Training loop with MSE loss in spectrogram domain
- Model tracing and export to TorchScript (.pt file)
- Validation and audio reconstruction capabilities

## Build System (`CMakeLists.txt`)
- Finds and links CLAP SDK and LibTorch
- Sets C++17 standard with ABI compatibility flags
- Configures shared library output (.clap file)
- Handles platform-specific output directories

## Dependencies
### C++
- CLAP SDK
- LibTorch (PyTorch C++ API)

### Python (requirements.txt)
- torch
- torchaudio
- librosa
- numpy

## Known Limitations
1. **Mamba Implementation**: Uses sequential scanning instead of parallel scan, limiting context length for real-time use
2. **Audio Frontend**:
   - Approximate Mel filterbank (not perfect inversion)
   - Uses input phase for reconstruction (no Griffin-Lim iteration)
   - Simplified STFT/iSTFT using torch FFT (could be optimized)
3. **Model Export**:
   - Traced model may not support all TorchScript features
   - Fixed input shape requirements
4. **Processing**:
   - Synchronous block processing (could cause dropouts with large models)
   - Single-channel processing in current implementation
5. **Training**:
   - Synthetic dataset only (no real data loaders implemented)
   - Simplified architecture compared to State Space Models paper

## Next Steps for Optimization
1. **Mamba Optimization**:
   - Implement parallel scan CUDA kernel for faster inference
   - Optimize state recurrence with vectorized operations
2. **Audio Frontend Improvements**:
   - Replace approximate Mel filterbank with torchaudio-compatible version
   - Implement Griffin-Lim iteration for better phase recovery
   - Optimize STFT/iSTFT with FFTW or custom kernels
3. **Processing Engine**:
   - Implement asynchronous processing with worker threads
   - Add double-buffering for parameter changes
   - Support multi-channel processing properly
4. **Model Development**:
   - Implement proper Mamba-2/Mamba-3 variants from mamba_ssm
   - Add discretization with zero-order hold (more accurate than Euler)
   - Implement causal convolution properly
5. **Training Pipeline**:
   - Add real dataset loaders (LibriTTS, music stems, etc.)
   - Implement perceptual losses in addition to MSE
   - Add learning rate scheduling and mixed precision training
6. **Plugin Features**:
   - Add GUI parameters for model selection
   - Implement preset management
   - Add side-chain capabilities
   - VST3/AU versions alongside CLAP

## Usage
1. Set up environment:
   ```bash
   # Install LibTorch from https://pytorch.org/get-started/locally/
   export LIBTORCH_DIR=/path/to/libtorch
   ```
2. Build and test:
   ```bash
   ./build_and_test.sh
   ```
3. The plugin will be installed to `~/.clap/libMambaDAW.clap`

## License
[To be determined]
