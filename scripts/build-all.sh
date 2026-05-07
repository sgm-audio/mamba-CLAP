#!/bin/bash
# Build script for all Mamba repository components

set -e  # Exit on any error

echo "Building all Mamba repository components..."

# Build Python package (mamba_ssm)
echo "Building Python package (mamba_ssm)..."
if [ -f "setup.py" ]; then
    # CPU-only build by default, set MAMBA_SKIP_CUDA_BUILD=TRUE if needed
    python setup.py sdist --dist-dir=dist
    echo "Python package built successfully in ./dist"
else
    echo "setup.py not found, skipping Python package build"
fi

# Build MambaDAW C++ plugin
echo "Building MambaDAW C++ plugin..."
if [ -d "MambaDAW" ]; then
    cd MambaDAW
    
    # Create build directory if it doesn't exist
    if [ ! -d "build" ]; then
        mkdir build
    fi
    
    cd build
    
    # Configure with CMake (adjust path to libtorch as needed)
    # Users should set LIBTORCH_PATH environment variable or edit this line
    if [ -z "$LIBTORCH_PATH" ]; then
        echo "Warning: LIBTORCH_PATH not set. Using default path /usr/local/libtorch"
        echo "To specify libtorch path, export LIBTORCH_PATH=/path/to/libtorch before running"
        LIBTORCH_PATH="/usr/local/libtorch"
    fi
    
    echo "Configuring with libtorch at: $LIBTORCH_PATH"
    cmake .. -DCMAKE_PREFIX_PATH="$LIBTORCH_PATH"
    
    # Build
    cmake --build . --config Release
    
    echo "MambaDAW build completed successfully"
    cd ../..
else
    echo "MambaDAW directory not found, skipping C++ build"
fi

echo "All builds completed!"
echo ""
echo "Built artifacts:"
echo "  - Python package: ./dist/"
echo "  - MambaDAW binary: ./MambaDAW/build/ (if built)"