#!/bin/bash
# Development environment setup script for Mamba repository

set -e  # Exit on any error

echo "Setting up Mamba repository development environment..."

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
MAMBA_FORCE_BUILD=TRUE pip install --no-cache-dir mamba-ssm --no-build-isolation

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

echo "Development environment setup complete!"
echo ""
echo "To verify the installation, run:"
echo "  python test_mamba.py"
echo ""
echo "To run tests:"
echo "  ./scripts/run-tests.sh"
echo ""
echo "To build all components:"
echo "  ./scripts/build-all.sh"