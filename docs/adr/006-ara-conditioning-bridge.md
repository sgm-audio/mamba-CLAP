# ADR-006: ARA Musical Context Conditioning Bridge

## Status
Accepted

## Context
ARA 2.0 provides musical/document context that must influence the SSM in a thread-safe way and support offline rendering workflows.

## Decision
Define a deterministic conditioning bridge:

`ARA context -> ConditioningFrame -> float cond[COND_DIM] -> 1x1 projections -> bias delta/B in SSM step`

### Contract
- `ARA Thread` computes conditioning vectors from document/playback context.
- Conditioning is published through atomic double-buffered frame contract (ADR-003).
- `Audio Thread` reads active frame and applies lightweight projection outputs as additive biases to SSM `delta` and `B` terms.
- Projection weights are preloaded and immutable in RT path.

### Offline Rendering
- ARA offline render jobs may request higher-quality context extraction on ARA thread.
- Resulting frames must still pass through the same lock-free frame contract to maintain one inference code path.

### Feature Ownership
- Context extraction logic: `Host API` + `UI/State` domain.
- Projection application and bias fusion: `SSM Core` domain.
- Weight preparation and validation: `ML Pipeline` domain.

## Consequences
### Positive
- ARA context can influence tone/motion without thread-law violations.
- Same conditioning contract works for live and offline rendering.

### Negative
- Requires strict dimensional validation between ARA extractor and model config.

## Verification Requirements
- Unit tests for context-to-vector mapping and projection shape checks.
- Integration tests that compare conditioned vs unconditioned output deterministically.

## Acceptance Criteria
- ARA musical context is reflected in `cond[COND_DIM]` and biases SSM `delta` and `B` through 1x1 projections.
- No locks, allocations, or file access are introduced in audio callback by conditioning path.
