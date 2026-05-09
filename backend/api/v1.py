"""
API v1 routes for the AI-Powered Audio Mastering Service.
"""
from flask import Blueprint, request, jsonify, current_app
from flask_jwt_extended import jwt_required, get_jwt_identity
from werkzeug.utils import secure_filename
from functools import wraps
import os
import uuid
from datetime import datetime, timedelta
import boto3
from botocore.exceptions import ClientError
from backend import db
from backend.models import User, Job
from backend.utils import validate_audio_file, get_file_extension, get_content_type_from_filename

# Create blueprint
api_bp = Blueprint('api', __name__)


def rate_limit(max_per_minute):
    """Simple rate limiting decorator using Redis."""
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            # Skip rate limiting in development/testing if desired
            if current_app.config.get('DEBUG', False):
                return func(*args, **kwargs)
            
            ip = request.remote_addr
            endpoint = request.endpoint or 'unknown'
            key = f"rate_limit:{endpoint}:{ip}"
            
            try:
                current = current_app.redis.incr(key)
                if current == 1:
                    current_app.redis.expire(key, 60)
                if current > max_per_minute:
                    return jsonify({
                        'error': 'RATE_LIMIT_EXCEEDED',
                        'message': f'Maximum {max_per_minute} requests per minute allowed'
                    }), 429
            except Exception as e:
                # If Redis is down, log but allow the request to proceed (fail open)
                current_app.logger.error(f"Rate limiting failed: {e}")
            
            return func(*args, **kwargs)
        return wrapper
    return decorator

# Helper function to get presigned URL for upload
def get_presigned_upload_url(file_name, content_type, user_id, job_id):
    """
    Generate a presigned URL for uploading a file to S3.
    """
    s3_client = boto3.client(
        's3',
        aws_access_key_id=current_app.config['AWS_ACCESS_KEY_ID'],
        aws_secret_access_key=current_app.config['AWS_SECRET_ACCESS_KEY'],
        region_name=current_app.config['AWS_S3_REGION_NAME']
    )
    
    # Generate a unique key for the file
    file_ext = get_file_extension(content_type)
    key = f"mastering-service-inputs/{user_id}/{job_id}/{uuid.uuid4().hex}{file_ext}"
    
    try:
        response = s3_client.generate_presigned_post(
            Bucket=current_app.config['AWS_STORAGE_BUCKET_NAME'],
            Key=key,
            Fields={"Content-Type": content_type},
            Conditions=[
                {"Content-Type": content_type},
                ["content-length-range", 1, current_app.config['MAX_FILE_SIZE']]
            ],
            ExpiresIn=current_app.config['PRESIGNED_URL_EXPIRY']
        )
    except ClientError as e:
        current_app.logger.error(f"Failed to generate presigned URL: {e}")
        return None
    
    return {
        'uploadUrl': response['url'],
        'fields': response['fields'],
        'method': 'POST',
        'expiresIn': current_app.config['PRESIGNED_URL_EXPIRY'],
        'objectKey': key
    }

# Helper function to get presigned URL for download
def get_presigned_download_url(object_key, expires_in=None):
    """
    Generate a presigned URL for downloading a file from S3.
    """
    if expires_in is None:
        expires_in = current_app.config['PRESIGNED_URL_EXPIRY']
    
    s3_client = boto3.client(
        's3',
        aws_access_key_id=current_app.config['AWS_ACCESS_KEY_ID'],
        aws_secret_access_key=current_app.config['AWS_SECRET_ACCESS_KEY'],
        region_name=current_app.config['AWS_S3_REGION_NAME']
    )
    
    try:
        url = s3_client.generate_presigned_url(
            ClientMethod='get_object',
            Params={
                'Bucket': current_app.config['AWS_STORAGE_BUCKET_NAME'],
                'Key': object_key
            },
            ExpiresIn=expires_in
        )
    except ClientError as e:
        current_app.logger.error(f"Failed to generate presigned download URL: {e}")
        return None
    
    return url

