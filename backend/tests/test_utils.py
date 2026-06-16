"""
Unit tests for backend utilities: S3 circuit breaker and URI validation.
"""

import time

import pytest

from backend.utils import (
    S3CircuitBreaker,
    S3CircuitOpenError,
    _global_s3_circuit_breaker,
    retry_s3_operation,
    validate_s3_uri,
)


@pytest.fixture(autouse=True)
def reset_global_circuit_breaker():
    """Reset the global S3 circuit breaker before each test."""
    _global_s3_circuit_breaker._consecutive_failures = 0
    _global_s3_circuit_breaker._last_failure_time = 0.0
    _global_s3_circuit_breaker._state = S3CircuitBreaker.CLOSED
    yield
    _global_s3_circuit_breaker._consecutive_failures = 0
    _global_s3_circuit_breaker._last_failure_time = 0.0
    _global_s3_circuit_breaker._state = S3CircuitBreaker.CLOSED


class TestS3CircuitBreaker:
    """Tests for the S3 circuit breaker."""

    def test_initial_state_is_closed(self):
        cb = S3CircuitBreaker(failure_threshold=3, recovery_timeout=5)
        assert cb.state == "closed"
        assert cb.is_available() is True

    def test_record_success_resets_failure_count(self):
        cb = S3CircuitBreaker(failure_threshold=3, recovery_timeout=5)
        cb.record_failure()
        cb.record_failure()
        assert cb._consecutive_failures == 2
        cb.record_success()
        assert cb._consecutive_failures == 0
        assert cb.state == "closed"

    def test_opens_after_failure_threshold(self):
        cb = S3CircuitBreaker(failure_threshold=3, recovery_timeout=5)
        cb.record_failure()
        assert cb.state == "closed"
        cb.record_failure()
        assert cb.state == "closed"
        cb.record_failure()
        assert cb.state == "open"
        assert cb.is_available() is False

    def test_half_open_after_recovery_timeout(self):
        cb = S3CircuitBreaker(failure_threshold=1, recovery_timeout=0.1)
        cb.record_failure()  # Opens immediately
        assert cb.state == "open"
        time.sleep(0.15)
        # Accessing state should transition to half_open
        assert cb.state == "half_open"
        assert cb.is_available() is True

    def test_half_open_to_closed_on_success(self):
        cb = S3CircuitBreaker(failure_threshold=1, recovery_timeout=0.05)
        cb.record_failure()
        assert cb.state == "open"
        time.sleep(0.1)
        _ = cb.state  # Transition to half_open
        cb.record_success()
        assert cb.state == "closed"

    def test_half_open_to_open_on_failure(self):
        cb = S3CircuitBreaker(failure_threshold=1, recovery_timeout=0.05)
        cb.record_failure()
        time.sleep(0.1)
        _ = cb.state  # Transition to half_open
        cb.record_failure()
        assert cb.state == "open"

    def test_get_time_until_retry(self):
        cb = S3CircuitBreaker(failure_threshold=1, recovery_timeout=5)
        cb.record_failure()
        assert cb.state == "open"
        remaining = cb.get_time_until_retry()
        assert 4.5 < remaining <= 5.0


class TestS3CircuitBreakerIntegration:
    """Integration tests for circuit breaker with retry_s3_operation."""

    def test_circuit_open_raises_error(self):
        # Trip the global circuit breaker (threshold is 5)
        for _ in range(5):
            _global_s3_circuit_breaker.record_failure()
        assert _global_s3_circuit_breaker.state == "open"

        with pytest.raises(S3CircuitOpenError):
            retry_s3_operation(lambda: None, use_circuit_breaker=True)

    def test_circuit_allows_request_when_closed(self):
        cb = S3CircuitBreaker(failure_threshold=5, recovery_timeout=30)
        result = retry_s3_operation(lambda: "success", use_circuit_breaker=True)
        assert result == "success"
        assert cb._consecutive_failures == 0


