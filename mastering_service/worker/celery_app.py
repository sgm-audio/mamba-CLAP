from celery import Celery

from mastering_service.core.config import settings

celery_app = Celery("mastering_worker", broker=settings.CELERY_BROKER_URL)
celery_app.conf.update(
    result_backend=settings.CELERY_RESULT_BACKEND,
    task_serializer="json",
    accept_content=["json"],
    result_serializer="json",
    timezone="UTC",
    enable_utc=True,
)

# Auto-discover tasks
celery_app.autodiscover_tasks(["mastering_service.worker"])
