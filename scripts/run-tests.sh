#!/usr/bin/env bash
# Legacy test dispatcher. Only run targets that exist; never report a missing
# component as a successful test run.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
TEST_TARGET="${1:-all}"

echo "Running test target: $TEST_TARGET"

case "$TEST_TARGET" in
  all|basic)
    if [[ ! -f test_mamba.py ]]; then
      echo "ERROR: test_mamba.py is absent; the 'all/basic' target is unavailable." >&2
      exit 2
    fi
    python test_mamba.py
    ;;
  mamba)
    if [[ ! -d mamba/tests ]]; then
      echo "ERROR: mamba/tests is absent; the upstream test target is unavailable." >&2
      exit 2
    fi
    python -m pytest mamba/tests/ -v
    ;;
  dsp)
    echo "ERROR: no DSP test component is present in this checkout; target unavailable." >&2
    exit 2
    ;;
  *)
    echo "ERROR: unknown target '$TEST_TARGET'. Available targets: all, basic, mamba." >&2
    exit 2
    ;;
esac

echo "Test target '$TEST_TARGET' completed successfully."
