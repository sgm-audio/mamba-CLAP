# Thread Ownership Diagram

```mermaid
flowchart LR
    subgraph MAIN[Main Thread]
        UI[UI and CLAP Param Flush]
        SER[State Serialize/Restore]
        LOAD[Weight Loader and mmap]
        PARAM_PUB[Param Publisher]
    end

    subgraph ARA[ARA Thread]
        DOC[DocumentController Updates]
        CTX[Musical Context Extractor]
        ENC[Conditioning Encoder]
    end

    subgraph RT[Audio Thread process()]
        EVENT_DRAIN[Drain Param Events]
        MODE[Mode Planner Runtime Choice]
        SSM[SSM Core Inference]
        HSTATE[(Live h_state owner)]
        SNAP[Snapshot Writer Seqlock]
    end

    PARAM_PUB -->|SPSC or atomics| EVENT_DRAIN
    ENC -->|Atomic double buffer ConditioningFrame| SSM
    LOAD -->|Prepared runtime context swap| SSM

    EVENT_DRAIN --> MODE
    MODE --> SSM
    SSM --> HSTATE
    HSTATE --> SNAP
    SNAP -->|Seqlock read| SER

    classDef rt fill:#fce5cd,stroke:#b45f06,stroke-width:2px;
    classDef nrt fill:#d9ead3,stroke:#38761d,stroke-width:1px;
    class RT,EVENT_DRAIN,MODE,SSM,HSTATE,SNAP rt;
    class MAIN,ARA,UI,SER,LOAD,PARAM_PUB,DOC,CTX,ENC nrt;
```

## Ownership Law
- Live `h_state` is mutable only on audio thread.
- Main thread and ARA thread communicate with audio only via lock-free channels.
- No direct pointer sharing for mutable state across thread domains.
