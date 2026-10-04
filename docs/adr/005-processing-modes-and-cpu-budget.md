# ADR-005: Processing Modes and CPU Budget Enforcement

## Status
Accepted as a design target only; CPU-budget thresholds and processing modes are not measured or implemented end to end. See [`../PROJECT_STATUS.md`](../PROJECT_STATUS.md).

## Context
The product must support both tiny and large Mamba models while keeping default-model DSP below 30% of real-time budget on a 4-core 2.5GHz CPU.

## Decision
Define two runtime modes with explicit eligibility and contracts.

### Mode A: Tiny Model Path
- Eligibility: models <= 2M parameters.
- Step granularity: per-sample or micro-block (`4-16` samples).
- Intended for minimum latency and simple causal pipelines.

### Mode B: Block Path
- Eligibility: models > 2M parameters.
- Step granularity: block chunks (`64-512` samples) with state carryover.
- Supports overlap-add when non-causal components are enabled.

### Shared Rules
- `h_state` continuity is preserved across block boundaries.
- Mode switch is resolved in main thread and applied at safe boundary (activation/reconfiguration point).
- Audio-thread branch behavior must avoid dynamic allocation and lock acquisition.

### CPU Budget Policy
- Default model benchmark must remain <= 30% RT budget on target reference CPU.
- If measured load exceeds threshold:
  1. Prefer larger chunking in Mode B.
  2. Reduce optional non-essential processing paths.
  3. Enforce fallback/bypass policy with host notification path on NRT thread.

## Consequences
### Positive
- Scales from low-parameter to high-parameter models.
- Preserves deterministic state handling while providing performance levers.

### Negative
- More runtime planner complexity and validation paths.

## Verification Requirements
- Performance harness measures worst-case callback time and average CPU under both modes.
- Regression threshold gate in CI for default-model profile.

## Proposed acceptance criteria (not passed)
- Demonstrate state continuity and callback-safety constraints for each implemented mode.
- Measure a named default model against a documented CPU-budget gate on reference hardware.
