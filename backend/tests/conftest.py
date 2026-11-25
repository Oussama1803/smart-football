"""Pytest fixtures and test database setup."""

import os
import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

# Force test configuration
os.environ["ENABLE_MQTT_SUBSCRIBER"] = "false"
os.environ["DATABASE_URL"] = "sqlite:///:memory:"

from app.database import Base, get_db
from app.main import app

# In-memory SQLite engine for tests
test_engine = create_engine(
    "sqlite:///:memory:",
    connect_args={"check_same_thread": False},
    poolclass=StaticPool,
)
TestingSessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=test_engine)


@pytest.fixture(autouse=True)
def setup_test_db():
    """Create fresh database tables before each test and drop them after."""
    Base.metadata.create_all(bind=test_engine)
    yield
    Base.metadata.drop_all(bind=test_engine)


@pytest.fixture
def db_session():
    """Yield a standalone database session for direct model/service tests."""
    db = TestingSessionLocal()
    try:
        yield db
    finally:
        db.close()


@pytest.fixture
def client(db_session):
    """Yield a FastAPI TestClient configured with the test database session."""

    def override_get_db():
        try:
            yield db_session
        finally:
            pass

    app.dependency_overrides[get_db] = override_get_db
    with TestClient(app) as test_client:
        yield test_client
    app.dependency_overrides.clear()


@pytest.fixture
def sample_shot_payload():
    """Valid standard shot message from Smart Football specification."""
    return {
        "event_id": "b3f1c2a0-7d4e-4f6a-9c1e-2a5d8e7f0b11",
        "device_id": "goal-esp32-01",
        "session_id": "session-001",
        "event_type": "shot",
        "timestamp": "2026-11-10T14:30:00Z",
        "ball_speed_kmh": 72.0,
        "pressure_reading": 640,
        "target_zone": "upper_right",
        "impact_intensity": 845,
        "firmware_version": "0.1.0",
    }
