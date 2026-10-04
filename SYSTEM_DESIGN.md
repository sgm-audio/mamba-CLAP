# Proposed system design: AI-Powered Audio Mastering Service

> **Status (2026-10-04): architecture proposal only.** The gateway, queues, GPU workers, storage, billing, and integrations below are not evidence of deployed components. Do not infer security, reliability, cost, scaling, or mastering-quality guarantees from this design. See [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md).

## High-Level Architecture

### Components
1. **API Gateway** (REST + WebSocket)
   - Handles client authentication, request routing, rate limiting
   - Terminates TLS, provides DDoS protection
   - Routes upload/download jobs to processing queue

2. **Job Queue** (Message Broker: Redis or RabbitMQ)
   - Stores incoming mastering jobs with metadata
   - Enables horizontal scaling of workers
   - Supports job prioritization (e.g., premium vs free tier)

3. **Processing Workers** (Auto-scaled GPU instances)
   - Pull jobs from queue, load AI models (Mamba-based CLAP/ARA)
   - Perform file validation, preprocessing, mastering inference
   - Upload results to object storage, notify completion via callback/WebSocket

4. **Object Storage** (S3-compatible, e.g., MinIO or AWS S3)
   - Stores uploaded input files (temporary, 24h TTL)
   - Stores mastered output files (encrypted, 24h TTL unless saved)
   - Serves downloaded files via presigned URLs

5. **Database** (PostgreSQL)
   - User accounts, subscription tiers, payment info
   - Job records (status, timestamps, parameters, file references)
   - Metadata preservation (ID3/BWF) mapping

6. **ARA2 Plugin** (Cross-platform: VST3/AU/AAX)
   - Native C++ wrapper using LibTorch for inference
   - Communicates with cloud service via secure API
   - Implements ARA2 round-trip (region transfer, processing, return)

7. **Web Frontend** (React/Vue)
   - File upload UI with drag-and-drop, waveform visualization (Web Audio API)
   - Real-time progress, A/B comparison, parameter controls
   - Account management, usage history, download links

8. **Model Registry & Training Pipeline** (Optional, internal)
   - Stores versioned Mamba-based checkpoints
   - Nightly retraining pipeline using licensed/mastered audio pairs
   - A/B testing framework for preset efficacy

### Data Flow
1. **Upload (Web)**
   - Browser → API Gateway (auth) → Presigned POST to Object Storage
   - Gateway enqueues job (file URI, user ID, preset, intensity) → Job Queue
   - Worker processes job: download → validate → preprocess → model inference → postprocess → upload result
   - Worker updates DB with job status, output URI
   - Gateway/WebSocket notifies client of completion
   - Browser downloads via presigned GET

2. **Upload (ARA Plugin)**
   - DAW → ARA Plugin (encrypts & sends audio blob + params) → API Gateway
   - Same enqueue → worker → result path as above
   - Plugin polls for result, receives audio blob, inserts into DAW timeline

3. **Download**
   - Browser/Plugin requests download via API → Gateway validates ownership → returns presigned URL (short-lived)
   - Client downloads directly from Object Storage

## Deployment Topology

### Environments
- **Development**: Single-node docker-compose (local GPU if available)
- **Staging**: Mirror of production with reduced scale
- **Production**:
  - **Regions**: Primary (us-east-1) with optional failover (eu-west-1)
  - **Compute**:
    - API Gateway: 2+ instances behind ALB (AWS) or LB (K8s Ingress)
    - Job Queue: Redis Cluster (3 nodes) or RabbitMQ HA cluster
    - Workers: Managed GPU node group (AWS EKS with node autoscaling, GKE, or Azure AKS)
      - Min: 2 nodes (for HA), Max: 20 nodes (based on queue depth)
      - Instance type: g5.xlarge (NVIDIA A10G) or equivalent
    - Object Storage: S3 bucket with cross-region replication (CRR) for DR
    - Database: PostgreSQL RDS Multi-AZ with read replica
    - Web Frontend: Static site hosted on CDN (CloudFront) with S3 origin
    - ARA Plugin: Distributed via website download; updates via version check

