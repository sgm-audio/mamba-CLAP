# Test Plan: AI-Powered Audio Mastering Service

## Test Strategy

### Unit Testing
- **Scope**: Individual components (models, utility functions, worker tasks) in isolation.
- **Tools**: 
  - pytest for test framework
  - unittest.mock for patching external dependencies (boto3, redis, database)
  - Factory Boy or manual object creation for test data
- **Coverage Goal**: ≥80% for critical paths (job lifecycle, file validation, authentication)
- **Frequency**: Run on every commit

### Integration Testing
- **Scope**: API endpoint interactions, job processing flow, database-external service interactions
- **Tools**:
  - pytest with Flask test client for API endpoints
  - moto for mocking AWS S3 interactions
  - redis-py with fakeredis or mock for Redis interactions
  - Test PostgreSQL database (using sqlite in-memory for unit, but PostgreSQL for integration if possible)
- **Focus Areas**:
  - Job creation → file upload (simulated) → processing → result retrieval
  - Error propagation and handling
  - State transitions in job lifecycle
  - Authentication and authorization
- **Frequency**: Run on every commit

### End-to-End Testing
- **Scope**: Critical user journeys (web interface and ARA plugin simulation)
- **Tools**:
  - For API: Cypress or Selenium for web UI tests (future)
  - For ARA plugin: Custom test harness simulating DAW interactions (future)
  - Contract testing between API and plugin (using the OpenAPI spec)
- **Focus Areas**:
  - Complete mastering flow via API (simulating web client)
  - ARA plugin round-trip (simulated)
  - File handling and metadata preservation
- **Frequency**: Nightly or pre-release

### Performance Testing
- **Scope**: Validate NFRs for processing speed and resource utilization
- **Tools**:
  - Locust or k6 for load testing API endpoints
  - Custom benchmarks for worker processing (with actual model)
  - GPU monitoring tools (nvidia-smi, pynvml) for utilization
- **Metrics**:
  - Job processing latency (p50, p95, p99)
  - GPU utilization during processing
  - Throughput (jobs/hour)
  - Error rates under load
- **Frequency**: Weekly or before major releases

### Security Testing
- **Scope**: Authentication, authorization, data protection, input validation
- **Tools**:
  - OWASP ZAP for automated scanning
  - Custom tests for injection flaws
  - Manual review for logic flaws
- **Focus Areas**:
  - JWT token handling
  - Input sanitization (file names, parameters)
  - Permission checks (user isolation)
  - Secure deletion of temporary files
- **Frequency**: Before major releases and after security-related changes

## Test Data and Environment Needs

### Test Data
- **Audio Files**:
  - Silent files (for edge case testing)
  - Clipped files (for declipping testing)
  - Professional reference files (for quality testing)
  - Various formats (WAV, AIFF, FLAC) and bit depths (16-bit, 24-bit)
  - Various sample rates (44.1kHz, 48kHz, 88.2kHz, 96kHz)
  - Frequency sweeps and impulse responses (for THD testing)
- **Metadata Files**:
  - ID3 tags, BWF chunks (for preservation testing)
- **Test User Accounts**:
  - Various subscription tiers (free, pro, enterprise)
  - Various credit balances

### Environment
- **Testing Environment**:
  - Isolated from production
  - Separate database instance
  - Mocked or isolated external services (S3, Redis)
  - GPU instance for performance testing (optional for unit/integration tests)
- **Dependencies**:
  - Python 3.8+
  - Redis 6+
  - PostgreSQL 12+ (or compatible)
  - AWS CLI compatible endpoint (for S3 mocking)
  - FFmpeg or SoX for generating test audio (optional)

## Traceability Matrix: PRD Acceptance Criteria to Test Cases

