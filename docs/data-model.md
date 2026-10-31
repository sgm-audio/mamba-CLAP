# Proposed data model: AI-Powered Audio Mastering Service

> **Status (2026-10-04): schema proposal only.** This document is not evidence of applied migrations, an operational database, or deployed billing/subscription features. Verify the current application and migrations before relying on these entities.

## Entities

### User
Represents a registered user of the service.

**Attributes:**
- `id` (UUID): Primary key
- `email` (String, unique): User's email address (used for login)
- `fullName` (String): User's full name
- `passwordHash` (String): Bcrypt hash of user's password
- `credits` (Integer): Available processing credits
- `subscriptionTier` (Enum: free, pro, enterprise): Subscription level
- `createdAt` (Timestamp): Account creation timestamp
- `updatedAt` (Timestamp): Last profile update timestamp
- `isActive` (Boolean): Account status (active/deactivated)
- `lastLoginAt` (Timestamp): Last login timestamp (nullable)

**Indexes:**
- Primary key: `id`
- Unique key: `email`
- Index: `subscriptionTier` (for tier-based feature flags)
- Index: `createdAt` (for user growth analytics)

### Job
Represents a single audio mastering request.

**Attributes:**
- `id` (UUID): Primary key
- `userId` (UUID, foreign key): Reference to User
- `fileName` (String): Original filename
- `fileSize` (Integer): File size in bytes
- `contentType` (String): MIME type (audio/wav, audio/aiff, audio/flac)
- `presetId` (String): Mastering preset used (transparent, warm, loud)
- `intensity` (Integer): Preset intensity percentage (0-100)
- `loudnessTarget` (Integer, nullable): Target loudness in LUFS (-24 to -6)
- `stereoWidth` (Integer, nullable): Stereo width percentage (0-200)
- `enableTruePeakLimiting` (Boolean): Whether true peak limiting was applied
- `status` (Enum: pending, processing, completed, failed, cancelled): Current job status
- `inputFileUri` (String): Reference to input file in object storage
- `outputFileUri` (String, nullable): Reference to output file in object storage
- `creditsUsed` (Integer, nullable): Credits consumed for this job
- `errorMessage` (String, nullable): Error message if job failed
- `createdAt` (Timestamp): Job creation timestamp
- `startedAt` (Timestamp, nullable): When processing started
- `completedAt` (Timestamp, nullable): When processing completed
- `loudnessMetrics` (JSON, nullable): Loudness analysis results (LUFS, True Peak, LRA, Peak)