### Scaling Approach
- **Horizontal Pod Autoscaler (K8s)** on worker deployment based on queue depth (custom metric from Redis)
- **Cluster Autoscaler** to adjust node count
- **API Gateway** scaled via ALB target groups (CPU/utilization)
- **Database** read replicas for analytics queries
- **Object Storage** inherently scalable

### CI/CD
- GitHub Actions: lint, unit tests, build Docker images, push to ECR/GCR
- Helm charts or Kustomize for Kubernetes manifests
- Blue/green deployments for zero-downtime updates
- Database migrations via Flyway/Liquibase

## Key Design Choices & Tradeoffs

| Choice | Alternative | Reasoning | Tradeoff |
|--------|-------------|-----------|----------|
| **Microservices (queue-based)** | Monolith | Enables independent scaling of GPU workers vs API; isolates failures | Increased operational complexity (service discovery, monitoring) |
| **GPU Workers (auto-scaled)** | Fixed GPU pool | Cost efficiency: scale to zero during low demand; handles burst traffic | Cold start latency (~30-60s) mitigated by keeping min 2 nodes warm |
| **Object Storage for files** | Local disk on workers | Simplifies worker statelessness, enables easy sharing, durable | Slight latency for download/upload (~100-200ms) acceptable for batch |
| **PostgreSQL** | NoSQL (MongoDB) | Strong consistency for jobs/users, relational integrity, mature tooling | Slightly higher latency vs eventual-consistency stores; mitigated with caching |
| **REST + WebSocket API** | gRPC only | REST familiar for web/devtools; WebSocket for real-time progress; avoids client library complexity | Slight overhead vs pure gRPC; acceptable for this usecase |
| **LibTorch in ARA Plugin** | ONNX Runtime | LibTorch offers best performance for PyTorch models; avoids conversion loss | Plugin size larger (~50MB); acceptable for professional audio plugins |
| **Presigned URLs** | Proxy through API | Offloads bandwidth from API servers, leverages CDN edge, simplifies scaling | Requires short-lived URL handling; mitigated with 5-15 min expiry |
| **24h TTL auto-delete** | Permanent storage | Reduces storage costs, aligns with privacy expectations, reduces compliance burden | Users must explicitly save results; mitigated with "Save to Library" feature (future) |

## Reliability Plan

### Timeouts & Retries
- **Client → API Gateway**: 30s timeout, exponential backoff (max 3 retries) on 5xx
- **API Gateway → Job Queue**: 5s timeout, immediate retry (idempotent enqueue via job ID)
- **Worker → Object Storage/DB**: 60s timeout, retry with jitter (max 3 attempts)
- **Worker Model Inference**: Hard timeout at 2x expected processing time; fails job, alerts on-chain
- **ARA Plugin → API**: 10s timeout, user-visible retry button, preserves DAW state

### Idempotency
- Jobs identified by UUID; enqueue checks for duplicate (user+file hash+params) within last hour
- Processing steps designed to be restartable: checkpoint after validation, after preprocessing, after inference
- Object storage writes use atomic multipart upload; finalize only on success

### Backpressure & Rate Limiting
- **API Gateway**: Token bucket limiter (100 req/sec/IP burst, 20 req/sec sustained)
- **Job Queue**: Workers acknowledge only after successful DB update; unacked jobs retry after visibility timeout
- **Worker Pool**: If queue depth > 2*worker_count for 5min, trigger autoscaling alert
- **Database**: Connection pool limits (max 20 connections per instance); excess requests queue with retry

### Circuit Breakers
- For external services (payment gateway, email): Hystrix-style with 5s timeout, 50% error threshold triggers open state (30s)

## Storage Choices & Consistency Model

