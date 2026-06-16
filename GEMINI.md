# Mamba Based: Context-Aware Audio Processing & SSM Research

"Mamba Based" is a multi-layered ecosystem dedicated to researching and applying **Mamba State Space Models (SSMs)** to professional audio processing. It integrates cutting-edge sequence modeling with high-performance audio engineering, bridging the gap between deep learning research and real-time DSP applications.

## 🏗️ Project Architecture

The workspace is organized into several distinct but interconnected components:

### 1. Core Research & Implementation (`/mamba`)
This directory contains the foundational Mamba SSM implementation, supporting multiple generations of the architecture:
- **Mamba-1**: The original selective state space model.
- **Mamba-2**: Introduced State Space Duality (SSD) and improved efficiency.
- **Mamba-3**: The latest inference-first state space model optimized for long-context modeling.
- **Key Modules**: `mamba_ssm` package, high-performance CUDA kernels (`csrc/`), and comprehensive benchmarks.

### 2. Mamba Compressor (`/mamba-compressor`)
A specialized application focusing on **Context-Aware Dynamic Audio Processing**.
- **Purpose**: Replaces heuristic compression rules with learned musical intelligence that "hears" long-term context (up to 60s+).
- **Architecture**: Mel Encoder → Mamba Context Tower → Parameter Head → Differentiable DSP.
- **Key Files**: `src/models/context_compressor.py`, `scripts/train_blind.py`.

### 3. CLAP Plugin Integration (`/clap-plugin`)
C++ implementation of the **CLAP (Clever Audio Plugin)** standard.
- **Goal**: Enable real-time, low-latency inference of Mamba-based audio models within Digital Audio Workstations (DAWs).
- **Current Status**: Includes `MambaClapPlugin` for spectrogram enhancement using a hybrid Mamba + HiFi-GAN approach.

### 4. Root-Level Audio Utilities
- **`enhanced_mamba_audio.py`**: PyTorch implementations of `MambaSpectrogramPredictor`, multi-resolution STFT losses, and differentiable audio processors.
- **`nemo_mamba_audio_model.py`**: Integration with NVIDIA NeMo for distributed training.

## 🚀 Key Commands & Workflows

### Environment Setup
1.  **PyTorch**: Ensure PyTorch 2.0+ with CUDA 11.8+ is installed.
2.  **Mamba SSM**:
    ```bash
    pip install mamba-ssm --no-build-isolation
    # For Mamba-3 (force build from source)
    MAMBA_FORCE_BUILD=TRUE pip install --no-cache-dir --force-reinstall git+https://github.com/state-spaces/mamba.git --no-build-isolation
    ```

### Training (Mamba Compressor)
Pre-train the compressor on unlabelled music data using synthetic dynamic corruption:
```bash
python mamba-compressor/scripts/train_blind.py \
  --data_dir ./datasets/music_corpus \
  --config mamba-compressor/configs/blind_pretrain.yaml
```

### Validation & Testing
- Run core SSM tests: `python test_mamba.py`
- Run module-specific tests: `pytest mamba/tests/modules/`

## 🛠️ Development Conventions

- **Hybrid Implementation**: Use Python for research, training, and orchestration; use C++/CUDA for high-performance kernels and real-time plugin components.
- **Differentiability**: All DSP layers (e.g., `dsp_layer.py`) must maintain a clean gradient path for end-to-end optimization.
- **Context Preservation**: Leverage Mamba's linear scaling to handle long-term musical dependencies (60s+) which are typically inaccessible to Transformers in real-time.
- **Type Safety**: Python code should use type hinting (checked via `mypy` or similar).

## 📁 Key Repository Files

- `AGENTS.md`: Specific setup and build instructions for AI agents.
- `enhanced_mamba_audio.py`: Core audio processing and loss functions.
- `requirements.txt`: Project dependencies (managed via `pip`).
- `mamba/`: Sub-repository for core SSM development.
- `mamba-compressor/`: Main audio processing application.

## 📝 Roadmap & Future Work

- [ ] **Triton/CUDA Optimization**: Refactor `_apply_ballistics` in `dsp_layer.py` into a high-performance kernel.
- [ ] **Real-Time Streaming**: Implement a robust `StreamingInference` pipeline with ring-buffer management.
- [ ] **Multiband Expansion**: Extend the compressor architecture to support frequency-dependent gain reduction.
- [ ] **CLAP Build System**: Finalize the CMake/Makefile build system for the C++ plugin.
