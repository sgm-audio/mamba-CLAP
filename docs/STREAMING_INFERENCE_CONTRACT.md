# Proposed streaming-inference contract — not implemented

> **Status (2026-10-04):** design proposal / in progress. This contract is not an established runtime. The MambaDAW inference-thread header/source are absent, the custom C++ SSM is not upstream selective Mamba/Mamba-3, and the trainer/export dimensions do not match the plug-in processor by default. The pseudocode, zero-latency claim, SIMD/alignment assumptions, and RT assertions from the previous version are withdrawn. See [`PROJECT_STATUS.md`](./PROJECT_STATUS.md).

## Scope and model contract

This document is a checklist for a future streaming implementation. It does not describe the current `clap-plugin/` TorchScript callback or prove a complete `MambaDAW` plug-in. Before implementation, select and document:

- the exact model family/revision and checkpoint format;
- waveform vs. feature input, sample rate, hop/frame rate, channel layout, tensor shapes, and normalization;
- state shapes, ownership, persistence, and reset/seek/sample-rate-change semantics;
- weight-loading and export path, including Python/C++ numerical parity;
- the thread on which inference runs and the bounded behavior when results are late or queues fill.

The existing custom C++ layer accepts scalar or feature-vector inputs, but its fixed, input-independent recurrence is not upstream selective Mamba. The current training and plug-in configurations are not yet aligned, so the table below is a schema template rather than a verified model contract.

| Item to define | Required evidence |
|---|---|
| Input/output shapes and numerical range | Versioned config plus valid/invalid shape tests |
| Recurrent/conv state | Deterministic state round-trip, reset, seek, and boundary tests |
| Weights/checkpoint | Versioned export, malformed-file tests, known test fixture |
| Streaming schedule | Block/frame/worker contract with queue capacity and stale-result policy |
| Output continuity | Host-level tests for bypass, underrun, reset, and reactivation |

## Proposed processing contract

The chosen implementation must specify a causal/streaming API and ensure that state is neither accidentally reset at block boundaries nor reused across independent instances. A future worker architecture must define bounded queue operations, overflow/underrun behavior, shutdown, model reload, and how the host receives current results. The current repository has not demonstrated this full path.

Do not put LibTorch/TorchScript/STFT model execution on a hard-real-time callback. The separate `clap-plugin/` path currently performs Torch/STFT/model operations in its callback and is not hard-real-time safe. A proposed worker design remains unimplemented/incomplete until input, inference, result handoff, and output processing are wired and tested end to end.

## Latency and real-time status

**No end-to-end latency, zero-sample algorithmic latency, deadline, allocation, dropout, or host-callback result is verified.** A causal recurrent layer might avoid look-ahead in isolation, but framing, feature extraction, worker scheduling, queues, reconstruction, and host buffering can add delay. Measure both reported plug-in latency and loopback behavior on named hosts/hardware with worst-case callback sizes.

Before any real-time claim, test worst-case callback duration, allocation/blocking behavior, thread safety, queue saturation, error/reset paths, sample rates, small/variable blocks, multiple instances, and shutdown under actual host load. Keep hard-RT assertions out of the audio callback; use instrumentation that does not itself violate callback constraints.

## Release gate

Do not describe this contract as implemented until the selected model and runtime build, Python/C++ outputs match on deterministic fixtures, DAW host tests pass, audio quality is evaluated, and allocation/deadline audits are published. The standalone `MambaLayer` test covers only the isolated custom layer.
