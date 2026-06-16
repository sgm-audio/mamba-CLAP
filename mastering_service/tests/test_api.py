import uuid

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from mastering_service.api.deps import get_db
from mastering_service.core.database import Base
from mastering_service.core.models import User
from mastering_service.core.security import create_access_token
from mastering_service.main import app

# Test database
SQLALCHEMY_DATABASE_URL = "sqlite:///./test.db"
engine = create_engine(SQLALCHEMY_DATABASE_URL, connect_args={"check_same_thread": False})
TestingSessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)


@pytest.fixture
def db():
    Base.metadata.create_all(bind=engine)
    db = TestingSessionLocal()
    try:
        yield db
    finally:
        db.close()
        Base.metadata.drop_all(bind=engine)


@pytest.fixture
def client(db):
    def override_get_db():
        try:
            yield db
        finally:
            pass

    app.dependency_overrides[get_db] = override_get_db
    return TestClient(app)


@pytest.fixture
def test_user(db):
    user = User(id=uuid.uuid4(), email="test@example.com", password_hash="hash", tier="free")
    db.add(user)
    db.commit()
    db.refresh(user)
    return user


@pytest.fixture
def auth_header(test_user):
    token = create_access_token(subject=test_user.id)
    return {"Authorization": f"Bearer {token}"}


def test_health_check(client):
    response = client.get("/health")
    assert response.status_code == 200
    assert response.json() == {"status": "healthy"}


def test_request_upload_url(client, auth_header):
    response = client.post("/v1/uploads/request", headers=auth_header)
    assert response.status_code == 200
    assert "upload_url" in response.json()
    assert "file_key" in response.json()


def test_create_job(client, auth_header, test_user):
    # Mock file key
    file_key = f"uploads/{test_user.id}/test.wav"

    payload = {
        "file_key": file_key,
        "parameters": {"preset_id": "transparent", "intensity": 75, "loudness_target": -14.0},
    }

    # Note: This will attempt to call .delay() on the mock celery task.
    # In a real test we'd mock the task.
    with pytest.MonkeyPatch().context() as mp:
        mp.setattr("mastering_service.worker.tasks.master_audio.delay", lambda x: None)
        response = client.post("/v1/jobs", json=payload, headers=auth_header)

    assert response.status_code == 202
    data = response.json()
    assert data["preset_id"] == "transparent"
    assert data["status"] == "PENDING"


def test_list_jobs(client, auth_header):
    response = client.get("/v1/jobs", headers=auth_header)
    assert response.status_code == 200
    data = response.json()
    assert "items" in data
    assert "total" in data
