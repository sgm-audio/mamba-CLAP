# Public messaging draft — **HOLD / not approved for posting**

**Status (2026-10-04):** The earlier drafts included unsubstantiated 60-second-context, RTX 4090 latency/memory, professional-quality, and release claims. Those claims have been removed. Do not publish performance or quality numbers until backed by a reproducible benchmark and evaluation report.

## Safe research-prototype draft

> I’m exploring whether state-space sequence models can help predict compressor parameters from audio context. The repository currently contains experimental Python model and differentiable-DSP code. It is not a finished plug-in, and I have not yet validated effective context duration, audio quality, real-time latency, or DAW-host behavior. I’d welcome feedback from audio/DSP developers on suitable baselines and evaluation methods.

## Evidence required before stronger claims

Before posting a release or benchmark announcement, attach:

- exact code revision, model/checkpoint, configuration, dataset and license/attribution;
- named hardware, framework, precision, batch/block size, sample rate, and measurement method;
- worst-case latency and memory measurements (not only average throughput);
- objective comparisons and blind listening results;
- reproducible test commands and any plug-in/host validation results.

Do not reuse the former 60-second, 3.2-ms, 1.8-GB, sub-5-ms, or “professional-grade” figures. They are not verified results in this repository.