class TestValidateS3Uri:
    """Tests for S3 URI validation."""

    def test_valid_uri(self):
        bucket, key = validate_s3_uri("s3://mybucket/path/to/file.wav")
        assert bucket == "mybucket"
        assert key == "path/to/file.wav"

    def test_valid_uri_with_nested_path(self):
        bucket, key = validate_s3_uri("s3://test-bucket/a/b/c/d.txt")
        assert bucket == "test-bucket"
        assert key == "a/b/c/d.txt"

    def test_empty_uri_raises(self):
        with pytest.raises(ValueError, match="non-empty string"):
            validate_s3_uri("")

    def test_missing_s3_prefix_raises(self):
        with pytest.raises(ValueError, match="must start with 's3://'"):
            validate_s3_uri("https://bucket/key")

    def test_missing_key_raises(self):
        with pytest.raises(ValueError, match="missing key"):
            validate_s3_uri("s3://bucketonly")

    def test_empty_bucket_raises(self):
        with pytest.raises(ValueError, match="Invalid bucket name"):
            validate_s3_uri("s3:///key")

    def test_invalid_bucket_name_too_short(self):
        with pytest.raises(ValueError, match="3-63 characters"):
            validate_s3_uri("s3://ab/key")

    def test_invalid_bucket_name_uppercase(self):
        with pytest.raises(ValueError, match="does not match AWS"):
            validate_s3_uri("s3://MyBucket/key")

    def test_key_too_long_raises(self):
        long_key = "a" * 1025
        with pytest.raises(ValueError, match="too long"):
            validate_s3_uri(f"s3://bucket/{long_key}")

    def test_key_ends_with_slash_raises(self):
        with pytest.raises(ValueError, match="cannot end with '/'"):
            validate_s3_uri("s3://bucket/key/")

    def test_key_with_control_char_raises(self):
        with pytest.raises(ValueError, match="null byte"):
            validate_s3_uri("s3://bucket/key\x00with null")

    def test_key_with_path_traversal_raises(self):
        with pytest.raises(ValueError, match="'..'"):
            validate_s3_uri("s3://bucket/../etc/passwd")

    def test_key_with_double_slash_raises(self):
        with pytest.raises(ValueError, match="consecutive slashes"):
            validate_s3_uri("s3://bucket/path//file.wav")

    def test_key_with_url_encoding_raises(self):
        with pytest.raises(ValueError, match="URL-encoded path traversal"):
            validate_s3_uri("s3://bucket/path%2Fto%2Ffile.wav")

    def test_key_with_encoded_traversal_raises(self):
        with pytest.raises(ValueError, match="URL-encoded path traversal"):
            validate_s3_uri("s3://bucket/%2e%2e/etc")

    def test_key_with_backslash_raises(self):
        with pytest.raises(ValueError, match="cannot contain backslash"):
            validate_s3_uri("s3://bucket/path\\file.wav")

    def test_key_with_dot_segment_raises(self):
        with pytest.raises(ValueError, match="invalid '/.' pattern"):
            validate_s3_uri("s3://bucket/path/./file.wav")

    def test_key_starts_with_slash_raises(self):
        with pytest.raises(ValueError, match="cannot start with '/'"):
            validate_s3_uri("s3://bucket//starts/with/slash")

    def test_valid_bucket_with_dots(self):
        bucket, key = validate_s3_uri("s3://my.bucket.name/path/to/file.wav")
        assert bucket == "my.bucket.name"
        assert key == "path/to/file.wav"

    def test_valid_bucket_with_dashes(self):
        bucket, key = validate_s3_uri("s3://my-bucket-123/path/to/file.wav")
        assert bucket == "my-bucket-123"
        assert key == "path/to/file.wav"

    def test_bucket_with_invalid_char_raises(self):
        with pytest.raises(ValueError, match="does not match AWS"):
            validate_s3_uri("s3://my_bucket/key")  # underscore not allowed
