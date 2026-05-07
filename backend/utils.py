"""
Utility functions for the AI-Powered Audio Mastering Service.
"""
import os
import mimetypes
from typing import Tuple, Optional

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