| PRD Acceptance Criterion | Test Case ID | Test Type | Verification Method |
|--------------------------|--------------|-----------|---------------------|
| **File Handling** |
| FR1.1: Accept WAV, AIFF, FLAC (8-24 bit, 44.1-192kHz) | TC-FH-001 | Unit/Integration | Upload valid files of each type/bit depth/rate, verify job creation succeeds |
| FR1.2: Output matches input bit depth/sample rate | TC-FH-002 | Integration | Process file, verify output file properties match input |
| FR1.3: Reject files >2GB or >2 hour duration | TC-FH-003 | Unit/Integration | Attempt upload oversized file, verify 413 error |
| FR1.4: Preserve metadata (ID3, BWF) | TC-FH-004 | Integration | Upload file with metadata, process, verify metadata in output |
| **Mastering Engine** |
| FR2.1: ≥3 distinct presets with adjustable intensity | TC-ME-001 | Unit | Verify preset configuration in API response |
| FR2.2: Manual control of loudness, EQ, width, saturation | TC-ME-002 | Unit/Integration | Set parameters via API, verify they affect processing |
| FR2.3: Apply true peak limiting | TC-ME-003 | Integration | Process file with peaks >0 dBTP, verify output true peak ≤ -0.5 dBTP |
| FR2.4: Output loudness within ±0.5 LUFS of target | TC-ME-004 | Integration | Set loudness target, process, measure output LUFS |
| FR2.5: <0.5% THD increase for "Transparent" preset | TC-ME-005 | Integration | Process sine wave, measure THD before/after |
| **User Interface** |
| FR3.1: Real-time waveform display | TC-UI-001 | E2E | Verify frontend shows updating waveform during processing |
| FR3.2: A/B comparison with zero-latency switching | TC-UI-002 | E2E | Verify frontend can toggle between original and processed audio |
| FR3.3: Display real-time loudness metrics | TC-UI-003 | E2E | Verify loudness metrics update during processing |
| FR3.4: Undo/redo of parameter changes | TC-UI-004 | E2E | Verify parameter changes can be reversed |
| FR3.5: Visual clipping warnings | TC-UI-005 | E2E | Verify clipping warnings appear when appropriate |
| **Integration & Export** |
| FR4.1: ARA2 plugin for DAW integration | TC-IE-001 | E2E | Test plugin round-trip with DAW (Reaper/Cubase) |
| FR4.2: Direct export to cloud storage | TC-IE-002 | Integration | Verify saved files appear in connected cloud storage |
| FR4.3: Secure, time-limited download links | TC-IE-003 | Unit/Integration | Verify result URL expires and is unguessable |
| FR4.4: Batch processing (≥5 files) | TC-IE-004 | Integration | Submit 5+ jobs, verify all process correctly |
| **Analysis & Feedback** |
| FR5.1: Frequency spectrum comparison | TC-AF-001 | E2E | Verify spectrum display shows input vs output |
| FR5.2: Suggest optimal loudness target | TC-AF-002 | Unit | Verify API returns loudness suggestion based on analysis |
| FR5.3: Warn about potential issues | TC-AF-003 | Integration | Upload file with known issues (sibilance, muddiness), verify warnings |
| FR5.4: Genre-specific reference matching | TC-AF-004 | E2E | Test reference matching feature with genre references |
| **Performance (NFRs)** |
| NFR1: Process 3-min stereo track in ≤90 seconds | TC-PERF-001 | Performance | Time processing of 3-min file on standard GPU instance |
| NFR2: Support ≥50 concurrent jobs | TC-PERF-002 | Load Test | Ramp up to 50+ concurrent jobs, verify system stability |
| NFR3: First byte within 5 seconds | TC-PERF-003 | Performance | Measure time to first byte of streamed response |
| **Reliability (NFRs)** |
| NFR4: 99.5% monthly uptime SLA | TC-REL-001 | Monitoring | Track uptime over month via health checks |
| NFR5: Automatic retry of failed jobs (up to 2x) | TC-REL-002 | Integration | Inject transient failure, verify job retries and succeeds |
| NFR6: Preserve user progress during network issues | TC-REL-003 | Integration | Simulate network interruption during processing, verify job can resume |
| **Security & Privacy (NFRs)** |
| NFR7: Encrypt audio transfers/storage (AES-256) | TC-SEC-001 | Security | Verify network traffic is encrypted (TLS), storage encryption enabled |
| NFR8: Auto-delete user audio after 24h | TC-SEC-002 | Integration | Upload file, wait 25h, verify file is deleted from storage |
| NFR9: No user audio for model training without opt-in | TC-SEC-003 | Audit | Verify training data excludes non-consented user audio |
| NFR10: GDPR/CCPA/COPPA compliance | TC-SEC-004 | Legal Review | Conduct compliance audit |
| **Compatibility (NFRs)** |
| NFR11: Support Chrome, Firefox, Safari, Edge (last 2 versions) | TC-COM-001 | Compatibility | Test web UI in each browser |
| NFR12: ARA plugin supports VST3, AU, AAX | TC-COM-002 | Compatibility | Test plugin in DAWs supporting each format |
| NFR13: Normalize file names to prevent special char issues | TC-COM-003 | Unit | Upload files with problematic names, verify safe handling |

