# Mamba Repository Agent Guidelines

## Setup
- Install PyTorch first (CUDA 11.6+ required for GPU)
- Install mamba-ssm: `pip install mamba-ssm --no-build-isolation`
- For Mamba-3: `MAMBA_FORCE_BUILD=TRUE pip install --no-cache-dir --force-reinstall git+https://github.com/state-spaces/mamba.git --no-build-isolation`
- Python audio dependencies: `pip install -r requirements.txt` (only contains torch)
- For DSP system: Install Ray, NumPy, Hypothesis, Pytest, LangSmith, Neo4j

## Key Commands
- Run basic validation: `python test_mamba.py`
- Run specific test suites: navigate to mamba/tests/ and run individual test files
- Generate text: `python benchmarks/benchmark_generation_mamba_simple.py`
- Build Python package: `python setup.py sdist --dist-dir=dist` (set MAMBA_SKIP_CUDA_BUILD=TRUE for CPU-only builds)
- Build C++ plugin (MambaDAW): 
  ```bash
  cd MambaDAW
  mkdir build && cd build
  cmake .. -DCMAKE_PREFIX_PATH=/path/to/libtorch
  cmake --build . --config Release
  ```
- Development helper scripts:
  - Setup development environment: `./scripts/setup-dev.sh`
  - Run tests: `./scripts/run-tests.sh [all|basic|mamba|dsp]`
  - Build all components: `./scripts/build-all.sh`
- Publish: GitHub Actions workflow triggers on tag push (v*)

## Repository Structure
- `/mamba`: Core Mamba-SSM implementation (modules, ops, models) - published as mamba_ssm package
- `/mamba-compressor`: Audio compression experiments
- `/clap-plugin`: CLAP audio model integration (C++/JUCE)
- `/MambaDAW`: Standalone C++ CLAP host and plugin architecture
- Standalone Python files: various Mamba applications (enhanced_mamba_audio.py, nemo_mamba_audio_model.py, etc.)
- `/dsp_system`: Emotion-aware DSP system with Ray actors
- `/docs`: Unified documentation structure (API references, guides, component documentation)
- Component interaction patterns: See `/docs/components/README.md` for how modules work together

## Important Notes
- Requires Linux and NVIDIA GPU for full CUDA acceleration
- AMD cards require additional ROCm patching (see mamba/README.md)
- Models available on Hugging Face: state-spaces organization
- For Mamba-2/3 variants, import from mamba_ssm (Mamba2, Mamba3 classes)
- CUDA builds require `--no-build-isolation` flag to use existing PyTorch installation
- Testing uses lm-evaluation-harness library (install with `pip install lm-eval==0.4.2`)
- Package name is mamba_ssm (not mamba) when importing
- Key dependencies: torch, packaging, ninja, einops, triton>=3.5.0, transformers, tilelang==0.1.8, apache-tvm-ffi<=0.1.9, quack-kernels>=0.3.4
- The repo is a monorepo with multiple distinct components:
  - Research (/mamba)
  - Applications (standalone Python files)
  - Plugins (/clap-plugin, /MambaDAW)
  - Experimental (/mamba-compressor, /dsp_system)
- When importing Mamba in Python code, use `from mamba_ssm import Mamba` (or Mamba2/Mamba3)
- For audio processing applications, see enhanced_mamba_audio.py, nemo_mamba_audio_model.py, and examples/basic_usage.py
- Development workflow:
  - Use pre-commit hooks for code quality (`pre-commit install` to activate)
  - Run tests with `./scripts/run-tests.sh [all|basic|mamba|dsp]`
  - Setup development environment with `./scripts/setup-dev.sh`
  - Build all components with `./scripts/build-all.sh`
  - Consistent development environment available via Docker (see Dockerfile)
  - Release process documented in `/docs/guides/release-process.md`# Test commit
