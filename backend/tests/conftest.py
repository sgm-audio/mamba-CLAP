"""
Pytest configuration and fixtures for the backend tests.
"""
import pytest
from backend.app import create_app, db as _db
from backend.config import TestingConfig

@pytest.fixture
def app():
    """
    Create and configure a new app instance for each test.
    """
    app = create_app('testing')
    with app.app_context():
        _db.create_all()
        yield app
        _db.session.remove()
        _db.drop_all()

@pytest.fixture
def client(app):
    """
    A test client for the app.
    """
    return app.test_client()

@pytest.fixture
def runner(app):
    """
    A test runner for the app's Click commands.
    """
    return app.test_cli_runner()

@pytest.fixture
def db(app):
    """
    Return the database object for the app.
    """
    with app.app_context():
        yield _db