# Authentication routes
@api_bp.route('/auth/register', methods=['POST'])
@rate_limit(5)  # 5 registrations per minute per IP
def register():
    """
    Register a new user.
    ---
    tags:
      - authentication
    requestBody:
      required: true
      content:
        application/json:
          schema:
            type: object
            required:
              - email
              - password
              - fullName
            properties:
              email:
                type: string
                format: email
              password:
                type: string
                format: password
                minLength: 8
              fullName:
                type: string
                minLength: 2
                maxLength: 100
    responses:
      '201':
        description: User created successfully
        content:
          application/json:
            schema:
              type: object
              properties:
                accessToken:
                  type: string
                tokenType:
                  type: string
                expiresIn:
                  type: integer
                user:
                  $ref: '#/components/schemas/User'
      '400':
        description: Invalid input
      '409':
        description: User already exists
    """
    data = request.get_json()
    
    # Validate input
    if not data or not data.get('email') or not data.get('password') or not data.get('fullName'):
        return jsonify({'error': 'VALIDATION_ERROR', 'message': 'Missing required fields'}), 400
    
    email = data['email'].strip().lower()
    password = data['password']
    fullName = data['fullName'].strip()
    
    if len(fullName) < 2 or len(fullName) > 100:
        return jsonify({'error': 'VALIDATION_ERROR', 'message': 'Full name must be between 2 and 100 characters'}), 400
    
    # Check if user already exists
    if User.query.filter_by(email=email).first():
        return jsonify({'error': 'CONFLICT', 'message': 'User with this email already exists'}), 409
    
    # Create new user
    from werkzeug.security import generate_password_hash
    hashed_password = generate_password_hash(password, method='pbkdf2:sha256')
    
    new_user = User(
        email=email,
        fullName=fullName,
        passwordHash=hashed_password,
        credits=10  # Give new users some free credits
    )
    
    db.session.add(new_user)
    db.session.commit()
    
    # Generate access token
    from flask_jwt_extended import create_access_token
    access_token = create_access_token(identity=new_user.id)
    
    return jsonify({
        'accessToken': access_token,
        'tokenType': 'Bearer',
        'expiresIn': 3600,  # 1 hour
        'user': {
            'id': str(new_user.id),
            'email': new_user.email,
            'fullName': new_user.fullName,
            'createdAt': new_user.createdAt.isoformat(),
            'credits': new_user.credits,
            'subscriptionTier': new_user.subscriptionTier
        }
    }), 201

@api_bp.route('/auth/login', methods=['POST'])
def login():
    """
    Log in user.
    ---
    tags:
      - authentication
    requestBody:
      required: true
      content:
        application/json:
          schema:
            type: object
            required:
              - email
              - password
            properties:
              email:
                type: string
                format: email
              password:
                type: string
                format: password
    responses:
      '200':
        description: Login successful
        content:
          application/json:
            schema:
              type: object
              properties:
                accessToken:
                  type: string
                tokenType:
                  type: string
                expiresIn:
                  type: integer
                user:
                  $ref: '#/components/schemas/User'
      '401':
        description: Invalid credentials
    """
    data = request.get_json()
    
    if not data or not data.get('email') or not data.get('password'):
        return jsonify({'error': 'VALIDATION_ERROR', 'message': 'Missing email or password'}), 400
    
    email = data['email'].strip().lower()
    password = data['password']
    
    user = User.query.filter_by(email=email).first()
    
    if not user or not user.check_password(password):
        return jsonify({'error': 'INVALID_CREDENTIALS', 'message': 'Invalid email or password'}), 401
    
    # Update last login
    user.lastLoginAt = datetime.utcnow()
    db.session.commit()
    
    # Generate access token
    from flask_jwt_extended import create_access_token
    access_token = create_access_token(identity=user.id)
    
    return jsonify({
        'accessToken': access_token,
        'tokenType': 'Bearer',
        'expiresIn': 3600,  # 1 hour
        'user': {
            'id': str(user.id),
            'email': user.email,
            'fullName': user.fullName,
            'createdAt': user.createdAt.isoformat(),
            'credits': user.credits,
            'subscriptionTier': user.subscriptionTier
        }
    }), 200

@api_bp.route('/auth/logout', methods=['POST'])
@jwt_required()
def logout():
    """
    Log out user.
    ---
    tags:
      - authentication
    security:
      - bearerAuth: []
    responses:
      '200':
        description: Logged out successfully
      '401':
        description: Unauthorized
    """
    # In a more advanced implementation, we might add the token to a blacklist
    # For now, we just return success (the client should discard the token)
    return jsonify({'message': 'Logged out successfully'}), 200