- **Object Storage (S3)**: Eventually consistent for overwrites; we use versioning + unique UUID keys (no overwrites). Read-after-write consistency for new objects (AWS S3 guarantee).
- **Database (PostgreSQL)**: Strong consistency (READ COMMITTED default). Jobs table uses SELECT FOR UPDATE on status transitions to prevent race conditions.
- **Cache (Redis, if used)**: For transient data (e.g., session tokens); TTL-based eviction; read-through/write-behind not required.
- **Consistency Guarantees**:
  - User sees their own writes (read-after-write) via sticky sessions or session store.
  - Cross-region replication lag <5s; reads routed to primary region for strong consistency.

## Observability Plan

### Logs
- **Structured JSON** (timestamp, level, service, trace_id, span_id, message)
- **API Gateway**: Request/response logs (latency, status code, user_id)
- **Worker**: Job start/end, model load time, GPU utilization, error traces
- **Plugin**: Connection attempts, round-trip latency, DAW host info
- **Retention**: Hot (7 days), Warm (30 days via cold storage), Archive (1 year for compliance)

### Metrics (Prometheus + Grafana)
- **Request Rate**: http_requests_total{method,endpoint,status}
- **Latency**: http_request_duration_seconds_bucket (p50, p95, p99)
- **Job Processing**: job_processing_seconds{preset,outcome}, queue_depth, worker_count
- **System**: CPU/MEM/GPU utilization per node, disk I/O, network
- **Business**: active_users, successful_masterings, failed_jobs, retry_rate
- **SLIs/SLOs**:
  - Availability: 99.5% of requests return <5xx (measured over 1h windows)
  - Latency: 95% of mastering jobs <2x audio length (SLO)
  - Error rate: <1% of jobs fail permanently (SLO)

### Traces (OpenTelemetry / Jaeger)
- Trace context propagated: Browser → API Gateway → Job Queue → Worker → Object Storage
- Spans: HTTP request, queue enqueue, DB write, S3 upload/download, model inference (preprocess, infer, postprocess)
- Sampling: 10% of traces, 100% of errors

### Alerting (PagerDuty/Slack)
- Critical: API error rate >5% for 5min, worker node failure, queue depth >1000 for 10min
- Warning: Processing latency >SLO for 15min, DB replication lag >5s, storage usage >80%

## Failure Modes & Mitigations

| Failure Mode | Impact | Mitigation |
|--------------|--------|------------|
| **GPU driver/cuda error** | Worker crashes, job loss | Worker catches exception, marks job failed, auto-restart via K8s liveness probe; node drained & replaced |
| **Object storage regional outage** | Upload/download failures | Cross-region replication; API Gateway can redirect to secondary region with latency penalty |
| **Database failure** | Job loss, user impact | Multi-AZ standby with automatic failover; read replica for reads during primary recovery |
| **Job queue backlog** | Increased latency | Autoscaling based on queue depth; admin can manually scale workers; priority queuing for premium |
| **Malicious file upload** | Security/compliance risk | File type validation (magic bytes), AV scan via ClamAV on upload, sandboxed worker environment |
| **Model inference NaN/output corruption** | Silent bad output | Worker validates output RMS/peak; if NaN or excessive clipping (> -0.5dB) marks job failed and alerts |
| **Network partition (worker unable to reach DB/queue)** | Jobs stuck in processing | Worker heartbeat missing after 2min → job marked failed; retry enqueued |
| **ARA plugin version incompatibility** | Plugin fails to load | Plugin checks server version on init; displays upgrade prompt; maintains backward compatibility for 2 versions |
| **Sudden traffic spike (e.g., viral social)** | Overwhelm autoscaling | Burst capacity via reserved instances; rate limit per IP; graceful degradation (queue longer wait times) |
| **Licensing/model usage dispute** | Legal risk | Only use licensed or internally generated training data; maintain provenance logs; opt-in for data usage |

## Architectural Decision Records (ADRs)

### ADR-001: Use Queue-Based Worker Architecture
- **Status**: Accepted
- **Context**: Need to scale GPU-intensive mastering jobs independently from API request handling.
- **Decision**: Implement a job queue (Redis/RabbitMQ) with stateless GPU workers pulling jobs.
- **Consequences**:
  - + Scalable, fault-tolerant, enables spot/preemptible instances for cost savings
  - - Increased system complexity, eventual consistency for job status (mitigated via WebSocket polling)
