# Mamba Based

**Context-Aware Audio Processing & Selective State Space Model (SSM) Research**

[![GitHub license](https://img.shields.io/github/license/SGM-Studios/mamba-CLAP)](https://github.com/SGM-Studios/mamba-CLAP/blob/master/LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/SGM-Studios/mamba-CLAP)](https://github.com/SGM-Studios/mamba-CLAP/stargazers)
[![Mamba Generation](https://img.shields.io/badge/SSM-Mamba--3-brightgreen)](https://github.com/state-spaces/mamba)

`Mamba Based` is a multi-layered ecosystem dedicated to researching and applying **Mamba State Space Models (SSMs)** to professional audio processing. It bridges the gap between state-of-the-art sequence modeling and high-performance audio engineering, enabling "musical intelligence" that can reason over long-term temporal contexts (60s+).

---

## 🏗️ Ecosystem Architecture

The workspace is organized into several distinct but interconnected components:

### 🐍 [Core Mamba Research](./mamba)
Foundational implementation of Mamba-1, Mamba-2 (SSD), and Mamba-3. Includes high-performance CUDA kernels and comprehensive benchmarks for long-context modeling.

### 🎚️ [Mamba Compressor](./mamba-compressor)
A specialized application focusing on **Context-Aware Dynamic Audio Processing**. It replaces heuristic compression rules with learned musical intelligence that "hears" long-term context.

### 🎹 [CLAP & JUCE Plugin](./clap-plugin)
C++/JUCE implementation of the **CLAP (Clever Audio Plugin)** standard. Features a "Neural HUD" UI built with OpenGL acceleration for real-time inference within DAWs.

### 🏛️ [MambaDAW](./MambaDAW)
A standalone C++ project implementing a high-performance CLAP host and plugin architecture using **LibTorch** for low-latency inference.

### 🧠 [Emotion-Aware DSP](./dsp_system)
A Ray-based system for emotion-aware audio processing. Includes LLM mediation for architectural friction resolution and Neo4j-based agent coordination mapping.

### 📂 [Documentation](./docs)
Unified repository documentation, including API references, component guides, and release processes.

---

## 🚀 Getting Started

### 1. Quick Setup (Recommended)
Use the provided automation scripts to set up your development environment:

```bash
# Clone and enter repo
git clone https://github.com/SGM-Studios/mamba-CLAP.git
cd mamba-CLAP

# Run setup script (installs dependencies and configures environment)
./scripts/setup-dev.sh
```

### 2. Manual Installation
If you prefer manual setup:

```bash
pip install -r requirements.txt

# Install Mamba-SSM (requires CUDA for GPU acceleration)
MAMBA_FORCE_BUILD=TRUE pip install --no-cache-dir mamba-ssm --no-build-isolation
```

---

## ✨ Key Features

- **Linear-Time Temporal Reasoning**: Unlike Transformers ($O(N^2)$), Mamba's $O(N)$ complexity enables real-time processing of long audio sequences.
- **Neural HUD Aesthetic**: A "Silicon Serpent" visual theme prioritizing high-contrast data visualization and GPU-accelerated spectrograms.
- **Differentiable Audio Engine**: End-to-end optimizable DSP layers that preserve phase coherence and transient integrity.
- **Emotionally Intelligent DSP**: Components designed for introspectability and emotional coherence, moving beyond "black box" processing.
- **Security & Performance**: Hardened audio threads and optimized IIR ballistics with `torch.compile` and future Triton support.

---

## 🏛️ State-Centric Architecture

The CLAP plugin implements a **state-centric design** where Mamba's hidden state (`h_t`) lives entirely on the real-time audio thread:

| Rule | Why | How |
|------|-----|-----|
| **No heap allocation in RT** | Prevents non-deterministic glitches | Buffers pre-allocated in `activate()` |
| **No background threads** | Synchronization overhead > inference time | `processBlock()` calls `processSample()` directly |
| **State persists** across callbacks | `h_t` is the memory; reset only between songs | `resetState()` called in `activate()`/`deactivate()` |
| **Quantization mandatory** | FP32 CPU > 200 µs/frame (fails real-time) | INT8/INT4 quantized models via `-DENABLE_QUANTIZATION=ON` |

**Why this matters**: In a traditional plugin, processing each block independently loses context. Mamba's state carries 60+ seconds of musical memory across callbacks, enabling compression decisions that "hear" what came before.

---

## 🛠️ Development & Automation

The project includes a suite of scripts in the [`scripts/`](./scripts) directory for common workflows:

- **Build All**: `./scripts/build-all.sh`
- **Run Tests**: `./scripts/run-tests.sh [all|basic|mamba|dsp]`
- **Linting**: Pre-commit hooks are configured via `.pre-commit-config.yaml`.

For detailed agent instructions and engineering standards, see [`GEMINI.md`](./GEMINI.md).
For setup and build specifics, see [`AGENTS.md`](./AGENTS.md).

---

## 📄 License

This project is licensed under the MIT License - see the individual `LICENSE` files in the subdirectories for details.

---

*"Hear the last 60 seconds of musical context and decide right now whether to clamp, breathe, or get out of the way."*
