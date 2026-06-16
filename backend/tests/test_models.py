"""
Unit tests for the data models.
"""

import uuid
from datetime import datetime, timezone

from backend.models import Job, User


def test_user_creation(db):
    """Test creating a user."""
    user = User(
        email="test@example.com",
        fullName="Test User",
        passwordHash="hashed_password",
        credits=5,
        subscriptionTier="pro",
    )
    db.session.add(user)
    db.session.commit()

    # Retrieve the user and check its attributes
    fetched_user = User.query.filter_by(email="test@example.com").first()
    assert fetched_user is not None
    assert fetched_user.email == "test@example.com"
    assert fetched_user.fullName == "Test User"
    assert fetched_user.credits == 5
    assert fetched_user.subscriptionTier == "pro"
    assert fetched_user.isActive is True  # Default value
    assert isinstance(fetched_user.createdAt, datetime)
    # The createdAt should be set to the current time (within a few seconds)
    assert (datetime.now(timezone.utc) - fetched_user.createdAt).total_seconds() < 5


def test_job_creation(db):
    """Test creating a job."""
    # First, create a user
    user = User(
        email="user@example.com", fullName="Job User", passwordHash="hashed_password", credits=10
    )
    db.session.add(user)
    db.session.commit()

    # Now create a job for that user
    job_id = uuid.uuid4()
    job = Job(
        id=job_id,
        userId=user.id,
        fileName="test.wav",
        fileSize=1024,
        contentType="audio/wav",
        presetId="transparent",
        intensity=50,
        loudnessTarget=-16,
        stereoWidth=100,
        enableTruePeakLimiting=True,
        status="pending",
        inputFileUri="s3://bucket/input.wav",
    )
    db.session.add(job)
    db.session.commit()

    # Retrieve the job and check its attributes
    fetched_job = Job.query.get(job_id)
    assert fetched_job is not None
    assert fetched_job.fileName == "test.wav"
    assert fetched_job.fileSize == 1024
    assert fetched_job.contentType == "audio/wav"
    assert fetched_job.presetId == "transparent"
    assert fetched_job.intensity == 50
    assert fetched_job.loudnessTarget == -16
    assert fetched_job.stereoWidth == 100
    assert fetched_job.enableTruePeakLimiting is True
    assert fetched_job.status == "pending"
    assert fetched_job.inputFileUri == "s3://bucket/input.wav"
    assert fetched_job.userId == user.id
    assert fetched_job.user.email == "user@example.com"
