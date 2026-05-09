"""
Data models for the AI-Powered Audio Mastering Service.
"""
from datetime import datetime
from backend import db
import uuid
from werkzeug.security import generate_password_hash, check_password_hash

class User(db.Model):
    __tablename__ = 'users'

    id = db.Column(db.UUID(as_binary=False), primary_key=True, default=uuid.uuid4)
    email = db.Column(db.String(255), unique=True, nullable=False, index=True)
    fullName = db.Column(db.String(100), nullable=False)
    passwordHash = db.Column(db.String(255), nullable=False)
    credits = db.Column(db.Integer, nullable=False, default=0)
    subscriptionTier = db.Column(db.String(20), nullable=False, default='free')
    createdAt = db.Column(db.DateTime(timezone=True), nullable=False, default=datetime.utcnow)
    updatedAt = db.Column(db.DateTime(timezone=True), nullable=False, default=datetime.utcnow, onupdate=datetime.utcnow)
    isActive = db.Column(db.Boolean, nullable=False, default=True)
    lastLoginAt = db.Column(db.DateTime(timezone=True), nullable=True)

    # Relationship
    jobs = db.relationship('Job', backref='user', lazy=True, cascade='all, delete-orphan')

    def __repr__(self):
        return f'<User {self.email}>'
    
    def set_password(self, password):
        """Hash and set the user's password."""
        self.passwordHash = generate_password_hash(password, method='pbkdf2:sha256')
    
    def check_password(self, password):
        """Check if the provided password matches the stored hash."""
        return check_password_hash(self.passwordHash, password)

class Job(db.Model):
    __tablename__ = 'jobs'

    id = db.Column(db.UUID(as_binary=False), primary_key=True, default=uuid.uuid4)
    userId = db.Column(db.UUID(as_binary=False), db.ForeignKey('users.id'), nullable=False, index=True)
    fileName = db.Column(db.String(255), nullable=False)
    fileSize = db.Column(db.BigInteger, nullable=False)
    contentType = db.Column(db.String(50), nullable=False)
    presetId = db.Column(db.String(20), nullable=False)
    intensity = db.Column(db.Integer, nullable=False)
    loudnessTarget = db.Column(db.Integer, nullable=True)
    stereoWidth = db.Column(db.Integer, nullable=True)
    enableTruePeakLimiting = db.Column(db.Boolean, nullable=False, default=True)
    status = db.Column(db.String(20), nullable=False, default='pending', index=True)
    inputFileUri = db.Column(db.Text, nullable=False)
    outputFileUri = db.Column(db.Text, nullable=True)
    processing_worker_id = db.Column(db.String(100), nullable=True)
    creditsUsed = db.Column(db.Integer, nullable=True)
    errorMessage = db.Column(db.Text, nullable=True)
    createdAt = db.Column(db.DateTime(timezone=True), nullable=False, default=datetime.utcnow)
    startedAt = db.Column(db.DateTime(timezone=True), nullable=True)
    completedAt = db.Column(db.DateTime(timezone=True), nullable=True)
    loudnessMetrics = db.Column(db.JSON, nullable=True)
    sampleRate = db.Column(db.Integer, nullable=True)
    bitDepth = db.Column(db.Integer, nullable=True)

    # Indexes
    __table_args__ = (
        db.Index('idx_jobs_userId', 'userId'),
        db.Index('idx_jobs_status', 'status'),
        db.Index('idx_jobs_createdAt', 'createdAt'),
        db.Index('idx_jobs_userIdStatus', 'userId', 'status'),
        db.Index('idx_jobsUserIdCreatedAt', 'userId', 'createdAt'),
    )

    def __repr__(self):
        return f'<Job {self.id} ({self.status})>'