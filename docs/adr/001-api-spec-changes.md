# ADR-001: Modify API Spec to Include Content Type in Job Creation and Job ID in Upload URL

## Status
Accepted as an API-design decision; implementation and deployment status are not established by this ADR. Verify against the current service code.

## Context
We are implementing the AI-Powered Audio Mastering Service API. During the implementation of the job creation endpoint, we encountered a problem:

- The `JobCreateRequest` in the OpenAPI spec does not include a `contentType` field, but we need to know the content type (audio/wav, audio/aiff, audio/flac) to:
    - Validate that the file is of an allowed type.
    - Determine the file extension for generating the object key in S3.
    - Set the `contentType` field in the `Job` model (which we decided should be non-nullable).

- The `UploadUrlRequest` in the OpenAPI spec does not include a `jobId` field, but we need to associate the uploaded file with a specific job to:
    - Set the `inputFileUri` in the `Job` model (which points to the object in S3).
    - Ensure that the file uploaded by the user is correctly linked to the job they created.

Without these fields, we cannot reliably associate the uploaded file with the job, leading to potential data inconsistency or incorrect processing.

## Decision
We will modify the OpenAPI spec as follows:

1. Add `contentType` to the `JobCreateRequest` object.
   - This will allow the client to specify the content type of the file when creating the job.
   - We will validate that the content type is one of the allowed types.
   - We will use the content type to determine the file extension for generating the S3 object key.

2. Add `jobId` to the `UploadUrlRequest` object.
   - This will allow the client to specify which job the upload is for.
   - We will use the job ID to generate the S3 object key for the input file.
   - We will ensure that the job belongs to the current user and is in the correct state (pending).

## Consequences
### Positive
- The API will be able to correctly associate uploaded files with jobs.
- The `Job` model will have a non-nullable `contentType` field, ensuring data integrity.
- The `inputFileUri` in the `Job` model can be set accurately during job creation (since we know the job ID and can generate the object key).
- The implementation will be simpler and more reliable.

### Negative
- Clients will need to be updated to include the new fields in their requests.
- This is a breaking change to the API, so we will need to version the API accordingly (e.g., introduce v2) or ensure that all clients are updated before deploying.

However, since this is still in development and we have not yet released the API to external clients, we can make this change without affecting existing users.

## Implementation Plan
1. Update the OpenAPI specification (`api-spec.yaml`) to add the `contentType` field to `JobCreateRequest` and the `jobId` field to `UploadUrlRequest`.
2. Update the `Job` model in `backend/models.py` to ensure `contentType` is non-nullable (as we will now set it during job creation).
3. Update the job creation endpoint to use the provided `contentType` to validate the file type and generate the S3 object key for `inputFileUri`.
4. Update the upload URL endpoint to use the provided `jobId` to generate the S3 object key and verify that the job exists and belongs to the user.
5. Update any relevant documentation.

## Related Documents
- PRD.md
- SYSTEM_DESIGN.md
- API_DESIGN.md
- data-model.md

## Acceptance Criteria
- Given a client creates a job with a valid content type, the job is created with the correct content type and inputFileUri.
- Given a client requests an upload URL with a valid job ID, the system returns a presigned URL for the correct object key associated with that job.
- Given a client attempts to create a job with an invalid content type, the system returns a validation error.
- Given a client attempts to request an upload URL for a job that does not exist or does not belong to them, the system returns an error.
