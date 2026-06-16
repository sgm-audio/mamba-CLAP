import enum
import uuid
from datetime import datetime, timedelta

from sqlalchemy import JSON, Column, DateTime
from sqlalchemy import Enum as SQLEnum
from sqlalchemy import ForeignKey, String, Uuid
from sqlalchemy.orm import relationship

from mastering_service.core.database import Base


class JobStatus(str, enum.Enum):
    PENDING = "PENDING"
    PROCESSING = "PROCESSING"
    COMPLETED = "COMPLETED"
    FAILED = "FAILED"


class User(Base):
    __tablename__ = "users"

    id = Column(Uuid(as_uuid=True), primary_key=True, default=uuid.uuid4)
    email = Column(String(255), unique=True, nullable=False, index=True)
    password_hash = Column(String, nullable=False)
    tier = Column(String(50), nullable=False, default="free")
    created_at = Column(DateTime, default=datetime.utcnow)

    jobs = relationship("Job", back_populates="user")


class Preset(Base):
    __tablename__ = "presets"

    id = Column(String(50), primary_key=True)
    name = Column(String(100), nullable=False)
    description = Column(String)
    config_schema = Column(JSON, nullable=False)


class Job(Base):
    __tablename__ = "jobs"

    id = Column(Uuid(as_uuid=True), primary_key=True, default=uuid.uuid4)
    user_id = Column(Uuid(as_uuid=True), ForeignKey("users.id"), nullable=False)
    input_key = Column(String, nullable=False)
    output_key = Column(String)
    preset_id = Column(String(50), ForeignKey("presets.id"))
    parameters = Column(JSON, nullable=False)
    status = Column(SQLEnum(JobStatus), nullable=False, default=JobStatus.PENDING)
    metrics = Column(JSON)
    error_info = Column(JSON)
    source = Column(String(20), nullable=False, default="web")
    created_at = Column(DateTime, default=datetime.utcnow)
    expires_at = Column(
        DateTime, nullable=False, default=lambda: datetime.utcnow() + timedelta(hours=24)
    )

    user = relationship("User", back_populates="jobs")
