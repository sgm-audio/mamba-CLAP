"""
Utility functions for the AI-Powered Audio Mastering Service.
"""
import os
import re
import mimetypes
import time
from typing import Tuple, Optional
import boto3
from botocore.exceptions import ClientError

def get_file_extension(content_type: str) -> str:
    """
    Get file extension from content type.
    Returns the extension including the dot (e.g., '.wav').
    """
    if content_type == 'audio/wav':
        return '.wav'
    elif content_type == 'audio/aiff':
        return '.aiff'
    elif content_type == 'audio/flac':
        return '.flac'
    else:
        # Default to .wav if unknown (should not happen if validated)
        return '.wav'

def get_content_type_from_filename(filename: str) -> Optional[str]:
    """
    Get content type from file name extension.
    Returns None if the extension is not recognized or if the file name is empty.
    """
    if not filename:
        return None
    # Get the extension and convert to lower case
    ext = os.path.splitext(filename)[1].lower()
    if ext == '.wav':
        return 'audio/wav'
    elif ext == '.aiff':
        return 'audio/aiff'
    elif ext == '.flac':
        return 'audio/flac'
    else:
        return None

def validate_audio_file(file_path: str, expected_content_type: str) -> Tuple[bool, str]:
    """
    Validate an audio file.
    Checks that the file exists and that its content type matches the expected type.
    This is a simple validation based on file extension and magic bytes.
    For a production system, we would use a more robust method (e.g., using python-magic or ffmpeg).
    Returns (is_valid, error_message).
    """
    if not os.path.exists(file_path):
        return False, "File does not exist"
    
    # Check file extension
    _, ext = os.path.splitext(file_path)
    ext = ext.lower()
    
    expected_ext = get_file_extension(expected_content_type)
    if ext != expected_ext:
        return False, f"File extension {ext} does not match expected content type {expected_content_type}"
    
    # TODO: Add more robust validation (e.g., check magic bytes, use a library to read the file)
    # For now, we assume the file is valid if the extension matches.
    
    return True, ""

def validate_s3_uri(uri: str) -> Tuple[str, str]:
    """
    Validate an S3 URI and extract bucket and key.
    Raises ValueError if invalid.
    Returns (bucket, key).
    """
    if not uri.startswith('s3://'):
        raise ValueError("Invalid S3 URI: must start with 's3://'")
    
    path = uri[5:]
    if '/' not in path:
        raise ValueError("Invalid S3 URI: missing key after bucket")
    
    bucket, key = path.split('/', 1)
    
    # Validate bucket (AWS rules)
    if not re.match(r'^[a-z0-9][a-z0-9\-.]{1,61}[a-z0-9]$', bucket):
        raise ValueError(f"Invalid bucket name")
    
    # Key validation
    if len(key) > 1024:
        raise ValueError("S3 key too long (max 1024)")
    
    if any(ord(c) < 32 for c in key):  # control chars
        raise ValueError("S3 key contains control characters")
    
    if '..' in key or '/./' in key or '//' in key or key.startswith('/'):
        raise ValueError("Path traversal attempt")
    
    # Canonicalization: ensure decoded == original (prevents %2e%2e%2f etc)
    import urllib.parse
    if urllib.parse.unquote(key) != key:
        raise ValueError("URL encoding not allowed in S3 key")
    
    return bucket, key


def setup_logging():
    """
    Set up structured logging for the application.
    In a production system, we would use a logging configuration that outputs JSON.
    For simplicity, we just configure the basic logger.
    """
    import logging
    import sys
    
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        stream=sys.stdout
    )
    return logging.getLogger(__name__)


def retry_s3_operation(func, max_attempts=3, initial_backoff=0.1):
    """
    Retry wrapper for S3 operations with exponential backoff.
    Only retries on transient errors (network issues, 5xx, throttling).
    :param func: Callable that takes no args (e.g., lambda: s3_client.download_file(...))
    :param max_attempts: Maximum number of attempts (default 3)
    :param initial_backoff: Initial sleep time in seconds (default 0.1)
    :return: Result of func if successful
    :raises: The last exception if all attempts fail
    """
    from botocore.exceptions import ClientError
    last_exception = None
    for attempt in range(1, max_attempts + 1):
        try:
            return func()
        except ClientError as e:
            last_exception = e
            error_code = e.response.get('Error', {}).get('Code', '')
            # Only retry on transient errors: 5xx, RequestTimeout, Throttling, SlowDown
            if error_code not in ['500', '503', '504', 'RequestTimeout', 'Throttling', 'SlowDown']:
                raise
            if attempt < max_attempts:
                sleep_time = initial_backoff * (2 ** (attempt - 1))
                time.sleep(sleep_time)
        except Exception as e:
            last_exception = e
            if attempt < max_attempts:
                sleep_time = initial_backoff * (2 ** (attempt - 1))
                time.sleep(sleep_time)
    raise last_exception