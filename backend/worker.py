"""
Worker tasks for processing audio mastering jobs.
"""
import os
import uuid
import tempfile
import logging
from datetime import datetime
from typing import Optional

import boto3
from botocore.exceptions import ClientError
from sqlalchemy.exc import SQLAlchemyError

from backend import db, create_app
from backend.models import Job
from backend.utils import validate_audio_file, get_file_extension, setup_logging, validate_s3_uri, retry_s3_operation

# Set up logging
logger = setup_logging()

# We'll create a Flask app instance to have access to the configuration and the database.
# We'll use the same configuration as the main app.
# We'll create the app once and reuse it.
# Note: In a production setting, we might want to use a more efficient way to share the configuration.
# But for simplicity, we'll create the app here.

# We'll create the app in a function so that we can have an application context.
def get_app():
    """
    Create and configure the Flask app.
    """
    app = create_app()
    return app

# We'll use a temporary directory for downloading and processing files.
# We'll clean up the temporary files after processing.

# We'll also load the model once per worker process to avoid reloading it for every job.
# However, note that we are using a placeholder model for now.
# In a real implementation, we would load the Mamba-based model here.

# We'll define a placeholder function for the mastering process.
def master_audio(input_file_path: str, output_file_path: str, preset_id: str, intensity: float,
                 loudness_target: Optional[float], stereo_width: Optional[float],
                 enable_true_peak_limiting: bool) -> dict:
    """
    Placeholder for the actual mastering process.
    In a real implementation, this would load the Mamba-based model and process the audio.
    For now, we'll just copy the input file to the output and return dummy loudness metrics.

    Returns a dictionary with loudness metrics.
    """
    # For now, we'll just copy the file.
    import shutil
    shutil.copy2(input_file_path, output_file_path)

    # Return dummy loudness metrics.
    # In a real implementation, we would compute these from the processed audio.
    return {
        'integratedLUFS': -14.0,
        'truePeak': -0.8,
        'lra': 8.5,
        'peak': -1.2
    }