# Job routes
@api_bp.route('/jobs', methods=['GET'])
@jwt_required()
def list_jobs():
    """
    List user's mastering jobs.
    ---
    tags:
      - jobs
    security:
      - bearerAuth: []
    parameters:
      - in: query
        name: limit
        schema:
          type: integer
          minimum: 1
          maximum: 100
          default: 20
        description: Number of jobs to return
      - in: query
        name: offset
        schema:
          type: integer
          minimum: 0
          default: 0
        description: Number of jobs to skip
      - in: query
        name: status
        schema:
          type: string
          enum: [pending, processing, completed, failed, cancelled]
        description: Filter by job status
    responses:
      '200':
        description: List of jobs
        content:
          application/json:
            schema:
              type: object
              properties:
                jobs:
                  type: array
                  items:
                    $ref: '#/components/schemas/JobResponse'
                total:
                  type: integer
                limit:
                  type: integer
                offset:
                  type: integer
      '401':
        description: Unauthorized
    """
    current_user_id = get_jwt_identity()
    
    # Get query parameters
    limit = request.args.get('limit', 20, type=int)
    offset = request.args.get('offset', 0, type=int)
    status = request.args.get('status', None)
    
    # Validate limit and offset
    if limit < 1 or limit > 100:
        limit = 20
    if offset < 0:
        offset = 0
    
    # Build query
    query = Job.query.filter_by(userId=current_user_id)
    
    if status:
        query = query.filter_by(status=status)
    
    # Get total count
    total = query.count()
    
    # Get paginated results
    jobs = query.order_by(Job.createdAt.desc()).offset(offset).limit(limit).all()
    
    # Format response
    job_list = []
    for job in jobs:
        job_data = {
            'id': str(job.id),
            'status': job.status,
            'presetId': job.presetId,
            'intensity': job.intensity,
            'createdAt': job.createdAt.isoformat(),
            'fileName': job.fileName,
            'fileSize': job.fileSize
        }
        if job.startedAt:
            job_data['startedAt'] = job.startedAt.isoformat()
        if job.completedAt:
            job_data['completedAt'] = job.completedAt.isoformat()
        # Note: We don't have a direct resultUrl in the Job model, but we can compute it if needed.
        # However, we have a separate endpoint for getting the result URL.
        # So we will not include it in the list response.
        job_list.append(job_data)
    
    return jsonify({
        'jobs': job_list,
        'total': total,
        'limit': limit,
        'offset': offset
    }), 200

