"""
Configuration module for the AI-Powered Audio Mastering Service.
"""
import os
from datetime import timedelta

basedir = os.path.abspath(os.path.dirname(__file__))


class Config:
    """Base configuration."""
    SECRET_KEY = os.environ.get('SECRET_KEY') or 'dev-secret-key-change-in-production'
    SQLALCHEMY_DATABASE_URI = os.environ.get('DATABASE_URL') or \
        'sqlite:///' + os.path.join(basedir, 'app.db')
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    JWT_SECRET_KEY = os.environ.get('JWT_SECRET_KEY') or 'jwt-secret-string'
    JWT_ACCESS_TOKEN_EXPIRES = timedelta(hours=1)
    # Redis configuration for RQ
    REDIS_URL = os.environ.get('REDIS_URL') or 'redis://localhost:6379/0'
    # Object storage (S3) configuration
    AWS_ACCESS_KEY_ID = os.environ.get('AWS_ACCESS_KEY_ID')
    AWS_SECRET_ACCESS_KEY = os.environ.get('AWS_SECRET_ACCESS_KEY')
    AWS_STORAGE_BUCKET_NAME = os.environ.get('AWS_STORAGE_BUCKET_NAME')
    AWS_S3_REGION_NAME = os.environ.get('AWS_S3_REGION_NAME', 'us-east-1')
    # Presigned URL expiration (in seconds)
    PRESIGNED_URL_EXPIRY = int(os.environ.get('PRESIGNED_URL_EXPIRY', 900))  # 15 minutes
    # Maximum file size (2GB)
    MAX_FILE_SIZE = 2 * 1024 * 1024 * 1024
    # Allowed audio content types
    ALLOWED_CONTENT_TYPES = {'audio/wav', 'audio/aiff', 'audio/flac'}
    # Mastering presets
    PRESETS = {
        'transparent': {
            'name': 'Transparent',
            'description': 'Minimal processing for clean, transparent masters',
            'defaultIntensity': 50,
            'parameters': {
                'loudnessTarget': -16,
                'stereoWidth': 100,
                'saturation': 0
            }
        },
        'warm': {
            'name': 'Warm',
            'description': 'Analog-style saturation for warmth and character',
            'defaultIntensity': 60,
            'parameters': {
                'loudnessTarget': -14,
                'stereoWidth': 100,
                'saturation': 30
            }
        },
        'loud': {
            'name': 'Loud',
            'description': 'Competitive loudness for streaming platforms',
            'defaultIntensity': 80,
            'parameters': {
                'loudnessTarget': -9,
                'stereoWidth': 100,
                'saturation': 10
            }
        }
    }


class DevelopmentConfig(Config):
    """Development configuration."""
    DEBUG = True
    SQLALCHEMY_ECHO = True


class TestingConfig(Config):
    """Testing configuration."""
    TESTING = True
    SQLALCHEMY_DATABASE_URI = 'sqlite:///:memory:'
    JWT_ACCESS_TOKEN_EXPIRES = timedelta(seconds=5)  # Short expiry for tests
    PRESIGNED_URL_EXPIRY = 10  # Short expiry for tests


class ProductionConfig(Config):
    """Production configuration."""
    DEBUG = False
    # Use environment variables for sensitive data
    SECRET_KEY = os.environ.get('SECRET_KEY')
    JWT_SECRET_KEY = os.environ.get('JWT_SECRET_KEY')
    # Ensure these are set in production
    SQLALCHEMY_DATABASE_URI = os.environ.get('DATABASE_URL')
    REDIS_URL = os.environ.get('REDIS_URL')


config_dict = {
    'development': DevelopmentConfig,
    'testing': TestingConfig,
    'production': ProductionConfig,
    'default': DevelopmentConfig
}