# We'll define the main job processing function that will be called by RQ.
def process_mastering_job(job_id: str):
    """
    Process a mastering job.
    This function is designed to be run by an RQ worker.

    Steps:
    1. Get the job from the database.
    2. Download the input file from S3 to a temporary location.
    3. Validate the file.
    4. Process the file (run the mastering model).
    5. Upload the output file to S3.
    6. Update the job in the database with the output file URI and loudness metrics.
    7. Handle any errors and update the job status accordingly.

    :param job_id: The ID of the job to process (as a string).
    """
    # We need to have an application context to access the database and configuration.
    app = get_app()
    with app.app_context():
        job = None
        try:
            # Convert the job_id string to a UUID object for the database query.
            job_uuid = uuid.UUID(job_id)
        except ValueError:
            logger.error(f"Invalid job ID format: {job_id}")
            return

        import socket
        # Get the job from the database using SELECT FOR UPDATE to lock it
        job = Job.query.filter_by(id=job_uuid, status='pending').with_for_update().first()
        if not job:
            logger.warning(f"Job {job_id} already processing or completed, or does not exist")
            return

        # Update the job status to processing.
        job.status = 'processing'
        job.startedAt = datetime.utcnow()
        job.processing_worker_id = f"{socket.gethostname()}:{os.getpid()}"
        try:
            db.session.commit()
        except SQLAlchemyError as e:
            logger.error(f"Failed to update job status to processing: {e}")
            db.session.rollback()
            return

        # We'll use a temporary directory for the input and output files.
        with tempfile.TemporaryDirectory() as temp_dir:
            # Define the paths for the input and output files.
            input_file_path = os.path.join(temp_dir, 'input' + get_file_extension(job.contentType))
            output_file_path = os.path.join(temp_dir, 'output' + get_file_extension(job.contentType))

            # Download the input file from S3.
            try:
                s3_client = boto3.client(
                    's3',
                    aws_access_key_id=app.config['AWS_ACCESS_KEY_ID'],
                    aws_secret_access_key=app.config['AWS_SECRET_ACCESS_KEY'],
                    region_name=app.config['AWS_S3_REGION_NAME']
                )
                # Validate and parse S3 URI
                try:
                    bucket, key = validate_s3_uri(job.inputFileUri)
                except ValueError as e:
                    raise ValueError(f"Invalid input file URI: {e}")

                # Download the file with retry
                retry_s3_operation(
                    lambda: s3_client.download_file(bucket, key, input_file_path),
                    max_attempts=3,
                    initial_backoff=0.2
                )
                logger.info(f"Downloaded input file for job {job_id} from s3://{bucket}/{key}")
            except Exception as e:
                logger.error(f"Failed to download input file for job {job_id}: {e}")
                job.status = 'failed'
                job.errorMessage = f"Failed to download input file: {str(e)}"
                job.completedAt = datetime.utcnow()
                try:
                    db.session.commit()
                except SQLAlchemyError:
                    db.session.rollback()
                return

            # Validate the input file.
            is_valid, error_msg = validate_audio_file(input_file_path, job.contentType)
            if not is_valid:
                logger.error(f"Input file validation failed for job {job_id}: {error_msg}")
                job.status = 'failed'
                job.errorMessage = f"Input file validation failed: {error_msg}"
                job.completedAt = datetime.utcnow()
                try:
                    db.session.commit()
                except SQLAlchemyError:
                    db.session.rollback()
                return

            # Process the file (run the mastering model).
            try:
                loudness_metrics = master_audio(
                    input_file_path=input_file_path,
                    output_file_path=output_file_path,
                    preset_id=job.presetId,
                    intensity=job.intensity,
                    loudness_target=job.loudnessTarget,
                    stereo_width=job.stereoWidth,
                    enable_true_peak_limiting=job.enableTruePeakLimiting
                )
            except Exception as e:
                logger.error(f"Failed to process audio for job {job_id}: {e}")
                job.status = 'failed'
                job.errorMessage = f"Audio processing failed: {str(e)}"
                job.completedAt = datetime.utcnow()
                try:
                    db.session.commit()
                except SQLAlchemyError:
                    db.session.rollback()
                return

            # Upload the output file to S3.
            try:
                # Generate a unique key for the output file.
                output_key = f"mastering-service-outputs/{job.userId}/{job.id}/{uuid.uuid4().hex}{get_file_extension(job.contentType)}"
                # Upload the file with retry
                retry_s3_operation(
                    lambda: s3_client.upload_file(
                        output_file_path,
                        app.config['AWS_STORAGE_BUCKET_NAME'],
                        output_key,
                        ExtraArgs={
                            'ContentType': job.contentType,
                            'ContentDisposition': 'attachment'
                        }
                    ),
                    max_attempts=3,
                    initial_backoff=0.2
                )
                # Construct the output file URI.
                output_file_uri = f"s3://{app.config['AWS_STORAGE_BUCKET_NAME']}/{output_key}"
                logger.info(f"Uploaded output file for job {job_id} to s3://{app.config['AWS_STORAGE_BUCKET_NAME']}/{output_key}")
            except Exception as e:
                logger.error(f"Failed to upload output file for job {job_id}: {e}")
                job.status = 'failed'
                job.errorMessage = f"Failed to upload output file: {str(e)}"
                job.completedAt = datetime.utcnow()
                try:
                    db.session.commit()
                except SQLAlchemyError:
                    db.session.rollback()
                return

            # Update the job with the output file URI and loudness metrics.
            job.status = 'completed'
            job.completedAt = datetime.utcnow()
            job.outputFileUri = output_file_uri
            job.loudnessMetrics = loudness_metrics
            # Note: We already deducted the credit when the job was created.
            # We do not need to deduct it again.
            try:
                db.session.commit()
                logger.info(f"Job {job_id} completed successfully")
            except SQLAlchemyError as e:
                logger.error(f"Failed to update job {job_id} as completed: {e}")
                db.session.rollback()
                # Job remains in processing state; but we'll try to mark failed below in outer catch? Actually this is inner.
                # We'll treat commit failure as an unexpected error and let outer except handle? No, outer except will not catch.
                # So we need to attempt to mark as failed if this final commit fails.
                try:
                    job.status = 'failed'
                    job.errorMessage = f"Database commit failed: {str(e)}"
                    db.session.commit()
                except SQLAlchemyError:
                    db.session.rollback()
                return

        except Exception as e:
            # Catch any unexpected errors.
            logger.error(f"Unexpected error processing job {job_id}: {e}")
            if job is not None:
                job.status = 'failed'
                job.errorMessage = f"Unexpected error: {str(e)}"
                job.completedAt = datetime.utcnow()
                # Try to commit; if it fails, we have limited recourse
                try:
                    db.session.commit()
                    logger.info(f"Job {job_id} marked as failed")
                except SQLAlchemyError as commit_err:
                    db.session.rollback()
                    logger.critical(f"Failed to mark job {job_id} as failed: {commit_err}. Job may remain in processing state.")
                    # In a production system, consider alerting or manual reconciliation.
        finally:
            # Ensure the database session is cleaned up
            db.session.remove()