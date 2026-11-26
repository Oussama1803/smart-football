"""Unit and integration tests for Goalkeeper MPU6050 telemetry (Étape 11)."""

import json
import pytest
from app.schemas import KeeperEventCreate
from app.validation import MessageValidationError, parse_and_validate_keeper_event
from app.services import record_keeper_event
from app.models import KeeperEvent
from app.mqtt_service import mqtt_service


@pytest.fixture
def valid_keeper_payload():
    return {
        "event_id": "test-keeper-evt-001",
        "device_id": "keeper-esp32-01",
        "session_id": "session-test",
        "event_type": "keeper_motion",
        "timestamp": "2026-11-26T10:00:00Z",
        "accel_x": 0.12,
        "accel_y": -0.98,
        "accel_z": 0.05,
        "gyro_x": 12.5,
        "gyro_y": -4.2,
        "gyro_z": 45.0,
        "dive_direction": "right",
        "firmware_version": "0.1.0",
    }


def test_validate_valid_keeper_payload(valid_keeper_payload):
    """Ensure valid keeper payload parses without error."""
    event = parse_and_validate_keeper_event(valid_keeper_payload)
    assert event.event_id == "test-keeper-evt-001"
    assert event.accel_y == -0.98
    assert event.dive_direction == "right"


def test_validate_invalid_dive_direction(valid_keeper_payload):
    """Ensure invalid dive direction is rejected."""
    bad_payload = dict(valid_keeper_payload)
    bad_payload["dive_direction"] = "somersault"
    with pytest.raises(MessageValidationError):
        parse_and_validate_keeper_event(bad_payload)


def test_validate_malformed_json():
    """Ensure invalid JSON raises MessageValidationError."""
    with pytest.raises(MessageValidationError):
        parse_and_validate_keeper_event(b"not a json")


def test_record_keeper_event_deduplication(db_session, valid_keeper_payload):
    """Test SUR1 anti-doublon idempotency for keeper events."""
    event_in = KeeperEventCreate(**valid_keeper_payload)
    
    event, is_created = record_keeper_event(db_session, event_in)
    assert is_created is True
    assert event.event_id == valid_keeper_payload["event_id"]

    # Re-inserting the same event_id must be ignored
    dup_event, dup_created = record_keeper_event(db_session, event_in)
    assert dup_created is False
    assert dup_event.id == event.id
    assert db_session.query(KeeperEvent).count() == 1


def test_api_ingest_and_query_keeper_events(client, valid_keeper_payload):
    """Test POST /api/keeper/events and GET /api/keeper/events /latest."""
    # Ingest via API
    res = client.post("/api/keeper/events", json=valid_keeper_payload)
    assert res.status_code == 201
    assert res.json()["status"] == "created"

    # Duplicate POST
    res_dup = client.post("/api/keeper/events", json=valid_keeper_payload)
    assert res_dup.status_code == 201
    assert res_dup.json()["status"] == "duplicate_ignored"

    # Query latest
    res_latest = client.get("/api/keeper/latest")
    assert res_latest.status_code == 200
    latest = res_latest.json()
    assert latest["event_id"] == valid_keeper_payload["event_id"]
    assert latest["dive_direction"] == "right"

    # Query list
    res_list = client.get("/api/keeper/events")
    assert res_list.status_code == 200
    assert len(res_list.json()) == 1


def test_mqtt_routing_for_keeper_events(db_session, valid_keeper_payload, monkeypatch):
    """Test that MQTT service routes keeper events correctly."""
    from app import mqtt_service as mqtt_module
    monkeypatch.setattr(mqtt_module, "SessionLocal", lambda: db_session)

    raw_bytes = json.dumps(valid_keeper_payload).encode("utf-8")
    mqtt_service._handle_event_message("keeper-esp32-01", raw_bytes)

    saved = db_session.query(KeeperEvent).filter(KeeperEvent.event_id == valid_keeper_payload["event_id"]).first()
    assert saved is not None
    assert saved.dive_direction == "right"
