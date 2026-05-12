#!/bin/bash
set -e

PLUGIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TORCH_CMAKE_DIR="/home/SGM/.local/lib/python3.14/site-packages/torch/share/cmake/Torch"
MODEL_PATH="$(dirname "$PLUGIN_DIR")/models/mamba3_audio_effects.pt"

echo "=== MambaFX CLAP Build ==="
echo "Plugin dir: $PLUGIN_DIR"
echo "Torch cmake: $TORCH_CMAKE_DIR"
echo "Model path:  $MODEL_PATH"

# Install FFTW3 via distrobox (one-time)
if ! pkg-config --exists fftw3f 2>/dev/null; then
    echo ""
    echo "FFTW3 not found. Installing via distrobox..."
    echo "Run this first:"
    echo ""
    echo "  distrobox create --name build-env --image ubuntu:22.04 --yes"
    echo "  distrobox enter build-env -- sudo apt-get install -y libfftw3-dev cmake build-essential"
    echo ""
    echo "Then re-run this script inside distrobox:"
    echo "  distrobox enter build-env -- bash '$PLUGIN_DIR/build.sh'"
    exit 1
fi

# Clean and configure
BUILD_DIR="$PLUGIN_DIR/build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DTorch_DIR="$TORCH_CMAKE_DIR" \
    -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=1"

cmake --build . --config Release -j$(nproc)

# Find the built plugin
CLAP_FILE=$(find "$BUILD_DIR" -name "*.clap" | head -1)

if [ -z "$CLAP_FILE" ]; then
    echo "ERROR: Build succeeded but no .clap file found"
    exit 1
fi

echo ""
echo "=== BUILD SUCCESSFUL ==="
echo "Plugin: $CLAP_FILE"

# Install to CLAP search path
CLAP_INSTALL_DIR="$HOME/.clap"
mkdir -p "$CLAP_INSTALL_DIR"
cp "$CLAP_FILE" "$CLAP_INSTALL_DIR/MambaFX.clap"

echo "Installed: $CLAP_INSTALL_DIR/MambaFX.clap"
echo ""
echo "Set model path before opening REAPER:"
echo "  export MAMBA_MODEL_PATH='$MODEL_PATH'"
echo ""
echo "Or add to ~/.bashrc:"
echo "  echo \"export MAMBA_MODEL_PATH='$MODEL_PATH'\" >> ~/.bashrc"
