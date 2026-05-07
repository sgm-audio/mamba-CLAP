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
        if [ -d "dsp_system" ]; then
            if [ -f "dsp_system/test_tube_dsp.py" ]; then
                python dsp_system/test_tube_dsp.py
            else
                echo "Running pytest in dsp_system..."
                cd dsp_system && python -m pytest -v
                cd ..
            fi
        else
            echo "DSP system directory not found"
        fi
        ;;
    *)
        echo "Unknown test target: $TEST_TARGET"
        echo "Available targets: all, basic, mamba, dsp"
        exit 1
        ;;
esac

echo "Tests completed successfully!"