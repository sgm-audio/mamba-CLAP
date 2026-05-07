from pydantic import BaseModel, EmailStr, HttpUrl, Field
from typing import Optional, List, Any
import uuid
from datetime import datetime
from mastering_service.core.models import JobStatus

# Shared
class JobParameters(BaseModel):
    preset_id: str
    intensity: int = Field(50, ge=0, le=100)
    loudness_target: float = Field(-14.0, ge=-24.0, le=-6.0)
    stereo_width: float = Field(100.0, ge=0.0, le=200.0)

class JobMetrics(BaseModel):
    integrated_lufs: Optional[float] = None
    true_peak_db: Optional[float] = None
    lra: Optional[float] = None

# Job Create
class JobCreate(BaseModel):
    file_key: str
    parameters: JobParameters
    source: str = "web"
    idempotency_key: Optional[str] = None

# Job Response
class Job(BaseModel):
    id: uuid.UUID
    status: JobStatus
    preset_id: str
    parameters: JobParameters
    metrics: Optional[JobMetrics] = None
    output_url: Optional[str] = None
    expires_at: datetime
    created_at: datetime

    class Config:
        from_attributes = True

class JobList(BaseModel):
    items: List[Job]
    total: int

class UploadRequestResponse(BaseModel):
    upload_url: str
    file_key: str

# Auth
class Token(BaseModel):
    access_token: str
    token_type: str

class TokenPayload(BaseModel):
    sub: Optional[str] = None
