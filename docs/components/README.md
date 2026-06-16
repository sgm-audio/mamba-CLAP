# Component Interaction Documentation

## Overview

The Mamba repository is organized as a monorepo with several distinct but interconnected components:

1. **Core Research** (`/mamba`) - The foundational Mamba-SSM implementation
2. **Applications** (Standalone Python files) - End-user applications and examples
3. **Plugins** (`/clap-plugin`, `/MambaDAW`) - Audio plugin implementations
4. **Experimental** (`/mamba-compressor`, `/dsp_system`) - Research prototypes

## Component Relationships

### Core Mamba (`/mamba`)
- Provides the foundational Mamba-SSM implementation published as the `mamba_ssm` Python package
- Contains modules, ops, and model implementations for Mamba-1, Mamba-2, and Mamba-3
- Used by all other components that require sequence modeling capabilities

### Applications (Standalone Python Files)
- Direct consumers of the `mamba_ssm` package
- Examples:
  - `enhanced_mamba_audio.py` - Audio processing application using Mamba for spectrogram enhancement
  - `nemo_mamba_audio_model.py* - Nemo-integrated Mamba audio model
  - `mamba_spectrogram_predictor.py` - Spectrogram prediction model

### CLAP Plugin (`/clap-plugin`)
- C++/JUCE implementation for real-time audio processing in DAWs
- Integrates with the core Mamba-SSM models for audio processing
- Uses LibTorch for inference in real-time contexts

### MambaDAW (`/MambaDAW`)
- Standalone C++ CLAP host and plugin architecture
- Hybrid architecture: Real-time C++ host with LibTorch for inference + Python-based training
- Provides a complete audio workstation environment built around Mamba models

### Experimental Components
- **Mamba Compressor** (`/mamba-compressor`): Context-aware dynamic processor replacing traditional heuristic rules with learned parameters
- **DSP System** (`/dsp_system`): Emotion-aware audio DSP system using Ray actors for distributed processing

## Data Flow and Integration

1. **Research → Applications**: Core Mamba innovations from `/mamba` are applied in standalone Python applications
2. **Research → Plugins**: Optimized Mamba models are deployed in `/clap-plugin` and `/MambaDAW` for real-time audio processing
3. **Research → Experimental**: Novel applications of Mamba techniques explored in `/mamba-compressor` and `/dsp_system`

## Shared Dependencies

All Python components depend on:
- `torch` (PyTorch)
- `mamba_ssm` (the core Mamba package built from `/mamba`)
- Various audio processing and utility libraries

C++ components depend on:
- LibTorch for PyTorch model inference
- JUCE framework for audio plugin development (`/clap-plugin`)
- CLAP specification compliance for DAW integration

## Extension Points

1. Adding new Mamba variants: Modify `/mamba` and export via `mamba_ssm`
2. New audio applications: Create standalone Python files importing from `mamba_ssm`
3. New plugin targets: Extend `/clap-plugin` or `/MambaDAW` with additional functionality
4. Experimental research: Utilize `/mamba-compressor` and `/dsp_system` for novel approaches
