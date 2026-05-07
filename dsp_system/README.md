# Emotion-Aware DSP System

This directory contains the core artifacts for a Ray-based, emotion-aware audio DSP system.

## Components

- `actors.py`: Ray actor definitions with integrated emotional state and soul checksums.
- `test_tube_dsp.py`: Dual-mode test suite (technical integrity + emotional coherence).
- `pipeline.yml`: Orchestration build pipeline for Architect, CodeGen, and Test agents.
- `neo4j_import.cypher`: Cypher script to map agent coordination and friction into a graph.
- `mediator_prompt.md`: Specialized prompt for resolving architectural vs. implementation friction.

## Requirements

- Python 3.8+
- Ray
- NumPy
- Hypothesis
- Pytest
- LangSmith (API access for tracing)
- Neo4j (for graph state management)

## Soul Constraint
"If this system feels like a black box, it has failed. Every component must be introspectable emotionally and technically."