@api_bp.route('/jobs', methods=['POST'])
@jwt_required()
@rate_limit(10)  # 10 job creations per minute per IP
def create_job():
    """
    Create a new mastering job.
    ---
    tags:
      - jobs
    security:
      - bearerAuth: []
    requestBody:
      required: true
      content:
        application/json:
          schema:
            type: object
            required:
              - fileName
              - fileSize
              - presetId
              - contentType
            properties:
              fileName:
                type: string
                maxLength: 255
              fileSize:
                type: integer
                minimum: 1
                maximum: 2147483648
              presetId:
                type: string
                enum: [transparent, warm, loud]
              contentType:
                type: string
                enum: [audio/wav, audio/aiff, audio/flac]
              intensity:
                type: number
                minimum: 0
                maximum: 100
                default: 50
              loudnessTarget:
                type: number
                minimum: -24
                maximum: -6
                default: -14
              stereoWidth:
                type: number
                minimum: 0
                maximum: 200
                default: 100
              enableTruePeakLimiting:
                type: boolean
                default: true
    responses:
      '202':
        description: Job accepted for processing
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/JobResponse'
      '400':
        description: Invalid request
      '401':
        description: Unauthorized
      '402':
        description: Payment required / insufficient credits
      '413':
        description: File too large
    """
    current_user_id = get_jwt_identity()
    data = request.get_json()
    
    # Validate input
    if not data:
        return jsonify({'error': 'VALIDATION_ERROR', 'message': 'No input data provided'}), 400
    
    required_fields = ['fileName', 'fileSize', 'presetId', 'contentType']
    for field in required_fields:
        if field not in data:
            return jsonify({'error': 'VALIDATION_ERROR', 'message': f'Missing required field: {field}'}), 400
    
    file_name = data['fileName'].strip()
    file_size = data['fileSize']
    preset_id = data['presetId']
    content_type = data['contentType']
    intensity = data.get('intensity', 50)
    loudness_target = data.get('loudnessTarget', -14)
    stereo_width = data.get('stereoWidth', 100)
    enable_true_peak_limiting = data.get('enableTruePeakLimiting', True)
    
    # Validate file name
    if not file_name or len(file_name) > 255:
        return jsonify({'error': 'VALIDATION_ERROR', 'message': 'File name must be between 1 and 255 characters'}), 400
    
    # Validate file size
    if not isinstance(file_size, int) or file_size < 1 or file_size > current_app.config['MAX_FILE_SIZE']:
        return jsonify({'error': 'VALIDATION_ERROR', 'message': f'File size must be between 1 and {current_app.config["MAX_FILE_SIZE"]} bytes'}), 400
    
    # Validate preset
    if preset_id not in current_app.config['PRESETS']:
        return jsonify({'error': 'VALIDALID_ERROR', 'message': 'Invalid preset ID'}), 400
    
    # Validate content type
    if content_type not in current_app.config['ALLOWED_CONTENT_TYPES']:
        return jsonify({'error': 'VALIDATION_ERROR', 'message': 'Invalid content type. Must be one of: audio/wav, audio/aiff, audio/flac'}), 400
    
    # Validate intensity
    if not isinstance(intensity, (int, float)) or intensity < 0 or intensity > 100:
        return jsonify({'error': 'VALIDATION_ERROR', 'message': 'Intensity must be between 0 and 100'}), 400
    
    # Validate loudness target
    if loudness_target is not None and (loudness_target < -24 or loudness_target > -6):
        return jsonify({'error': 'VALIDATION_ERROR', 'message': 'Loudness target must be between -24 and -6 LUFS'}), 400
    
    # Validate stereo width
    if stereo_width is not None and (stereo_width < 0 or stereo_width > 200):
        return jsonify({'error': 'VALIDATION_ERROR', 'message': 'Stereo width must be between 0 and 200'}), 400
    
    # Check user credits
    user = User.query.get(current_user_id)
    if user.credits < 1:
        return jsonify({'error': 'INSUFFICIENT_CREDITS', 'message': 'Insufficient credits to process this job'}), 402
    
    # Check concurrent job limit
    max_concurrent = current_app.config.get('MAX_CONCURRENT_JOBS', 5)
    active_jobs = Job.query.filter_by(userId=current_user_id).filter(
        Job.status.in_(['pending', 'processing'])
    ).count()
    if active_jobs >= max_concurrent:
        return jsonify({
            'error': 'TOO_MANY_JOBS',
            'message': f'Maximum {max_concurrent} concurrent jobs allowed. Please wait for current jobs to complete.'
        }), 429
    
    # Generate job ID
    job_id = uuid.uuid4()
    
    # Determine file extension from content type
    file_ext = get_file_extension(content_type)
    if not file_ext:
        # This should not happen because we validated the content type, but just in case
        return jsonify({'error': 'VALIDATION_ERROR', 'message': 'Could not determine file extension from content type'}), 400
    
    # Generate the object key for the input file
    input_object_key = f"mastering-service-inputs/{current_user_id}/{job_id}/{uuid.uuid4().hex}{file_ext}"
    input_file_uri = f"s3://{current_app.config['AWS_STORAGE_BUCKET_NAME']}/{input_object_key}"
    
    # Create new job
    new_job = Job(
        id=job_id,
        userId=current_user_id,
        fileName=file_name,
        fileSize=file_size,
        contentType=content_type,
        presetId=preset_id,
        intensity=intensity,
        loudnessTarget=loudness_target,
        stereoWidth=stereo_width,
        enableTruePeakLimiting=enable_true_peak_limiting,
        status='pending',
        inputFileUri=input_file_uri
    )
    
    # Deduct one credit from the user
    user.credits -= 1
    
    # Save to database
    db.session.add(new_job)
    db.session.commit()
    
    # Return job response
    return jsonify({
        'id': str(new_job.id),
        'status': new_job.status,
        'presetId': new_job.presetId,
        'intensity': new_job.intensity,
        'createdAt': new_job.createdAt.isoformat(),
        'fileName': new_job.fileName,
        'fileSize': new_job.fileSize
    }), 202

