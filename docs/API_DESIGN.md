# Proposed data model & API design strategy

> **Status (2026-10-04): API/schema proposal only.** Endpoints, tables, and integration details below are design notes, not a verified or deployed service contract. Check current routes, migrations, and tests before treating any item as implemented.

## 1. Data Model

### Entities & Relationships
The system uses a relational model (PostgreSQL) for structured data and Redis for transient state.

#### `users`
| Column | Type | Constraints | Description |
|--------|------|-------------|-------------|
| `id` | UUID | PK, Default gen_random_uuid() | Unique identifier |
| `email` | VARCHAR(255) | UNIQUE, NOT NULL | User login email |
| `password_hash` | TEXT | NOT NULL | Argon2/BCrypt hash |
| `tier` | VARCHAR(50) | NOT NULL, Default 'free' | 'free', 'pro', 'enterprise' |
| `created_at` | TIMESTAMPTZ | NOT NULL, Default NOW() | Account creation time |

#### `presets`
| Column | Type | Constraints | Description |
|--------|------|-------------|-------------|
| `id` | VARCHAR(50) | PK | Slug (e.g., 'transparent') |
| `name` | VARCHAR(100) | NOT NULL | Display name |
| `description` | TEXT | | Human-readable explanation |
| `config_schema` | JSONB | NOT NULL | JSON Schema for valid params |

#### `jobs`
| Column | Type | Constraints | Description |
|--------|------|-------------|-------------|
| `id` | UUID | PK, Default gen_random_uuid() | Job ID |
| `user_id` | UUID | FK -> users.id, NOT NULL | Owner |
| `input_key` | TEXT | NOT NULL | S3 Object Key for source |
| `output_key` | TEXT | | S3 Object Key for result |
| `preset_id` | VARCHAR(50) | FK -> presets.id | Selected algorithm |
| `parameters` | JSONB | NOT NULL | Flattened params (intensity, LUFS, etc.) |
| `status` | VARCHAR(20) | NOT NULL | PENDING, PROCESSING, COMPLETED, FAILED |
| `metrics` | JSONB | | Results: LUFS, True Peak, LRA |
| `error_info` | JSONB | | Code and message if failed |
| `source` | VARCHAR(20) | NOT NULL | 'web', 'ara_plugin' |
| `created_at` | TIMESTAMPTZ | NOT NULL | Creation timestamp |
| `expires_at` | TIMESTAMPTZ | NOT NULL | 24-hour deletion deadline |

### Indexes
- `idx_jobs_user_id`: For fast retrieval of user history.
- `idx_jobs_status_created`: For worker polling and cleanup monitoring.
- `idx_jobs_expires_at`: For the lifecycle manager (redundant with S3 policy but good for DB cleanup).

### Migration Strategy
- **Tool**: `Alembic` (Python) or `Flyway` (Java).
- **Rules**:
    - No `SELECT *` in application code to allow column additions.
    - Destructive changes (column drops) require a 3-step release: (1) Add new, (2) Backfill & Dual-write, (3) Drop old.

---

## 2. API Strategy

### Versioning
- **URL Versioning**: `/v1/...` for major breaking changes.
- **Header Versioning**: Optional `X-API-Version` for minor feature toggles.

### Validation Rules
- **Loudness**: Clamped between `-24.0` and `-6.0` LUFS.
- **Intensity**: Integer `0-100`.
- **Files**: Validated via header check (WAV, AIFF, FLAC) before job acceptance.

---

## 3. OpenAPI Specification

```yaml
openapi: 3.0.3
info:
  title: Mamba Mastering API
  description: AI-Powered Audio Mastering Service with Context-Aware SSMs.
  version: 1.0.0
servers:
  - url: https://api.mamba-mastering.com/v1
    description: Production Server

components:
  securitySchemes:
    bearerAuth:
      type: http
      scheme: bearer
      bearerFormat: JWT

  schemas:
    ErrorResponse:
      type: object
      properties:
        error:
          type: object
          properties:
            code:
              type: string
              example: "VALIDATION_FAILED"
            message:
              type: string
              example: "The requested loudness target is out of range."
            details:
              type: array
              items:
                type: object

    Job:
      type: object
      properties:
        id:
          type: string
          format: uuid
        status:
          type: string
          enum: [PENDING, PROCESSING, COMPLETED, FAILED]
        preset_id:
          type: string
        parameters:
          $ref: '#/components/schemas/JobParameters'
        metrics:
          type: object
          properties:
            integrated_lufs: { type: number }
            true_peak_db: { type: number }
            lra: { type: number }
        output_url:
          type: string
          format: uri
        expires_at:
          type: string
          format: date-time

    JobParameters:
      type: object
      required: [preset_id]
      properties:
        preset_id:
          type: string
          enum: [transparent, warm, loud]
        intensity:
          type: integer
          minimum: 0
          maximum: 100
          default: 50
        loudness_target:
          type: number
          minimum: -24
          maximum: -6
          default: -14
        stereo_width:
          type: number
          minimum: 0
          maximum: 200
          default: 100

paths:
  /uploads/request:
    post:
      summary: Request a pre-signed URL for audio upload
      security: [{ bearerAuth: [] }]
      responses:
        '200':
          description: Pre-signed URL generated
          content:
            application/json:
              schema:
                type: object
                properties:
                  upload_url: { type: string, format: uri }
                  file_key: { type: string }

  /jobs:
    get:
      summary: List user mastering jobs
      security: [{ bearerAuth: [] }]
      parameters:
        - name: limit
          in: query
          schema: { type: integer, default: 20 }
        - name: offset
          in: query
          schema: { type: integer, default: 0 }
      responses:
        '200':
          description: List of jobs
          content:
            application/json:
              schema:
                type: object
                properties:
                  items:
                    type: array
                    items: { $ref: '#/components/schemas/Job' }
                  total: { type: integer }

    post:
      summary: Submit a new mastering job
      security: [{ bearerAuth: [] }]
      requestBody:
        required: true
        content:
          application/json:
            schema:
              type: object
              required: [file_key, parameters]
              properties:
                file_key: { type: string }
                parameters: { $ref: '#/components/schemas/JobParameters' }
                idempotency_key: { type: string }
      responses:
        '202':
          description: Job accepted
          content:
            application/json:
              schema: { $ref: '#/components/schemas/Job' }

  /jobs/{jobId}:
    get:
      summary: Get job status and results
      security: [{ bearerAuth: [] }]
      parameters:
        - name: jobId
          in: path
          required: true
          schema: { type: string, format: uuid }
      responses:
        '200':
          description: Job details
          content:
            application/json:
              schema: { $ref: '#/components/schemas/Job' }
```
