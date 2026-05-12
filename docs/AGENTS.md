# Mamba Repository Agent Guidelines

## Setup
- Run setup script: `./scripts/setup-dev.sh` (installs deps, PyTorch, mamba-ssm, dev tools)
- Manual setup requires CUDA-specific flags:
  ```bash
  pip install -r requirements.txt
  pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118
  MAMBA_FORCE_BUILD=TRUE pip install --no-cache-dir mamba-ssm --no-build-isolation
  pip install pytest black ruff mypy causal-conv1d>=1.4.0 --no-build-isolation
  # For DSP system: pip install ray numpy hypothesis pytest langsmith neo4j
  ```

## Key Commands
- Basic validation: `python test_mamba.py`
- Run specific tests:
  - All: `./scripts/run-tests.sh all`
  - Mamba core: `./scripts/run-tests.sh mamba` (or `cd mamba && python -m pytest tests/ -v`)
  - DSP system: `./scripts/run-tests.sh dsp` (or `cd dsp_system && python -m pytest -v`)
- Build all components: `./scripts/build-all.sh`
- Build MambaDAW CLAP plugin (critical for real-time):
  ```bash
  cd MambaDAW
  mkdir -p build && cd build
  cmake .. -DUSE_CUDA=OFF -DENABLE_QUANTIZATION=ON  # Quantization mandatory for RT
  cmake --build . --config Release -j$(nproc)
  ```
- Build JUCE-based CLAP plugin:
  ```bash
  # Uses JUCE CMake exporter
  cd JUCE_Plugin
  ./Builds/LinuxMakefile/build
  ```
- Activate pre-commit hooks: `pre-commit install`

## Repository Structure
- `/mamba`: Core Mamba-SSM implementation
- `/mamba-compressor`: Context-aware dynamic audio processing
- `/clap-plugin`: CLAP audio model integration (C++/JUCE)
- `/MambaDAW`: Standalone C++ CLAP host/plugin (LibTorch-based)
- `/dsp_system`: Emotion-aware DSP with Ray/Neo4j
- `/JUCE_Plugin`: JUCE-based audio plugin (VST3/AU/AAX/CLAP)
- `/scripts`: Automation scripts (setup-dev.sh, run-tests.sh, build-all.sh)

## Important Notes
- Import Mamba as: `from mamba_ssm import Mamba` (or Mamba2/Mamba3)
- CUDA builds require `--no-build-isolation` to use existing PyTorch
- State-centric design is critical: Mamba's hidden state (`h_t`) must remain on real-time thread
- JUCE plugins use `AudioProcessorValueTreeState` (APVTS) for robust state management
- Maintain strict audio/GUI thread isolation in JUCE components

## Critical Gotchas (Must Know)
- **Real-time audio thread**: Zero heap allocations in process() – pre-allocate in activate(), reuse buffers
- **State persistence**: Mamba state lives on RT thread – reset only between songs via activate()/deactivate()
- **Quantization required**: FP32 CPU >200µs/frame fails real-time – use `-DENABLE_QUANTIZATION=ON`
- **No background threads**: Synchronization overhead > inference time – processBlock() calls processSample() directly
- **FFTW scaling**: Forward/ISTFT divide by √n_fft; multiply by 1/sum_squared_window for unity gain
- **Thread communication**: Use lock-free SPSC ring buffers for UI→audio communication (power-of-2 capacity, trivially copyable messages)
- **JUCE development**: Use CMake build system, maintain APVTS parameter versioning for backwards compatibility