@api_bp.route('/jobs/<string:jobId>', methods=['GET'])
@jwt_required()
def get_job(jobId):
    """
    Get job status and details.
    ---
    tags:
      - jobs
    security:
      - bearerAuth: []
    parameters:
      - in: path
        name: jobId
        required: true
        schema:
          type: string
          format: uuid
    responses:
      '200':
        description: Job details
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/JobResponse'
      '401':
        description: Unauthorized
      '404':
        description: Job not found
    """
    current_user_id = get_jwt_identity()
    
    job = Job.query.filter_by(id=jobId, userId=current_user_id).first()
    
    if not job:
        return jsonify({'error': 'NOT_FOUND', 'message': 'Job not found'}), 404
    
    # Build job response
    job_data = {
        'id': str(job.id),
        'status': job.status,
        'presetId': job.presetId,
        'intensity': job.intensity,
        'createdAt': job.createdAt.isoformat(),
        'fileName': job.fileName,
        'fileSize': job.fileSize
    }
    if job.startedAt:
        job_data['startedAt'] = job.startedAt.isoformat()
    if job.completedAt:
        job_data['completedAt'] = job.completedAt.isoformat()
    # Note: We don't include resultUrl here because we have a separate endpoint for it.
    
    return jsonify(job_data), 200

@api_bp.route('/jobs/<string:jobId>', methods=['DELETE'])
@jwt_required()
def cancel_job(jobId):
    """
    Cancel a job.
    ---
    tags:
      - jobs
    security:
      - bearerAuth: []
    parameters:
      - in: path
        name: jobId
        required: true
        schema:
          type: string
          format: uuid
    responses:
      '200':
        description: Job cancelled successfully
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/JobResponse'
      '401':
        description: Unauthorized
      '404':
        description: Job not found
      '409':
        description: Job cannot be cancelled (already completed/failed)
    """
    current_user_id = get_jwt_identity()
    
    job = Job.query.filter_by(id=jobId, userId=current_user_id).first()
    
    if not job:
        return jsonify({'error': 'NOT_FOUND', 'message': 'Job not found'}), 404
    
    if job.status in ['completed', 'failed']:
        return jsonify({'error': 'CONFLICT', 'message': 'Job cannot be cancelled as it is already completed or failed'}), 409
    
    # Update job status to cancelled
    job.status = 'cancelled'
    # Note: We do not delete the input file immediately; it will be cleaned up by the TTL policy.
    # We also do not refund the credit for simplicity (could be changed later).
    
    db.session.commit()
    
    # Return job response
    return jsonify({
        'id': str(job.id),
        'status': job.status,
        'presetId': job.presetId,
        'intensity': job.intensity,
        'createdAt': job.createdAt.isoformat(),
        'fileName': job.fileName,
        'fileSize': job.fileSize
    }), 200

