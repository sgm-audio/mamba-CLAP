# Threading and buffering — proposed design, not verified

**Status (2026-10-04):** The threading model below is an engineering target. It is not a report of a working, measured implementation. `MambaDAW` references an inference thread whose header/implementation are absent. The separate `clap-plugin/` experiment performs Torch/STFT/model work inside its callback, and the separate `plugin/` worker architecture is incomplete. See [`PROJECT_STATUS.md`](./PROJECT_STATUS.md).

Earlier versions of this document stated a 133 ns/sample result, zero-sample latency, AVX2 guarantees, and a completed real-time-safety checklist. No reproducible benchmark or complete callback audit supporting those statements is present in this checkout; those claims are withdrawn.

## Proposed processing boundary

For a model that cannot be proven to meet the host callback deadline, the preferred experiment is:

```text
Audio callback (bounded work)              Worker (not real-time)
----------------------------               -----------------------
copy/feature extraction into                pop complete frame(s)
preallocated bounded SPSC queue  ------->   run model/inference
read newest valid parameters     <-------   publish fixed-size result
apply bounded DSP / dry fallback           handle model load, errors,
                                            diagnostics, model swapping
```

This diagram is a target, not a claim that the queues, worker, or result path are currently wired end to end. A worker does not by itself make a plug-in real-time safe: queue-full behavior, stale results, reset/seek semantics, shutdown, and output continuity must all be specified and tested.

## Callback budget and measurement

The available time for one host block is `frames / sample_rate`. For example, 64 frames at 48 kHz is about 1.33 ms for the **entire** processing chain. That is a deadline, not a target average. Measure worst-case time and underruns in the actual host on target CPUs and block sizes.

Required checks before making any RT-safety claim:

- instrument allocations, deallocations, locks, logging, file I/O, exceptions, and system calls in the callback;
- exercise minimum/maximum block sizes, sample rates, silence, seeks/resets, rapid automation, model-load failure, queue overflow, and multiple plug-in instances;
- measure tail latency (not just average throughput), and validate with the host's plug-in validator and at least one DAW;
- define a bounded fallback when inference is late or unavailable;
- verify all state and feature queues are single-producer/single-consumer as designed, and confirm lifetime/shutdown ordering.

## Current code observations

- `MambaDAW/src/plugin_processor.cpp` enqueues mel frames through an `InferenceThread`, but `MambaDAW/src/inference_thread.h` and the corresponding implementation are absent.
- `clap-plugin/src/clap-plugin.cpp` calls Torch/STFT/model operations from `processAudioBlock()`, so its “preallocated buffers” comments do not establish callback safety.
- `plugin/core/PluginCore.cpp` starts an engine but does not wire its ring/STFT pointers or consume inference results; the engine implementation and several DSP sources are missing.
- `MambaDAW`'s optional INT8 code lazily allocates quantized vectors when first used. It must not be selected on an audio thread until preparation is explicit and tested off-thread.

## Status

**No hard-real-time safety, zero-latency, 133 ns/sample, or dropout-free result is currently verified.** Record benchmark hardware, compiler flags, model dimensions, sample rate, host block size, binary revision, and worst-case measurement with any future performance claim.
