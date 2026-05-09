#!/usr/bin/env python3
"""Recover stuck jobs (orphaned processing state)."""
import logging
import sys
from datetime import datetime, timedelta

from backend.app import create_app, db
from backend.models import Job

logger = logging.getLogger(__name__)

def recover_stuck_jobs(timeout_minutes: int = 30) -> int:
    """Mark jobs stuck in 'processing' for > timeout as 'failed'."""
    app = create_app('production')
    with app.app_context():
        cutoff = datetime.utcnow() - timedelta(minutes=timeout_minutes)
        
        stuck_jobs = Job.query.filter(
            Job.status == 'processing',
            Job.startedAt < cutoff
        ).all()
        
        for job in stuck_jobs:
            logger.warning(
                f"Recovering stuck job {job.id} "
                f"(worker: {job.processing_worker_id}, started: {job.startedAt})"
            )
            job.status = 'failed'
            job.errorMessage = f'Worker timeout after {timeout_minutes} minutes'
            job.completedAt = datetime.utcnow()
        
        try:
            db.session.commit()
            logger.info(f"Recovered {len(stuck_jobs)} stuck jobs")
            return len(stuck_jobs)
        except Exception as e:
            logger.error(f"Failed to commit recovered jobs: {e}")
            db.session.rollback()
            return -1

if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, stream=sys.stdout)
    count = recover_stuck_jobs()
    exit(0 if count >= 0 else 1)