**Indexes:**
- Primary key: `id`
- Foreign key: `userId` (references User.id)
- Index: `status` (for job queue processing)
- Index: `createdAt` (for job history and analytics)
- Composite index: `(userId, status)` (for user's job listings)
- Composite index: `(userId, createdAt)` (for user's job history)

### AudioFile (Virtual Entity)
Represents audio files stored in object storage (not stored in DB directly, but tracked via URIs in Job).

**Attributes (stored in object storage metadata):**
- `uri` (String): S3-compatible URI (e.g., s3://bucket/key)
- `contentType` (String): MIME type
- `contentLength` (Integer): File size in bytes
- `etag` (String): MD5 hash of file content (for integrity)
- `cacheControl` (String): max-age=86400 (24 hours)
- `expires` (Timestamp): 24 hours from upload
- `serverSideEncryption` (String): AES256
- `metadata` (Map):
  - `originalFilename` (String): User-provided filename
  - `uploadedBy` (UUID): User ID
  - `jobId` (UUID): Associated job ID
  - `uploadTimestamp` (Timestamp): When file was uploaded
  - `fileType` (String): input or output

**Object Storage Structure:**
- `mastering-service-inputs/{userId}/{jobId}/{uuid}.{ext}` - Input files
- `mastering-service-outputs/{userId}/{jobId}/{uuid}.{ext}` - Output files
- TTL lifecycle rule: Delete objects after 24 hours

## Relationships

### User 1:N Job
- One user can have many mastering jobs
- Each job belongs to exactly one user
- Cascade delete: When a user is deleted, their jobs are also deleted (after file cleanup)
- Restrict delete: A user cannot be deleted if they have pending/processing jobs (must wait or cancel first)

### Job 1:1 AudioFile (Input)
- Each job has exactly one input audio file
- Input file exists only for the duration of the job's processing lifecycle

### Job 0:1 AudioFile (Output)
- Each job may have zero or one output audio file
- Output file exists only after successful completion and before expiration (24h)

## Data Validation Rules & Constraints

### User Constraints
- Email must be valid format (RFC 5322)
- Password must be at least 8 characters
- Full name must be 2-100 characters
- Credits cannot be negative
- Subscription tier must be one of: free, pro, enterprise

### Job Constraints
- File name must not be empty and must be ≤255 characters
- File size must be between 1 byte and 2GB (2,147,483,648 bytes)
- Content type must be one of: audio/wav, audio/aiff, audio/flac
- Preset ID must be one of: transparent, warm, loud
- Intensity must be between 0 and 100 (inclusive)
- Loudness target, if provided, must be between -24 and -6 LUFS
- Stereo width, if provided, must be between 0 and 200 percent
- At terminal states (completed/failed/cancelled), either:
  - completed: outputFileUri must not be null, creditsUsed must not be null
  - failed: errorMessage must not be null
  - cancelled: outputFileUri must be null, creditsUsed must be null

### State Transition Rules
- pending → processing: When worker picks up job from queue
- processing → completed: When worker successfully finishes processing
- processing → failed: When worker encounters unrecoverable error
- * → cancelled: When user requests cancellation (only allowed from pending/processing)
- completed/failed/cancelled → *: No further transitions allowed (terminal states)

## Migrations Strategy

### Versioning
- Use sequential version numbers (V1, V2, V3, ...)
- Each migration is both upward and downward compatible where possible
- Downward migrations (downgrades) only supported for non-destructive changes

### Migration Tools
- Use Flyway or Liquibase for database versioning
- Migrations stored in `sql/migrations/` directory
- Each migration file: `V{version}__{description}.sql`
- Example: `V1__create_initial_tables.sql`

### Sample Migration (V1)
```sql
-- V1__create_initial_tables.sql
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email VARCHAR(255) UNIQUE NOT NULL,
    fullName VARCHAR(100) NOT NULL,
    passwordHash VARCHAR(255) NOT NULL,
    credits INTEGER NOT NULL DEFAULT 0,
    subscriptionTier VARCHAR(20) NOT NULL DEFAULT 'free',
    createdAt TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updatedAt TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    isActive BOOLEAN NOT NULL DEFAULT TRUE,
    lastLoginAt TIMESTAMP WITH TIME ZONE NULL
);

CREATE TABLE jobs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    userId UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    fileName VARCHAR(255) NOT NULL,
    fileSize BIGINT NOT NULL CHECK (fileSize >= 1 AND fileSize <= 2147483648),
    contentType VARCHAR(50) NOT NULL CHECK (contentType IN ('audio/wav', 'audio/aiff', 'audio/flac')),
    presetId VARCHAR(20) NOT NULL CHECK (presetId IN ('transparent', 'warm', 'loud')),
    intensity INTEGER NOT NULL CHECK (intensity >= 0 AND intensity <= 100),
    loudnessTarget INTEGER NULL CHECK (loudnessTarget IS NULL OR (loudnessTarget >= -24 AND loudnessTarget <= -6)),
    stereoWidth INTEGER NULL CHECK (stereoWidth IS NULL OR (stereoWidth >= 0 AND stereoWidth <= 200)),
    enableTruePeakLimiting BOOLEAN NOT NULL DEFAULT TRUE,
    status VARCHAR(20) NOT NULL CHECK (status IN ('pending', 'processing', 'completed', 'failed', 'cancelled')),
    inputFileUri TEXT NOT NULL,
    outputFileUri TEXT NULL,
    creditsUsed INTEGER NULL,
    errorMessage TEXT NULL,
    createdAt TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
    startedAt TIMESTAMP WITH TIME ZONE NULL,
    completedAt TIMESTAMP WITH TIME ZONE NULL,
    loudnessMetrics JSONB NULL,

    INDEX idx_jobs_userId (userId),
    INDEX idx_jobs_status (status),
    INDEX idx_jobs_createdAt (createdAt),
    INDEX idx_jobs_userId_status (userId, status),
    INDEX idx_jobs_userIdCreatedAt (userId, createdAt)
);

-- Insert default presets as reference data (could also be in code)
INSERT INTO job_presets (id, name, description, defaultIntensity, loudnessTarget, stereoWidth, saturation)
VALUES
('transparent', 'Transparent', 'Minimal processing for clean, transparent masters', 50, -16, 100, 0),
('warm', 'Warm', 'Analog-style saturation for warmth and character', 60, -14, 100, 30),
('loud', 'Loud', 'Competitive loudness for streaming platforms', 80, -9, 100, 10);
```

### Backward Compatibility Rules
1. **Additive Changes Only**: New columns must be nullable or have defaults
2. **Never Remove Columns**: Mark as deprecated instead, remove after 2+ versions
3. **Never Change Column Types**: Add new column with new type, migrate data, then rename
4. **Stored Procedures**: Treat as code; version separately if complex logic changes
5. **Indexes**: Can be added freely; consider performance impact on writes
6. **Constraints**:
   - Can add new constraints if existing data complies
   - For restrictive constraints, migrate data first or make nullable initially

### Deployment Process
1. Apply database migrations during deployment (before new code starts)
2. Use blue/green deployment or rolling updates for zero-downtime
3. Monitor migration logs for errors
4. Have rollback plan: backup DB before migration, test downgrade in staging

## Example Payloads

### User Registration Request
```json
{
  "email": "producer@example.com",
  "password": "securePassword123!",
  "fullName": "Alex Producer"
}
```

### User Login Request
```json
{
  "email": "producer@example.com",
  "password": "securePassword123!"
}
```

### Auth Response
```json
{
  "accessToken": "<jwt>",
  "tokenType": "Bearer",
  "expiresIn": 3600,
  "user": {
    "id": "550e8400-e29b-41d4-a716-446655440000",
    "email": "producer@example.com",
    "fullName": "Alex Producer",
    "createdAt": "2023-05-01T12:34:56Z",
    "credits": 10,
    "subscriptionTier": "pro"
  }
}
```

### Job Creation Request
```json
{
  "fileName": "mymix.wav",
  "fileSize": 52428800,
  "presetId": "warm",
  "intensity": 75,
  "loudnessTarget": -14,
  "stereoWidth": 110,
  "enableTruePeakLimiting": true
}
```

### Job Response (Processing)
```json
{
  "id": "a1b2c3d4-e5f6-7890-g1h2-i3j4k5l6m7n8",
  "status": "processing",
  "presetId": "warm",
  "intensity": 75,
  "createdAt": "2023-05-01T12:34:56Z",
  "fileName": "mymix.wav",
  "fileSize": 52428800
}
```

### Job Response (Completed)
```json
{
  "id": "a1b2c3d4-e5f6-7890-g1h2-i3j4k5l6m7n8",
  "status": "completed",
  "presetId": "warm",
  "intensity": 75,
  "createdAt": "2023-05-01T12:34:56Z",
  "startedAt": "2023-05-01T12:35:10Z",
  "completedAt": "2023-05-01T12:36:20Z",
  "fileName": "mymix.wav",
  "fileSize": 52428800,
  "resultUrl": "https://audio-mastering.example.com/v1/jobs/a1b2c3d4-e5f6-7890-g1h2-i3j4k5l6m7n8/result",
  "creditsUsed": 1,
  "loudnessMetrics": {
    "integratedLUFS": -14.0,
    "truePeak": -0.8,
    "lra": 8.5,
    "peak": -1.2
  }
}
```

### Preset List Response
```json
{
  "presets": [
    {
      "id": "transparent",
      "name": "Transparent",
      "description": "Minimal processing for clean, transparent masters",
      "defaultIntensity": 50,
      "parameters": {
        "loudnessTarget": -16,
        "stereoWidth": 100,
        "saturation": 0
      }
    },
    {
      "id": "warm",
      "name": "Warm",
      "description": "Analog-style saturation for warmth and character",
      "defaultIntensity": 60,
      "parameters": {
        "loudnessTarget": -14,
        "stereoWidth": 100,
        "saturation": 30
      }
    },
    {
      "id": "loud",
      "name": "Loud",
      "description": "Competitive loudness for streaming platforms",
      "defaultIntensity": 80,
      "parameters": {
        "loudnessTarget": -9,
        "stereoWidth": 100,
        "saturation": 10
      }
    }
  ],
  "total": 3
}
```

### Health Check Response
```json
{
  "status": "healthy",
  "timestamp": "2023-05-01T12:34:56Z",
  "version": "1.0.0",
  "uptimeSeconds": 86400,
  "components": {
    "api": "healthy",
    "database": "healthy",
    "jobQueue": "healthy",
    "objectStorage": "healthy"
  }
}
```

### Error Response
```json
{
  "error": "VALIDATION_ERROR",
  "message": "File size exceeds maximum allowed size of 2GB",
  "details": {
    "field": "fileSize",
    "provided": 3221225472,
    "maximum": 2147483648
  },
  "timestamp": "2023-05-01T12:34:56Z",
  "path": "/jobs"
}
```
