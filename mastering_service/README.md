# Mamba Mastering Service

Production-level AI Audio Mastering API built with FastAPI, SQLAlchemy, and Celery.

## Features
- **Asynchronous Processing**: Decoupled API and Mastering Engine via Celery/Redis.
- **Context-Aware SSMs**: Integration with Mamba-based audio models.
- **S3-Compatible Storage**: Direct-to-cloud resumable uploads.
- **Structured Logging & Observability**: OpenTelemetry ready.

## Setup

1. **Install Dependencies**:
   ```bash
   pip install -r mastering_service/requirements.txt
   ```

2. **Environment Configuration**:
   Create a `.env` file or set environment variables:
   ```bash
   SECRET_KEY=your_secret_key
   POSTGRES_SERVER=localhost
   REDIS_HOST=localhost
   S3_BUCKET=your-bucket-name
   ```

3. **Initialize Database**:
   ```bash
   python -m mastering_service.scripts.init_db
   ```

## Running the Service

### 1. API Server
```bash
uvicorn mastering_service.main:app --host 0.0.0.0 --port 8000 --reload
```
The API documentation will be available at `http://localhost:8000/docs`.

### 2. Celery Worker
```bash
celery -A mastering_service.worker.celery_app worker --loglevel=info
```

## Testing & Linting

### Run Tests
```bash
pytest mastering_service/tests/
```

### Linting (Ruff)
```bash
ruff check mastering_service/
```

## API Summary
- `POST /v1/uploads/request`: Get pre-signed URL for audio upload.
- `POST /v1/jobs`: Submit mastering parameters and start processing.
- `GET /v1/jobs/{id}`: Poll status and retrieve results/metrics.
- `GET /v1/jobs`: List user history.
