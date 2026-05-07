from fastapi import APIRouter, Depends, HTTPException, status, Query
from sqlalchemy.orm import Session
from typing import List, Optional
import uuid
from datetime import datetime

from mastering_service.core import models, schemas, security
from mastering_service.api import deps
from mastering_service.core.config import settings

router = APIRouter()

@router.post("/uploads/request", response_model=schemas.UploadRequestResponse)
def request_upload_url(
    *,
    db: Session = Depends(deps.get_db),
    current_user: models.User = Depends(deps.get_current_active_user)
):
    """
    Generate a pre-signed URL for direct S3 upload.
    In a real implementation, this would use boto3.generate_presigned_post.
    """
    file_key = f"uploads/{current_user.id}/{uuid.uuid4()}.wav"
    # Mocked URL for now
    upload_url = f"https://{settings.S3_BUCKET}.s3.amazonaws.com/{file_key}?signature=mock"
    
    return {"upload_url": upload_url, "file_key": file_key}

@router.post("/jobs", response_model=schemas.Job, status_code=status.HTTP_202_ACCEPTED)
def create_job(
    *,
    db: Session = Depends(deps.get_db),
    job_in: schemas.JobCreate,
    current_user: models.User = Depends(deps.get_current_active_user)
):
    """
    Submit a new mastering job.
    """
    # Check if file_key belongs to user (In production, verify S3 existence)
    if not job_in.file_key.startswith(f"uploads/{current_user.id}/"):
        raise HTTPException(status_code=403, detail="Invalid file key")

    job = models.Job(
        user_id=current_user.id,
        input_key=job_in.file_key,
        preset_id=job_in.parameters.preset_id,
        parameters=job_in.parameters.model_dump(),
        status=models.JobStatus.PENDING,
        source=job_in.source
    )
    db.add(job)
    db.commit()
    db.refresh(job)
    
    from mastering_service.worker.tasks import master_audio
    master_audio.delay(str(job.id))
    
    return job

@router.get("/jobs", response_model=schemas.JobList)
def list_jobs(
    db: Session = Depends(deps.get_db),
    current_user: models.User = Depends(deps.get_current_active_user),
    skip: int = 0,
    limit: int = 20,
):
    """
    Retrieve user history.
    """
    jobs = db.query(models.Job).filter(models.Job.user_id == current_user.id)\
             .offset(skip).limit(limit).all()
    total = db.query(models.Job).filter(models.Job.user_id == current_user.id).count()
    
    return {"items": jobs, "total": total}

@router.get("/jobs/{job_id}", response_model=schemas.Job)
def get_job(
    *,
    db: Session = Depends(deps.get_db),
    job_id: uuid.UUID,
    current_user: models.User = Depends(deps.get_current_active_user)
):
    """
    Get job status and metrics.
    """
    job = db.query(models.Job).filter(models.Job.id == job_id, models.Job.user_id == current_user.id).first()
    if not job:
        raise HTTPException(status_code=404, detail="Job not found")
    
    return job
