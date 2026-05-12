"""
Utility functions for the AI-Powered Audio Mastering Service.
"""
import os
import re
import mimetypes
import time
import threading
from typing import Tuple, Optional, Callable, Any
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
    
    # Check magic bytes for more robust validation
    try:
        with open(file_path, 'rb') as f:
            header = f.read(12)  # Read enough bytes to check common audio formats
            
        # WAV file header: 'RIFF' + file size + 'WAVE'
        if expected_content_type == 'audio/wav':
            if len(header) >= 12 and header[0:4] == b'RIFF' and header[8:12] == b'WAVE':
                pass  # Valid WAV
            else:
                return False, "Invalid WAV file: missing RIFF/WAVE header"
                
        # AIFF file header: 'FORM' + file size + 'AIFF' or 'AIFC'
        elif expected_content_type == 'audio/aiff':
            if len(header) >= 12 and header[0:4] == b'FORM' and (header[8:12] == b'AIFF' or header[8:12] == b'AIFC'):
                pass  # Valid AIFF/AIFC
            else:
                return False, "Invalid AIFF file: missing FORM/AIFF or FORM/AIFC header"
                
        # FLAC file header: 'fLaC' + metadata
        elif expected_content_type == 'audio/flac':
            if len(header) >= 4 and header[0:4] == b'fLaC':
                pass  # Valid FLAC
            else:
                return False, "Invalid FLAC file: missing fLaC header"
    except IOError:
        return False, "Could not read file header for validation"
    
    return True, ""

def validate_s3_uri(uri: str) -> Tuple[str, str]:
    """
    Validate an S3 URI and extract bucket and key.
    Raises ValueError if invalid.
    Returns (bucket, key).
    """
    if not uri or not isinstance(uri, str):
        raise ValueError("S3 URI must be a non-empty string")

    if not uri.startswith('s3://'):
        raise ValueError("Invalid S3 URI: must start with 's3://'")

    path = uri[5:]
    if not path or '/' not in path:
        raise ValueError("Invalid S3 URI: missing key after bucket")

    bucket, key = path.split('/', 1)

    # Validate bucket name (AWS S3 bucket naming rules)
    # Must be 3-63 chars, lowercase, start/end with letter or number, can contain -. .
    if not bucket or len(bucket) < 3 or len(bucket) > 63:
        raise ValueError(f"Invalid bucket name: '{bucket}' - must be 3-63 characters")
    if not re.match(r'^[a-z0-9][a-z0-9\-.]{1,61}[a-z0-9]$', bucket):
        raise ValueError(f"Invalid bucket name: '{bucket}' - does not match AWS S3 bucket naming rules")

    # Key validation
    if not key:
        raise ValueError("S3 key cannot be empty")

    if len(key) > 1024:
        raise ValueError("S3 key too long (max 1024 characters)")

    # Reject keys with problematic patterns
    if key.endswith('/'):
        raise ValueError("S3 key cannot end with '/'")

    # Check for control characters (including null byte)
    if '\x00' in key:
        raise ValueError("S3 key contains null byte")
    if any(ord(c) < 32 for c in key):
        raise ValueError("S3 key contains control characters")

    # Path traversal and injection prevention
    # Check for encoded versions first (before decoding)
    import urllib.parse
    if any(encoded in key.lower() for encoded in ['%2e%2e', '%2f', '%5c', '%00']):
        raise ValueError("S3 key contains URL-encoded path traversal or invalid characters")

    # Decode and check canonical form
    try:
        decoded_key = urllib.parse.unquote(key)
    except Exception:
        raise ValueError("S3 key contains invalid URL encoding")

    if decoded_key != key:
        raise ValueError("S3 key contains URL-encoded characters - use raw key only")

    # Check for path traversal patterns (decoded)
    if '..' in decoded_key:
        raise ValueError("S3 key contains path traversal sequence '..'")
    if decoded_key.startswith('/'):
        raise ValueError("S3 key cannot start with '/'")
    if '/./' in decoded_key or decoded_key.endswith('/.'):
        raise ValueError("S3 key contains invalid '/.' pattern")
    if '//' in decoded_key:
        raise ValueError("S3 key contains consecutive slashes '//'")
    if decoded_key.endswith('/'):
        raise ValueError("S3 key cannot end with '/'")

    # Check for backslash (Windows path separator could cause issues)
    if '\\' in decoded_key:
        raise ValueError("S3 key cannot contain backslash")

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


