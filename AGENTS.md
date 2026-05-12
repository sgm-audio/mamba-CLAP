# Mamba Based Agent Guidelines

## Essential Setup
- Run setup script: `./scripts/setup-dev.sh` (installs deps, PyTorch, mamba-ssm, dev tools)
- `./scripts/setup-dev.sh` currently installs `mamba-ssm` with `MAMBA_SKIP_CUDA_BUILD=TRUE`; use the manual install command below when you need CUDA extension builds.
- Manual setup requires CUDA-specific flags:
  ```bash
  pip install -r requirements.txt
  pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118
  MAMBA_FORCE_BUILD=TRUE pip install --no-cache-dir mamba-ssm --no-build-isolation
  pip install pytest black ruff mypy causal-conv1d>=1.4.0 --no-build-isolation
  # For DSP system: pip install ray numpy hypothesis pytest langsmith neo4j
  ```

## JUCE Installation
Since JUCE is not available in standard package repositories for Bazzite/Fedora Atomic systems, manual installation is required:

1. Clone JUCE repository:
   ```bash
   git clone https://github.com/juce-framework/JUCE.git ~/JUCE
   ```

2. Build and install Projucer:
   ```bash
   cd ~/JUCE/extras/Projucer
   mkdir -p build && cd build
   cmake .. -G "Unix Makefiles"
   make -j$(nproc)
   sudo make install
   ```

3. Add JUCE to your PATH (add to ~/.bashrc or ~/.zshrc):
   ```bash
   echo 'export JUCE_PATH="$HOME/JUCE"' >> ~/.bashrc
   echo 'export PATH="$JUCE_PATH/extras/Projucer/Builds/LinuxMakefile/build:$PATH"' >> ~/.bashrc
   source ~/.bashrc
   ```

4. Verify installation:
   ```bash
   projucer --version
   ```

## Key Commands
- Basic validation: `python test_mamba.py`
- Run specific tests:
  - All: `./scripts/run-tests.sh all`
  - Basic: `./scripts/run-tests.sh basic`
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
- Mamba Mastering Service:
  - API: `uvicorn mastering_service.main:app --host 0.0.0.0 --port 8000 --reload`
  - Worker: `celery -A mastering_service.worker.celery_app worker --loglevel=info`
  - Tests: `pytest mastering_service/tests/`
- Activate pre-commit hooks: `pre-commit install`

## Repository Structure
- `/mamba`: Core Mamba-SSM implementation
- `/mamba-compressor`: Context-aware dynamic audio processing
- `/clap-plugin`: CLAP audio model integration (C++/JUCE)
- `/MambaDAW`: Standalone C++ CLAP host/plugin (LibTorch-based)
- `/dsp_system`: Emotion-aware DSP with Ray/Neo4j
- `/JUCE_Plugin`: JUCE-based audio plugin (VST3/AU/AAX/CLAP)
- `/mastering_service`: FastAPI + Celery mastering API and async processing pipeline
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

## Code Quality
- Pre-commit hooks enforce: black (line-length=100), isort, ruff, mypy
- Python code should use type hinting
- All DSP layers must maintain clean gradient path for end-to-end optimization