- **Status**: Accepted

### ADR-002: Store Audio Files in Object Storage, Not Database
- **Status**: Accepted
- **Context**: Audio files are large (MB to GB), numerous, and temporary.
- **Decision**: Use S3-compatible object storage with presigned URLs for transfer; store only metadata (URI, checksum) in DB.
- **Consequences**:
  - + Scalable, cost-effective (pay-per-storage), enables CDN caching, simplifies worker statelessness
  - - Requires handling of eventual consistency (mitigated by UUID keys and short TTL)
- **Status**: Accepted

### ADR-003: PostgreSQL for Transactional Data
- **Status**: Accepted
- **Context**: Need strong consistency for job states, user accounts, and payments.
- **Decision**: Use managed PostgreSQL (RDS/Aurora) with multi-AZ replication.
- **Consequences**:
  - + ACID transactions, rich querying, mature tooling, familiar to team
  - - Higher cost than NoSQL; vertical scaling limits (mitigated with read replicas and caching)
- **Status**: Accepted

### ADR-004: LibTorch in ARA Plugin vs ONNX
- **Status**: Accepted
- **Context**: ARA plugin needs low-latency inference for interactive use.
- **Decision**: Bundle LibTorch with plugin to run PyTorch Mamba models directly.
- **Consequences**:
  - + Best performance, no conversion loss, full PyTorch feature support
  - - Increases plugin size (~40-60MB), longer initial load (~1s) mitigated by caching
- **Status**: Accepted

### ADR-005: Presigned URLs for Direct Client-Storage Transfer
- **Status**: Accepted
- **Context**: Avoid API server becoming bandwidth bottleneck for file transfers.
- **Decision**: Generate time-limited (5-15min) presigned GET/POST URLs for S3 operations.
- **Consequences**:
  - + Offloads bandwidth, leverages CDN edge, reduces API complexity
  - - Requires client to handle URL expiry; mitigated by short-lived UI elements and auto-refresh
- **Status**: Accepted

### ADR-006: Ephemeral File Storage with 24h TTL
- **Status**: Accepted
- **Context**: Privacy expectations and cost minimization for transient mastering jobs.
- **Decision**: Automatically delete input/output objects after 24 hours unless user explicitly saves.
- **Consequences**:
  - + Reduces storage costs (~90% savings), aligns with GDPR data minimization, reduces attack surface
  - - Requires user education; mitigated by clear UI warnings and "Save to Library" feature (future)
- **Status**: Accepted

## Risks & Prototyping Needs

### High-Risk Items Requiring Spike/Prototype
1. **Model Inference Latency on Target GPU**
   - Spike: Benchmark Mamba-based CLAP inference on g5.xlarge with various audio lengths; optimize batching, TensorRT if needed.
2. **ARA Plugin Real-Time Audio Transfer**
   - Spike: Implement prototype VST3 plugin using LibTorch; measure round-trip latency with DAWs (Pro Tools, Logic) under various buffer sizes.
   - Mitigation: Fallback to offline processing if latency unacceptable.
3. **True Peak Limiting Algorithm Implementation**
   - Spike: Implement ITU BS.1770-4 compliant true peak limiter in Torch/CUDA; validate with reference meters (PPM, TC Electronic).
4. **Cold Start Mitigation for GPU Workers**
   - Spike: Test keeping 2 warm nodes vs scaling to zero; measure cost vs latency tradeoff; consider keeping minimal GPU fraction always on.

### Mitigated Through Design
- **Model Drift/Quality Degradation**: Addressed via continuous monitoring of output metrics (Loudness, THD, spectral centroid) against reference; automated rollback on degradation.
- **Security Vulnerabilities in Plugin**: Mitigated by sandboxing (seccomp, namespaces), minimal dependencies, regular dependency audits, fuzzing of audio parsers.
- **Legal/IP Issues with Training Data**: Mitigated by using only licensed/internal data, maintaining data provenance logs, and explicit opt-in for usage.
