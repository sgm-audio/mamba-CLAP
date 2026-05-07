#!/bin/bash
set -e  # Exit on any error

echo "=== MambaDAW Build and Test Script ==="

# Step 1: Create virtual environment and install Python requirements
echo "Step 1: Setting up Python environment..."
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
echo "Python environment ready."

# Step 2: Run training script to generate model (1 epoch for quick test)
echo "Step 2: Training model (1 epoch)..."
mkdir -p resources
python python_training/train_mamba.py --epochs 1
echo "Model training complete. Model saved to resources/mamba_model.pt"

# Step 3: Build C++ project with CMake
echo "Step 3: Building C++ plugin..."
mkdir -p build && cd build

# Provide instructions for LibTorch if not found
if [ -z "$LIBTORCH_DIR" ]; then
    echo "Warning: LIBTORCH_DIR environment variable not set."
    echo "Please set LIBTORCH_DIR to the root of your LibTorch installation."
    echo "Example: export LIBTORCH_DIR=/path/to/libtorch"
    echo "Attempting to find LibTorch via CMake's find_package(Torch)..."
fi

# Configure and build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
echo "C++ plugin built successfully."

# Step 4: Copy .clap file to standard plugin directory
echo "Step 4: Installing plugin..."
PLUGIN_DIR="$HOME/.clap"
mkdir -p "$PLUGIN_DIR"
cp release/libMambaDAW.clap "$PLUGIN_DIR/" 2>/dev/null || cp release/MambaDAW.clap "$PLUGIN_DIR/" || echo "Warning: Could not find .clap file in expected locations."
echo "Plugin copied to $PLUGIN_DIR"

deactivate
echo "=== Build and Test Complete ==="
echo "To use the plugin, ensure your DAW scans $PLUGIN_DIR for CLAP plugins."
echo "Known limitations:"
echo "1. Current Mamba implementation uses sequential scanning which limits context length."
echo "2. Audio frontend uses approximate Mel filterbank (not perfect inversion)."
echo "3. Model exported is traced and may not support all TorchScript features."
echo "Next steps for optimization:"
echo "1. Implement parallel scan CUDA kernel for faster inference."
echo "2. Replace approximate Mel with learnable filterbank or use torchaudio compatible filters."
echo "3. Implement Griffin-Lim for better phase recovery in iSTFT."