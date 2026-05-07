# Mamba Repository Agent Guidelines

## Setup
- Install PyTorch first (CUDA 11.6+ required for GPU)
- Install mamba-ssm: `pip install mamba-ssm --no-build-isolation`
- Optionally install causal-conv1d: `pip install causal-conv1d>=1.4.0 --no-build-isolation` or `pip install mamba-ssm[causal-conv1d] --no-build-isolation`
- For Mamba-3: `MAMBA_FORCE_BUILD=TRUE pip install --no-cache-dir --force-reinstall git+https://github.com/state-spaces/mamba.git --no-build-isolation`
- Important build environment variables:
  - `MAMBA_SKIP_CUDA_BUILD=TRUE`: Skip CUDA compilation (CPU-only builds)
  - `MAMBA_FORCE_BUILD=TRUE`: Force rebuild from source
  - `MAMBA_FORCE_CXX11_ABI=TRUE`: Force C++11 ABI for compatibility with nvcr images

## Key Commands
- Run basic validation: `python test_mamba.py`
- Run specific test suites: navigate to mamba/tests/ and run individual test files
- Generate text: `python benchmarks/benchmark_generation_mamba_simple.py`
- Build package: `python setup.py sdist --dist-dir=dist` (set MAMBA_SKIP_CUDA_BUILD=TRUE for CPU-only builds)
- Publish: GitHub Actions workflow triggers on tag push (v*)

## Repository Structure
- `/mamba`: Core Mamba-SSM implementation (modules, ops, models)
- `/mamba-compressor`: Audio compression experiments
- `/clap-plugin`: CLAP audio model integration
- Standalone Python files: various Mamba applications (enhanced_mamba_audio.py, nemo_mamba_audio_model.py, etc.)

## Important Notes
- Requires Linux and NVIDIA GPU for full functionality
- AMD cards require additional ROCm patching (see mamba/README.md)
- Models available on Hugging Face: state-spaces organization
- For Mamba-2/3 variants, import from mamba_ssm (Mamba2, Mamba3 classes)
- CUDA builds require `--no-build-isolation` flag to use existing PyTorch installation
- Testing uses lm-evaluation-harness library (install with `pip install lm-eval==0.4.2`)
- Package name is mamba_ssm (not mamba) when importing
- Key dependencies: torch, packaging, ninja, einops, triton>=3.5.0, transformers, tilelang==0.1.8, apache-tvm-ffi<=0.1.9, quack-kernels>=0.3.4