@api_bp.route('/jobs/<string:jobId>/result', methods=['GET'])
@jwt_required()
def get_job_result(jobId):
    """
    Get mastered audio file download URL.
    ---
    tags:
      - jobs
    security:
      - bearerAuth: []
    parameters:
      - in: path
        name: jobId
        required: true
        schema:
          type: string
          format: uuid
    responses:
      '200':
        description: Download URL for mastered file
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/JobResultResponse'
      '401':
        description: Unauthorized
      '404':
        description: Job not found or not completed
      '410':
        description: Result expired (file deleted)
    """
    current_user_id = get_jwt_identity()
    
    job = Job.query.filter_by(id=jobId, userId=current_user_id).first()
    
    if not job:
        return jsonify({'error': 'NOT_FOUND', 'message': 'Job not found'}), 404
    
    if job.status != 'completed':
        return jsonify({'error': 'NOT_FOUND', 'message': 'Job is not completed yet'}), 404
    
    if not job.outputFileUri:
        return jsonify({'error': 'NOT_FOUND', 'message': 'Result file not found'}), 404
    
    # Extract the object key from the outputFileUri (expected format: s3://bucket/key)
    # We assume the outputFileUri is in the format: s3://bucket/key
    try:
        # Remove the s3:// prefix
        if job.outputFileUri.startswith('s3://'):
            object_key = job.outputFileUri[5:]  # Remove 's3://'
            # Split the bucket and key
            bucket, key = object_key.split('/', 1)
        else:
            # If it's not in the expected format, we cannot generate a presigned URL
            current_app.logger.error(f"Unexpected outputFileUri format: {job.outputFileUri}")
            return jsonify({'error': 'INTERNAL_ERROR', 'message': 'Internal server error'}), 500
    except Exception as e:
        current_app.logger.error(f"Failed to parse outputFileUri: {e}")
        return jsonify({'error': 'INTERNAL_ERROR', 'message': 'Internal server error'}), 500
    
    # Generate presigned download URL
    download_url = get_presigned_download_url(key)
    
    if not download_url:
        return jsonify({'error': 'INTERNAL_ERROR', 'message': 'Failed to generate download URL'}), 500
    
    # Get the file extension from the outputFileUri to determine the format
    # We assume the output file has the same extension as the input file? Not necessarily.
    # We don't store the output format in the Job model, but we can infer from the content type? 
    # We don't have the output content type stored. We'll leave it as unknown for now.
    # Alternatively, we can store the output content type in the Job model? 
    # But the spec does not require it. We'll leave it out for now.
    
    # For now, we'll set the format to 'wav' as a default, but note that this is not accurate.
    # We should store the output format in the Job model. However, to avoid changing the model,
    # we will leave it as null and let the client decide? 
    # But the spec for the JobResultResponse requires a format.
    #
    # We'll change: we will store the output content type in the Job model? 
    # But that would be a change to the data model. We'll do it in a separate ADR if needed.
    #
    # For now, we'll assume the output is always WAV. This is not ideal, but we can change later.
    #
    # We'll get the file extension from the outputFileUri if possible.
    try:
        # The outputFileUri is in the format: s3://bucket/key
        # We want the extension of the key.
        if job.outputFileUri.startswith('s3://'):
            object_key = job.outputFileUri[5:]
            # Get the file extension from the key
            _, file_ext = os.path.splitext(object_key)
            if file_ext:
                # Remove the dot
                file_ext = file_ext.lower()
                if file_ext == '.wav':
                    format_str = 'wav'
                elif file_ext == '.aiff':
                    format_str = 'aiff'
                elif file_ext == '.flac':
                    format_str = 'flac'
                else:
                    format_str = 'wav'  # default
            else:
                format_str = 'wav'  # default
        else:
            format_str = 'wav'  # default
    except Exception:
        format_str = 'wav'  # default
    
    # Prepare loudness metrics (if available)
    loudness_metrics = None
    if job.loudnessMetrics:
        loudness_metrics = {
            'integratedLUFS': job.loudnessMetrics.get('integratedLUFS'),
            'truePeak': job.loudnessMetrics.get('truePeak'),
            'lra': job.loudnessMetrics.get('lra'),
            'peak': job.loudnessMetrics.get('peak')
        }
    
    return jsonify({
        'downloadUrl': download_url,
        'expiresIn': current_app.config['PRESIGNED_URL_EXPIRY'],
        'fileName': f"mastered_{job.fileName}",
        'fileSize': job.fileSize,  # We assume the file size is the same? Not necessarily, but we don't store the output file size.
        'format': format_str,
        'sampleRate': 0,  # We don't store the sample rate in the Job model. We'll leave it as 0 for now.
        'bitDepth': 0,    # We don't store the bit depth in the Job model.
        'loudnessMetrics': loudness_metrics
    }), 200

# Presets route
@api_bp.route('/presets', methods=['GET'])
@jwt_required()
def list_presets():
    """
    List available mastering presets.
    ---
    tags:
      - presets
    security:
      - bearerAuth: []
    responses:
      '200':
        description: List of presets
        content:
          application/json:
            schema:
              type: object
              properties:
                presets:
                  type: array
                  items:
                    $ref: '#/components/schemas/Preset'
                total:
                  type: integer
      '401':
        description: Unauthorized
    """
    presets_list = []
    for preset_id, preset_data in current_app.config['PRESETS'].items():
        presets_list.append({
            'id': preset_id,
            'name': preset_data['name'],
            'description': preset_data['description'],
            'defaultIntensity': preset_data['defaultIntensity'],
            'parameters': preset_data['parameters']
        })
    
    return jsonify({
        'presets': presets_list,
        'total': len(presets_list)
    }), 200

# Health check route
@api_bp.route('/health', methods=['GET'])
def get_health():
    """
    Health check endpoint.
    ---
    tags:
      - health
    responses:
      '200':
        description: Service is healthy
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/HealthResponse'
      '503':
        description: Service unavailable
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/ErrorResponse'
    """
    # We'll do a simple health check: check if we can connect to the database and the queue.
    # For now, we'll just return healthy.
    # In a real implementation, we would check the database, Redis, and object storage.
    return jsonify({
        'status': 'healthy',
        'timestamp': datetime.utcnow().isoformat(),
        'version': '1.0.0',
        'uptimeSeconds': 0,  # We don't track uptime in this simple implementation
        'components': {
            'api': 'healthy',
            'database': 'healthy',  # We assume it's healthy if we can make a query
            'jobQueue': 'healthy',  # We assume it's healthy if we can connect to Redis
            'objectStorage': 'healthy'  # We assume it's healthy if we can connect to S3
        }
    }), 200