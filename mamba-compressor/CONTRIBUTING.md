# Contributing to MambaCompressor — research prototype

> **Status (2026-10-04):** experimental Python research code. There is no validated package-install workflow, checked-in `tests/` suite, pretrained model, or release process for this component. See [`README.md`](README.md) and [`../docs/PROJECT_STATUS.md`](../docs/PROJECT_STATUS.md). Treat the development commands from older versions of this guide as withdrawn unless the referenced files exist and the commands have been tested.

## Contribution focus

Useful contributions include reproducible model/data tests, checkpoint save/load and inference contracts, baseline comparisons, dataset/license documentation, numerical robustness, and careful audio-quality evaluation. CUDA/Triton kernels, streaming, multiband processing, and plug-in integration are research or implementation tasks—not existing supported features.

## Before submitting a change

1. State the hypothesis and the exact component/model/API being changed.
2. Add a focused test or reproducible experiment; include tensor shapes, sample rate, seed/config, and expected behavior.
3. Record the exact command and environment used. Do not state “all tests pass” when there is no defined suite or when dependencies prevented a run.
4. Separate measured quality/latency/memory results from design targets; include baselines and listening protocol for audio claims.
5. Check data and dependency licenses before adding training assets or distribution instructions.

## Current test/install limitations

- No `pyproject.toml`/`setup.py` or `tests/` directory is present under `mamba-compressor/`; the former `pip install -e ".[dev]"` and `pytest tests/` examples are not supported by the current tree.
- `scripts/train_blind.py` is an experimental entry point, but end-to-end training was not run in the current audit environment. Its CLI and data settings are summarized in [`README.md`](README.md).
- The standalone C++ layer harness is documented at the repository level in [`../docs/BUILD_AND_TEST_GUIDE.md`](../docs/BUILD_AND_TEST_GUIDE.md); it does not test this Python component.
