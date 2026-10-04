#!/bin/bash
# Legacy dependency bootstrap for the Mamba repository.
# Not validated against the current checkout; it installs large CUDA/Mamba
# dependencies and does not configure or verify a complete plug-in build.
# See docs/BUILD_AND_TEST_GUIDE.md before use.

set -euo pipefail

echo "WARNING: legacy, unverified setup script; dependency installation is not build/test validation."
echo "The full plug-in build is currently blocked. See docs/PROJECT_STATUS.md."
echo "Proceeding with the historical dependency install commands..."

# Check if we're in the right directory
if [ ! -f "README.md" ] || [ ! -d "mamba" ]; then
    echo "Error: This script must be run from the repository root directory"
    exit 1
fi

# Install Python dependencies
echo "Installing Python dependencies..."
pip install -r requirements.txt

# Install PyTorch (assuming CUDA 11.6+ is available)
echo "Installing PyTorch with CUDA support..."
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118

# Install mamba-ssm with CUDA support
echo "Installing mamba-ssm..."
MAMBA_FORCE_BUILD=TRUE MAMBA_SKIP_CUDA_BUILD=TRUE pip install --no-cache-dir mamba-ssm --no-build-isolation

# Install development dependencies
echo "Installing development dependencies..."
pip install pytest black ruff mypy

# Install DSP system dependencies if needed
if [ -d "dsp_system" ]; then
    echo "Installing DSP system dependencies..."
    pip install ray numpy hypothesis pytest langsmith neo4j
fi

# Install causal-conv1d for better performance
echo "Installing causal-conv1d..."
pip install causal-conv1d>=1.4.0 --no-build-isolation

echo "Historical dependency commands finished; the environment and plug-in build are NOT verified."
echo "See docs/BUILD_AND_TEST_GUIDE.md for the currently verified narrow test and known blockers."
echo "Do not treat this script's successful exit as proof that the full repository builds or tests pass."