def retry_s3_operation(func, max_attempts=3, initial_backoff=0.1, use_circuit_breaker=True):
    """
    Retry wrapper for S3 operations with exponential backoff.
    Only retries on transient errors (5xx, throttling, etc).

    When use_circuit_breaker=True (default), fails fast without attempting
    if the circuit is open (too many recent failures).

    :param func: Callable that takes no args (e.g., lambda: s3_client.download_file(...))
    :param max_attempts: Maximum number of attempts (default 3)
    :param initial_backoff: Initial sleep time in seconds (default 0.1)
    :param use_circuit_breaker: If True, check circuit breaker before attempting (default True)
    :return: Result of func if successful
    :raises: S3CircuitOpenError if circuit is open and use_circuit_breaker=True
    :raises: The last exception if all attempts fail
    """
    if use_circuit_breaker and not _global_s3_circuit_breaker.is_available():
        raise S3CircuitOpenError(
            f"S3 circuit is open. Retry in {_global_s3_circuit_breaker.get_time_until_retry():.1f}s"
        )

    last_exception = None
    for attempt in range(1, max_attempts + 1):
        try:
            result = func()
            if use_circuit_breaker:
                _global_s3_circuit_breaker.record_success()
            return result
        except ClientError as e:
            last_exception = e
            error_code = e.response.get('Error', {}).get('Code', '')
            # Only retry on transient errors: 5xx, RequestTimeout, Throttling, SlowDown
            if error_code not in ['500', '503', '504', 'RequestTimeout', 'Throttling', 'SlowDown']:
                if use_circuit_breaker:
                    _global_s3_circuit_breaker.record_success()  # Non-transient is "success" for circuit purposes
                raise
            if attempt < max_attempts:
                sleep_time = initial_backoff * (2 ** (attempt - 1))
                time.sleep(sleep_time)
        except Exception as e:
            last_exception = e
            if attempt < max_attempts:
                sleep_time = initial_backoff * (2 ** (attempt - 1))
                time.sleep(sleep_time)

    if use_circuit_breaker:
        _global_s3_circuit_breaker.record_failure()
    raise last_exception


class S3CircuitBreaker:
    """
    Thread-safe circuit breaker for S3 operations.

    States:
    - CLOSED: Normal operation, requests go through
    - OPEN: Circuit is tripped, requests fail fast without trying
    - HALF_OPEN: After recovery_timeout, one probe request is allowed

    Transitions:
    - CLOSED -> OPEN: After failure_threshold consecutive failures
    - OPEN -> HALF_OPEN: After recovery_timeout seconds
    - HALF_OPEN -> CLOSED: On successful probe
    - HALF_OPEN -> OPEN: On failed probe
    """

    CLOSED = 'closed'
    OPEN = 'open'
    HALF_OPEN = 'half_open'

    def __init__(self, failure_threshold: int = 5, recovery_timeout: int = 30):
        self.failure_threshold = failure_threshold
        self.recovery_timeout = recovery_timeout
        self._consecutive_failures = 0
        self._last_failure_time = 0.0
        self._state = self.CLOSED
        self._lock = threading.RLock()

    @property
    def state(self) -> str:
        with self._lock:
            if self._state == self.OPEN:
                if time.time() - self._last_failure_time >= self.recovery_timeout:
                    self._state = self.HALF_OPEN
            return self._state

    def is_available(self) -> bool:
        """Check if operation is allowed (not circuit-open)."""
        return self.state != self.OPEN

    def record_success(self) -> None:
        """Record a successful S3 operation."""
        with self._lock:
            self._consecutive_failures = 0
            self._state = self.CLOSED

    def record_failure(self) -> None:
        """Record a failed S3 operation."""
        with self._lock:
            self._consecutive_failures += 1
            self._last_failure_time = time.time()
            if self._consecutive_failures >= self.failure_threshold:
                self._state = self.OPEN

    def get_time_until_retry(self) -> float:
        """Returns seconds until circuit might close, or 0 if already half-open or closed."""
        with self._lock:
            if self._state == self.OPEN:
                elapsed = time.time() - self._last_failure_time
                remaining = self.recovery_timeout - elapsed
                return max(0.0, remaining)
            return 0.0


_global_s3_circuit_breaker = S3CircuitBreaker()


def with_s3_circuit_breaker(func: Callable[[], Any]) -> Any:
    """
    Execute an S3 operation with circuit breaker protection.

    If the circuit is open, fails fast with S3CircuitOpenError.
    Transient errors (5xx, throttling) increment the failure counter.
    Successful calls reset the failure counter.

    Usage:
        result = with_s3_circuit_breaker(
            lambda: s3_client.download_file(bucket, key, path)
        )
    """
    if not _global_s3_circuit_breaker.is_available():
        raise S3CircuitOpenError(
            f"S3 circuit is open. Retry in {_global_s3_circuit_breaker.get_time_until_retry():.1f}s"
        )

    try:
        result = func()
        _global_s3_circuit_breaker.record_success()
        return result
    except ClientError as e:
        error_code = e.response.get('Error', {}).get('Code', '')
        is_transient = error_code in ['500', '503', '504', 'RequestTimeout', 'Throttling', 'SlowDown']
        if is_transient:
            _global_s3_circuit_breaker.record_failure()
        raise
    except Exception:
        _global_s3_circuit_breaker.record_failure()
        raise


class S3CircuitOpenError(Exception):
    """Raised when S3 circuit breaker is open and request was not attempted."""
    pass