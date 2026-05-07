# mamba-CLAP

**Professional Audio Processing with Selective State Space Models (SSMs)**

[![GitHub license](https://img.shields.io/github/license/SGM-Studios/mamba-CLAP)](https://github.com/SGM-Studios/mamba-CLAP/blob/master/LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/SGM-Studios/mamba-CLAP)](https://github.com/SGM-Studios/mamba-CLAP/stargazers)
[![Mamba Generation](https://img.shields.io/badge/SSM-Mamba--3-brightgreen)](https://github.com/state-spaces/mamba)

`mamba-CLAP` is a high-performance audio processing ecosystem that leverages **Mamba Selective State Space Models** for linear-time temporal reasoning and **Differentiable DSP** for professional-grade signal processing. It bridges the gap between state-of-the-art sequence modeling and low-latency audio applications.

---

## 🏗️ Ecosystem Architecture

The project is structured as a unified monorepo containing research, applications, and deployment targets:

### 🐍 [Core Mamba Research](./mamba)
Foundational implementation of Mamba-1, Mamba-2 (SSD), and Mamba-3. Includes high-performance CUDA kernels for selective scanning and comprehensive text/sequence generation benchmarks.

### 🎚️ [Mamba Compressor](./mamba-compressor)
A context-aware dynamic processor that replaces traditional heuristic rules with "musical intelligence."
- **Context Depth**: Linear scaling allows for 60s+ of musical context with minimal memory overhead.
- **Interpretable Parameters**: The model predicts time-varying `gain_db`, `attack_ms`, and `release_ms` for a differentiable IIR ballistic filter.

### 🎹 [CLAP & JUCE Plugin](./clap-plugin)
Professional C++/JUCE implementation for real-time inference within Digital Audio Workstations (DAWs).
- **Standard**: Fully compliant with the [CLAP (Clever Audio Plugin)](https://cleveraudio.org/) standard.
- **Interface**: A high-fidelity "Neural HUD" UI built with OpenGL acceleration and custom LookAndFeel.

### 🏛️ [MambaDAW](./MambaDAW)
A standalone C++ project implementing a high-performance CLAP host and plugin architecture.
- **Hybrid Architecture**: Real-time C++ host using **LibTorch** for low-latency inference, coupled with Python-based training.
- **Build System**: Integrated CMake build system for cross-platform deployment.

---

## 🚀 Getting Started

### 1. Python Environment (Research & Training)
Ideal for developers training new models or running offline audio enhancement.

**Prerequisites**: Python 3.10+, NVIDIA GPU (recommended).

```bash
# Clone and enter repo
git clone https://github.com/SGM-Studios/mamba-CLAP.git
cd mamba-CLAP

# Install dependencies
pip install -r requirements.txt

# Install Mamba-SSM (requires CUDA for GPU acceleration)
MAMBA_FORCE_BUILD=TRUE pip install --no-cache-dir mamba-ssm --no-build-isolation
```

**Usage Example**:
```python
from enhanced_mamba_audio import AudioProcessor
proc = AudioProcessor(sample_rate=44100)
# Refer to scripts/train_blind.py for full training workflows
```

### 2. C++ Build (Real-Time Plugin)
Ideal for music producers wanting to use Mamba in a DAW (Ableton, FL Studio, Bitwig).

**Prerequisites**: CMake, [LibTorch](https://pytorch.org/get-started/locally/), [CLAP SDK](https://github.com/free-audio/clap).

```bash
cd MambaDAW
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/libtorch
cmake --build . --config Release
```

---

## ✨ Key Features

- **Linear-Time Temporal Reasoning**: Unlike Transformers ($O(N^2)$), Mamba's $O(N)$ complexity enables real-time processing of long audio sequences.
- **Neural HUD Aesthetic**: A "Silicon Serpent" visual theme prioritizing high-contrast data visualization and GPU-accelerated spectrograms.
- **Differentiable Audio Engine**: End-to-end optimizable DSP layers that preserve phase coherence and transient integrity.
- **Multi-Generation Support**: Native support for Mamba-3's inference-first architecture and MIMO modes.

---

## 🚀 Quick Start

### Installation

1.  **Clone the Repository**:
    ```bash
    git clone https://github.com/SGM-Studios/mamba-CLAP.git
    cd mamba-CLAP
    ```

2.  **Environment Setup**:
    ```bash
    pip install -r requirements.txt
    # Install mamba-ssm with CUDA build
    MAMBA_FORCE_BUILD=TRUE pip install --no-cache-dir mamba-ssm --no-build-isolation
    ```

### Basic Inference (Python)

```python
from enhanced_mamba_audio import AudioProcessor
from mamba_ssm.modules.mamba3 import Mamba3

# Load and preprocess audio
proc = AudioProcessor(sample_rate=44100)
waveform, sr = proc.load_audio("input.wav")

# Run Mamba-based spectrogram enhancement
# (See enhanced_mamba_audio.py for detailed implementation)
```

---

## 🛠️ Development

- **Architecture Details**: See [`clap-plugin/juce_gui/DESIGN.md`](./clap-plugin/juce_gui/DESIGN.md) for UI/UX specifications.
- **Project Context**: See [`GEMINI.md`](./GEMINI.md) for detailed agent instructions and engineering standards.
- **History**: Check the [`CHANGELOG.md`](./CHANGELOG.md) for version updates.

---

## 🤝 Contributing

We welcome contributions from the audio engineering and deep learning communities. Please see our contribution guidelines in individual subdirectories.

## 📄 License

This project is licensed under the MIT License - see the individual `LICENSE` files in the subdirectories for details.

---

*"Hear the last 60 seconds of musical context and decide right now whether to clamp, breathe, or get out of the way."*
