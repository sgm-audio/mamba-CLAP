#!/usr/bin/env bash
# Local checks for the parts of this repository that build today.
# Usage: bash scripts/ci.sh   (CXX defaults to c++; set CXX=clang++ etc.)
# Not covered: MambaDAW plug-in build, plugin/, clap-plugin/ (see docs/PROJECT_STATUS.md).
set -euo pipefail
cd "$(dirname "$0")/.."
CXX="${CXX:-c++}"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

echo "== Rust crate (mamba_prompt_system): cargo test"
cargo test --quiet

echo "== C++ standalone layer test"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -IMambaDAW/src \
  MambaDAW/tests/test_mamba_layer.cpp MambaDAW/src/mamba_layer.cpp -o "$out/test_mamba_layer"
"$out/test_mamba_layer"

echo "== C++ parameter gate test"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -IMambaDAW/src \
  MambaDAW/tests/param_gate_test.cpp MambaDAW/src/parameter_gate.cpp -o "$out/param_gate_test"
"$out/param_gate_test"

echo "ci.sh: all checks passed"
