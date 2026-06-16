#!/bin/bash
# Test runner script for Mamba repository

set -e  # Exit on any error

# Default to running all tests if no arguments provided
TEST_TARGET="${1:-all}"

echo "Running tests for: $TEST_TARGET"

case "$TEST_TARGET" in
    all|basic)
        echo "Running basic validation tests..."
        python test_mamba.py
        ;;
    mamba)
        echo "Running Mamba core tests..."
        if [ -d "mamba/tests" ]; then
            cd mamba && python -m pytest tests/ -v
            cd ..
        else
            echo "No tests found in mamba/tests"
        fi
        ;;
  dsp)
    echo "Running DSP system tests..."
    if [ -d "emotion-aware-dsp" ]; then
      cd emotion-aware-dsp && python -m pytest -v
      cd ..
    else
      echo "emotion-aware-dsp directory not found"
    fi
    ;;
    *)
        echo "Unknown test target: $TEST_TARGET"
        echo "Available targets: all, basic, mamba, dsp"
        exit 1
        ;;
esac

echo "Tests completed successfully!"
