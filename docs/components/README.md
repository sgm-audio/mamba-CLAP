# Component inventory and boundaries

> **Status (2026-10-04):** This document describes repository boundaries, not an integrated product. No train → export → load → real-time plug-in pipeline is verified. Use [`../PROJECT_STATUS.md`](../PROJECT_STATUS.md) as the current status source.

## Components

- **`mamba/` — upstream reference/research code.** Contains upstream Mamba-family Python modules and ops. It is not connected to the C++ plug-in paths or a verified audio-effect checkpoint in this repository.
- **`mamba-compressor/` — experimental Python model/training code.** Researches sequence-model parameter prediction and differentiable DSP. No verified pretrained checkpoint loader, complete inference example, or quality evaluation is established.
- **`MambaDAW/` — custom C++ SSM and plug-in prototype.** Its layer can be tested independently, but the complete CLAP processor is blocked by a missing inference-thread header and unaligned trainer/export dimensions. It is not upstream Mamba/Mamba-3.
- **`clap-plugin/` — separate LibTorch/TorchScript CLAP experiment.** It performs Torch/STFT/model work in its audio callback and is not hard-real-time safe. It is not the same implementation as `MambaDAW`.
- **`plugin/` — incomplete plug-in architecture experiment.** The CMake target references missing sources, and worker/result wiring is incomplete.
- **Root-level Python/audio files** — separate research scripts; their presence does not establish model quality or integration with a plug-in runtime.

## Integration status

These components are not currently interchangeable or joined into one product. The model formats, dimensions, state contracts, trained weights, callback path, and host wrappers differ or remain unverified. Treat architecture diagrams and shared-dependency lists as proposals unless a build/test is recorded.

There is no checked-in `/dsp_system` component in the current repository. Do not use stale paths, dependency lists, or implementation descriptions without checking the current tree.
