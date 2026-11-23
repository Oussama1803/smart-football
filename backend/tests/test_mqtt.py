"""Tests for MQTT message routing and reception logic."""

import json
from unittest.mock import patch
from app.models import DeviceStatus, Shot
from app.mqtt_service import mqtt_service


def test_mqtt_handle_valid_event_message(db_session, sample_shot_payload):
    """A valid MQTT event message received on topic is written to the database."""
    payload_bytes = json.dumps(sample_shot_payload).encode("utf-8")

    with patch("app.mqtt_service.SessionLocal", return_value=db_session):
        mqtt_service._handle_event_message("goal-esp32-01", payload_bytes)

    shot = db_session.query(Shot).filter(Shot.event_id == sample_shot_payload["event_id"]).first()
    assert shot is not None
    assert shot.ball_speed_kmh == 72.0
    assert shot.target_zone == "upper_right"


def test_mqtt_handle_invalid_speed_rejected(db_session, sample_shot_payload):
    """SEC2 Requirement: MQTT event with speed > 150 km/h is rejected, nothing in DB."""
    invalid_payload = sample_shot_payload.copy()
    invalid_payload["ball_speed_kmh"] = 185.0
    payload_bytes = json.dumps(invalid_payload).encode("utf-8")

    with patch("app.mqtt_service.SessionLocal", return_value=db_session):
        mqtt_service._handle_event_message("goal-esp32-01", payload_bytes)

    # Nothing should be inserted in DB
    assert db_session.query(Shot).count() == 0


def test_mqtt_handle_duplicate_event_message(db_session, sample_shot_payload):
    """SUR1 Requirement: Duplicate MQTT message is rejected by anti-doublon."""
    payload_bytes = json.dumps(sample_shot_payload).encode("utf-8")

    with patch("app.mqtt_service.SessionLocal", return_value=db_session):
        mqtt_service._handle_event_message("goal-esp32-01", payload_bytes)
        mqtt_service._handle_event_message("goal-esp32-01", payload_bytes)

    assert db_session.query(Shot).count() == 1


def test_mqtt_handle_status_message(db_session):
    """Device status/heartbeat message is saved in device_status table."""
    status_payload = {
        "device_id": "goal-esp32-01",
        "status": "heartbeat",
        "firmware_version": "0.1.0",
        "uptime_seconds": 3600,
    }
    payload_bytes = json.dumps(status_payload).encode("utf-8")

    with patch("app.mqtt_service.SessionLocal", return_value=db_session):
        mqtt_service._handle_status_message("goal-esp32-01", payload_bytes)

    status_row = db_session.query(DeviceStatus).filter(DeviceStatus.device_id == "goal-esp32-01").first()
    assert status_row is not None
    assert status_row.status == "heartbeat"
    assert status_row.uptime_seconds == 3600
