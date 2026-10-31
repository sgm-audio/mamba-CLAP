# Implementation summary — research/prototype status

**Status:** In progress. This repository does not currently contain a verified production Mamba-3 audio plug-in.

## Components

- `mamba/`: vendored upstream Mamba-family Python code, including Mamba-3. It is not wired into a C++ plug-in runtime here.
- `MambaDAW/`: custom recurrent SSM and CLAP-oriented C++ prototype. The layer is not upstream Mamba/Mamba-3. The full processor path has a missing `inference_thread.h` dependency and no completed thread/engine implementation.
- `clap-plugin/`: separate LibTorch/TorchScript CLAP experiment. Torch/STFT/model calls occur in the audio callback; no real-time safety or trained Mamba-3 audio model is verified.
- `plugin/`: separate architecture experiment with missing CMake-listed sources and incomplete inference-result wiring.
- `mamba-compressor/`: Python research into Mamba and differentiable DSP; no validated checkpoint or plug-in integration is established by the current evidence.

## Verified scope and open work

A standalone `MambaLayer` test can be compiled with GCC and the C++ layer source. This is a narrow smoke test only. Full CMake builds, DAW validation, Python/C++ model parity, audio-quality evaluation, and worst-case real-time measurements remain unverified.

The default MambaDAW trainer uses a scalar waveform model (`d_model=1`) while the processor constructs an 80-dimensional mel-frame model; the pipeline needs a single matched representation, checkpoint format, and parity test. LoRA, RunPod deployment, and commercial adapter delivery are not implemented.

See [`PROJECT_STATUS.md`](../PROJECT_STATUS.md) for workstream labels and release gates. Earlier “implementation complete” documents are superseded and should not be read as verification records.
