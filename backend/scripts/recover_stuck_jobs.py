#!/usr/bin/env python3
"""
Job Stuck-State Recovery Daemon

Scans for jobs that have been in 'processing' state for too long
(presumed dead/orphaned) and marks them as 'failed'.

Run via cron every 5 minutes:
    */5 * * * * /var/home/SGM/dev/Mamba Based/venv/bin/python /var/home/SGM/dev/Mamba Based/backend/scripts/recover_stuck_jobs.py

Or via systemd timer (see backend/scripts/recover-stuck-jobs.timer.yaml).
"""
import os
import sys
import argparse
import logging
from datetime import datetime, timedelta, timezone

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from backend import db
from backend.app import create_app
from backend.models import Job

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(name)s: %(message)s'
)
logger = logging.getLogger('recover_stuck_jobs')


STUCK_THRESHOLD_MINUTES = int(os.environ.get('STUCK_JOB_THRESHOLD_MINUTES', 30))
MAX_JOBS_PER_RUN = int(os.environ.get('MAX_JOBS_RECOVERY_PER_RUN', 100))


def recover_stuck_jobs(app, threshold_minutes=STUCK_THRESHOLD_MINUTES, max_per_run=MAX_JOBS_PER_RUN):
    """
    Find and mark stuck jobs as failed.

    Args:
        app: Flask application with app context
        threshold_minutes: Minutes a job can be 'processing' before considered stuck
        max_per_run: Maximum jobs to recover in a single run

    Returns:
        tuple: (found_count, recovered_count)
    """
    threshold_time = datetime.now(timezone.utc) - timedelta(minutes=threshold_minutes)

    with app.app_context():
        stuck_jobs = Job.query.filter(
            Job.status == 'processing',
            Job.startedAt < threshold_time
        ).limit(max_per_run).all()

        found_count = len(stuck_jobs)

        if found_count == 0:
            logger.info("No stuck jobs found")
            return 0, 0

        logger.info(f"Found {found_count} stuck job(s) - marking as failed")

        recovered_count = 0
        for job in stuck_jobs:
            job.status = 'failed'
            job.completedAt = datetime.now(timezone.utc)
            job.errorMessage = (
                f"Job processing timeout - stuck in 'processing' state for more than "
                f"{threshold_minutes} minutes. This may indicate a worker crash or "
                f"network issue during processing."
            )
            logger.warning(
                f"Recovered stuck job {job.id} (user={job.userId}, "
                f"startedAt={job.startedAt.isoformat() if job.startedAt else 'None'})"
            )
            recovered_count += 1

        db.session.commit()
        logger.info(f"Recovery complete: {recovered_count} job(s) marked as failed")

        return found_count, recovered_count


def main():
    parser = argparse.ArgumentParser(
        description='Recover jobs stuck in processing state'
    )
    parser.add_argument(
        '--threshold-minutes',
        type=int,
        default=STUCK_THRESHOLD_MINUTES,
        help=f"Minutes a job can be processing before considered stuck (default: {STUCK_THRESHOLD_MINUTES})"
    )
    parser.add_argument(
        '--max-per-run',
        type=int,
        default=MAX_JOBS_PER_RUN,
        help=f"Maximum jobs to recover per run (default: {MAX_JOBS_PER_RUN})"
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help="Show what would be recovered without making changes"
    )
    parser.add_argument(
        '--config',
        choices=['development', 'testing', 'production'],
        default=os.environ.get('FLASK_ENV', 'development'),
        help="Configuration to use"
    )

    args = parser.parse_args()

    if args.dry_run:
        logger.info("DRY RUN - no changes will be made")

    env = os.environ.get('FLASK_ENV', args.config)
    app = create_app(env)

    if args.dry_run:
        with app.app_context():
            threshold_time = datetime.now(timezone.utc) - timedelta(minutes=args.threshold_minutes)
            stuck_jobs = Job.query.filter(
                Job.status == 'processing',
                Job.startedAt < threshold_time
            ).limit(args.max_per_run).all()
            if stuck_jobs:
                logger.info(f"DRY RUN: Would recover {len(stuck_jobs)} stuck job(s):")
                for job in stuck_jobs:
                    logger.info(
                        f"  - Job {job.id} (user={job.userId}, "
                        f"startedAt={job.startedAt.isoformat() if job.startedAt else 'None'}, "
                        f"processing_worker={job.processing_worker_id})"
                    )
            else:
                logger.info("DRY RUN: No stuck jobs found")
        return 0

    found, recovered = recover_stuck_jobs(
        app,
        threshold_minutes=args.threshold_minutes,
        max_per_run=args.max_per_run
    )

    return 0 if recovered == found else 1


if __name__ == '__main__':
    sys.exit(main())