## Acceptance Criteria Test Mapping (Selected Critical Path)

### Given/When/Then Format for Key Scenarios

**Scenario: Successful Mastering Job Flow**
```
Given a registered user with sufficient credits
When they upload a valid WAV file and create a mastering job with preset "Warm" at intensity 75
Then the system shall:
  - Accept the job and return a job ID
  - Generate a presigned upload URL for the input file
  - Upon file upload, transition job to "processing" state
  - Process the file using the Mamba-based model
  - Upon completion, transition job to "completed" state
  - Store the output file in object storage
  - Return a presigned download URL for the mastered file
  - Deduct one credit from the user's account
  - Provide loudness metrics (Integrated LUFS, True Peak, LRA, Peak)
```

**Scenario: Invalid File Upload**
```
Given a registered user
When they attempt to upload a file exceeding 2GB
Then the system shall:
  - Reject the upload request with HTTP 413
  - Not create a job
  - Not deduct any credits
  - Return an error message indicating file size limit exceeded
```

**Scenario: Near-Silent Input (Edge Case)**
```
Given a registered user
When they upload a file with -60 dBFS RMS (near silence)
And process with any preset
Then the system shall:
  - Apply ≤2% processing intensity
  - Generate a warning about low signal-to-noise ratio
  - Output silence with appropriate dithering
  - Not introduce audible noise or pumping artifacts
```

**Scenario: Clipped Input (Edge Case)**
```
Given a registered user
When they upload a file peaking at +3 dBFS (clipped)
And processing begins
Then the system shall:
  - Detect and report clipping during validation
  - Apply declipping algorithm if enabled in settings
  - Limit further processing to prevent audible artifacts
  - If declipping is disabled, process with caution and warn user
```

## Automated Test Implementations

### Unit Test: File Validation (backend/tests/test_utils.py)
```python
import pytest
from backend.utils import validate_audio_file, get_content_type_from_filename
import os
import tempfile

def test_validate_audio_file_valid_wav():
    """Test validation of a valid WAV file."""
    # Create a temporary WAV file (minimal valid WAV header)
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
        # Write minimal RIFF WAV header (44 bytes)
        wav_header = b'RIFF$\x00\x00\x00WAVEfmt \x10\x00\x00\x00\x01\x00\x01\x00'
        wav_header += b'\x44\xac\x00\x00\x88\x58\x01\x00\x02\x00\x10\x00data\x00\x00\x00\x00'
        f.write(wav_header)
        f.flush()
        
        is_valid, msg = validate_audio_file(f.name, 'audio/wav')
        assert is_valid == True
        assert msg == ""
        
        os.unlink(f.name)

def test_validate_audio_file_invalid_extension():
    """Test validation rejects wrong extension."""
    with tempfile.NamedTemporaryFile(suffix='.txt', delete=False) as f:
        f.write(b'not audio')
        f.flush()
        
        is_valid, msg = validate_audio_file(f.name, 'audio/wav')
        assert is_valid == False
        assert "File extension" in msg
        
        os.unlink(f.name)

def test_get_content_type_from_filename():
    """Test content type detection from file name."""
    assert get_content_type_from_filename('test.wav') == 'audio/wav'
    assert get_content_type_from_filename('test.aiff') == 'audio/aiff'
    assert get_content_type_from_filename('test.flac') == 'audio/flac'
    assert get_content_type_from_filename('test.mp3') is None
    assert get_content_type_from_filename('test') is None
    assert get_content_type_from_filename('') is None
```

### Integration Test: Job Creation and Processing (backend/tests/test_api.py)
```python
import pytest
import json
import os
import uuid
from datetime import datetime, timedelta
from unittest.mock import patch, MagicMock
from flask_jwt_extended import create_access_token

# These would be in the actual test file with proper imports
# For brevity, we're showing the test structure

def test_create_job_success(client, app, db, s3):
    """Test successful job creation."""
    # Set up test user
    from backend.models import User
    from werkzeug.security import generate_password_hash
    
    test_user = User(
        email='test@example.com',
        fullName='Test User',
        passwordHash=generate_password_hash('password123'),
        credits=10
    )
    db.session.add(test_user)
    db.session.commit()
    
    # Get auth token
    access_token = create_access_token(identity=test_user.id)
    headers = {'Authorization': f'Bearer {access_token}'}
    
    # Job creation request
    job_data = {
        'fileName': 'test.wav',
        'fileSize': 1024000,  # ~1MB
        'presetId': 'warm',
        'contentType': 'audio/wav',
        'intensity': 75,
        'loudnessTarget': -14,
        'stereoWidth': 110,
        'enableTruePeakLimiting': True
    }
    
    response = client.post('/api/v1/jobs', 
                          data=json.dumps(job_data),
                          content_type='application/json',
                          headers=headers)
    
    assert response.status_code == 202
    data = json.loads(response.data)
    assert 'id' in data
    job_id = data['id']
    assert data['status'] == 'pending'
    assert data['fileName'] == 'test.wav'
    
    # Verify job in database
    from backend.models import Job
    job = Job.query.get(uuid.UUID(job_id))
    assert job is not None
    assert job.fileName == 'test.wav'
    assert job.status == 'pending'
    assert job.inputFileUri is not None
    assert job.outputFileUri is None
    
    # Verify credit was deducted
    user = User.query.get(test_user.id)
    assert user.credits == 9

def test_get_job_result_after_processing(client, app, db, s3):
    """Test retrieving job result after processing (mocked)."""
    # Set up test user and job (similar to above)
    from backend.models import User, Job
    from werkzeug.security import generate_password_hash
    import tempfile
    
    test_user = User(
        email='test2@example.com',
        fullName='Test User 2',
        passwordHash=generate_password_hash('password123'),
        credits=10
    )
    db.session.add(test_user)
    db.session.commit()
    
    access_token = create_access_token(identity=test_user.id)
    headers = {'Authorization': f'Bearer {access_token}'}
    
    # Create job
    job_data = {
        'fileName': 'test.wav',
        'fileSize': 1024000,
        'presetId': 'transparent',
        'contentType': 'audio/wav'
    }
    
    response = client.post('/api/v1/jobs', 
                          data=json.dumps(job_data),
                          content_type='application/json',
                          headers=headers)
    
    assert response.status_code == 202
    job_id = json.loads(response.data)['id']
    
    # Mock the worker processing by directly updating the job
    # In a real test, we would trigger the worker and wait, but for unit test we simulate
    with app.app_context():
        job = Job.query.get(uuid.UUID(job_id))
        job.status = 'completed'
        job.startedAt = datetime.utcnow() - timedelta(seconds=10)
        job.completedAt = datetime.utcnow()
        job.outputFileUri = f's3://{os.environ["AWS_STORAGE_BUCKET_NAME"]}/mastering-service-outputs/{test_user.id}/{job.id}/{uuid.uuid4().hex}.wav'
        job.loudnessMetrics = {
            'integratedLUFS': -14.0,
            'truePeak': -0.8,
            'lra': 8.5,
            'peak': -1.2
        }
        job.sampleRate = 44100
        job.bitDepth = 24
        db.session.commit()
    
    # Get job result
    response = client.get(f'/api/v1/jobs/{job_id}/result', headers=headers)
    
    assert response.status_code == 200
    data = json.loads(response.data)
    assert 'downloadUrl' in data
    assert data['expiresIn'] > 0
    assert data['fileName'] == 'mastered_test.wav'
    assert data['format'] == 'wav'
    assert data['sampleRate'] == 44100
    assert data['bitDepth'] == 24
    assert data['loudnessMetrics']['integratedLUFS'] == -14.0
    assert data['loudnessMetrics']['truePeak'] == -0.8
```

### Integration Test: Edge Case - Near Silent Input (backend/tests/test_api.py)
```python
def test_job_near_silent_input(client, app, db, s3):
    """Test processing of near-silent audio file."""
    # Set up test user
    from backend.models import User
    from werkzeug.security import generate_password_hash
    
    test_user = User(
        email='test3@example.com',
        fullName='Test User 3',
        passwordHash=generate_password_hash('password123'),
        credits=10
    )
    db.session.add(test_user)
    db.session.commit()
    
    access_token = create_access_token(identity=test_user.id)
    headers = {'Authorization': f'Bearer {access_token}'}
    
    # Create job with near-silent file (we'll simulate by creating a job with
    # a file that we know is silent - in reality we'd upload an actual silent file)
    job_data = {
        'fileName': 'silent.wav',
        'fileSize': 1024000,  # 1MB of silence
        'presetId': 'warm',
        'contentType': 'audio/wav',
        'intensity': 50
    }
    
    response = client.post('/api/v1/jobs', 
                          data=json.dumps(job_data),
                          content_type='application/json',
                          headers=headers)
    
    assert response.status_code == 202
    job_id = json.loads(response.data)['id']
    
    # Simulate worker processing for near-silent input
    # In a real implementation, the worker would detect low RMS and apply minimal processing
    with app.app_context():
        job = Job.query.get(uuid.UUID(job_id))
        # Simulate the worker's detection and processing
        job.status = 'completed'
        job.startedAt = datetime.utcnow() - timedelta(seconds=5)
        job.completedAt = datetime.utcnow()
        job.outputFileUri = f's3://{os.environ["AWS_STORAGE_BUCKET_NAME"]}/mastering-service-outputs/{test_user.id}/{job.id}/{uuid.uuid4().hex}.wav'
        # For near-silent input, we expect minimal processing
        job.loudnessMetrics = {
            'integratedLUFS': -60.0,  # Still very quiet
            'truePeak': -60.0,
            'lra': 0.1,  # Very small loudness range
            'peak': -60.0
        }
        job.sampleRate = 44100
        job.bitDepth = 16
        db.session.commit()
    
    # Get job result
    response = client.get(f'/api/v1/jobs/{job_id}/result', headers=headers)
    
    assert response.status_code == 200
    data = json.loads(response.data)
    # Verify minimal processing was applied (in a real test, we'd check the audio)
    # For now, we verify the job completed and we got a result
    assert data['status'] == 'completed'
```

## Load Test Plan (Conceptual)
```
# Using Locust to test concurrent job creation and processing
from locust import HttpUser, task, between

class MasteringUser(HttpUser):
    wait_time = between(1, 3)
    
    def on_start(self):
        """Log in before starting tasks"""
        response = self.client.post("/api/v1/auth/login", 
                                   json={"email": "test@example.com", "password": "password"})
        self.access_token = response.json()["accessToken"]
        self.headers = {"Authorization": f"Bearer {self.access_token}"}
    
    @task(3)
    def create_and_check_job(self):
        """Create a job and periodically check its status"""
        # Create job
        job_data = {
            "fileName": "test.wav",
            "fileSize": 5242880,  # 5MB
            "presetId": "transparent",
            "contentType": "audio/wav"
        }
        response = self.client.post("/api/v1/jobs",
                                   json=job_data,
                                   headers=self.headers)
        job_id = response.json()["id"]
        
        # Check status a few times
        for _ in range(3):
            self.client.get(f"/api/v1/jobs/{job_id}", headers=self.headers)
            # In a real test, we would wait for completion, but for load test
            # we just check status a few times to simulate user behavior
    
    @task(1)
    def get_presets(self):
        """Occasionally check available presets"""
        self.client.get("/api/v1/presets", headers=self.headers)
```

## Test Environment Setup Instructions

1. **Install Test Dependencies**:
   ```bash
   pip install -r requirements.txt
   pip install pytest moto fakeredis locust  # For testing
   ```

2. **Set Environment Variables for Testing**:
   ```bash
   export FLASK_ENV=testing
   export SECRET_KEY=test-secret-key
   export JWT_SECRET_KEY=test-jwt-secret
   export DATABASE_URL=sqlite:///:memory:  # For unit tests
   export REDIS_URL=redis://localhost:6379/0
   export AWS_ACCESS_KEY_ID=testing
   export AWS_SECRET_ACCESS_KEY=testing
   export AWS_STORAGE_BUCKET_NAME=test-bucket
   export AWS_S3_REGION_NAME=us-east-1
   export PRESIGNED_URL_EXPIRY=900
   ```

3. **Run Tests**:
   ```bash
   # Unit and integration tests
   pytest backend/tests/ -v
   
   # With coverage
   pytest --cov=backend --cov-report=html backend/tests/
   
   # Load test (example)
   locust -f locustfile.py --host=http://localhost:5000
   ```

## Conclusion

This test plan ensures that all PRD acceptance criteria have corresponding test strategies, with critical paths covered by automated unit and integration tests. Performance, security, and compatibility testing are addressed through dedicated test types. The traceability matrix provides clear mapping from requirements to test cases, enabling verification that the implementation meets all specified requirements.

As the implementation evolves, this test plan should be updated to reflect new features and changing requirements, maintaining a strong verification foundation for the AI-Powered Audio Mastering Service.