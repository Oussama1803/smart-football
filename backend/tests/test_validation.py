"""Tests for message validation rules (speed, zones, types, missing fields)."""

import pytest
from app.validation import MessageValidationError, parse_and_validate_shot


def test_valid_shot_parsing(sample_shot_payload):
    """Verify that a specification-compliant shot is accepted."""
    shot = parse_and_validate_shot(sample_shot_payload)
    assert shot.event_id == sample_shot_payload["event_id"]
    assert shot.ball_speed_kmh == 72.0
    assert shot.target_zone == "upper_right"
    assert shot.pressure_reading == 640
    assert shot.impact_intensity == 845


def test_valid_boundary_speeds(sample_shot_payload):
    """Verify valid speed limits [5.0, 150.0] km/h."""
    # Minimum valid speed
    payload_min = sample_shot_payload.copy()
    payload_min["ball_speed_kmh"] = 5.0
    assert parse_and_validate_shot(payload_min).ball_speed_kmh == 5.0

    # Maximum valid speed
    payload_max = sample_shot_payload.copy()
    payload_max["ball_speed_kmh"] = 150.0
    assert parse_and_validate_shot(payload_max).ball_speed_kmh == 150.0


@pytest.mark.parametrize("invalid_speed", [4.9, 0.0, -10.0, 150.1, 200.0, 350.0])
def test_out_of_range_speed_rejected(sample_shot_payload, invalid_speed):
    """SEC2 Requirement: Speeds strictly outside 5 to 150 km/h must be rejected."""
    payload = sample_shot_payload.copy()
    payload["ball_speed_kmh"] = invalid_speed
    with pytest.raises(MessageValidationError) as exc_info:
        parse_and_validate_shot(payload)
    assert "out of valid range" in exc_info.value.message


@pytest.mark.parametrize("valid_zone", [
    "upper_left", "upper_right", "lower_left", "lower_right", "none",
    "UPPER_LEFT", "None",  # case-insensitive
    "center", "upper_center", "lower_center", "middle_left", "middle_right"  # 9-zone extension
])
def test_allowed_target_zones(sample_shot_payload, valid_zone):
    """Verify allowed zones including prototype 4 zones, none, and 9-zone extensions."""
    payload = sample_shot_payload.copy()
    payload["target_zone"] = valid_zone
    shot = parse_and_validate_shot(payload)
    assert shot.target_zone == valid_zone.strip().lower()


@pytest.mark.parametrize("invalid_zone", ["top_bar", "post", "goal_line", "invalid", "", "123"])
def test_invalid_target_zones_rejected(sample_shot_payload, invalid_zone):
    """Verify arbitrary or invalid target zones are rejected."""
    payload = sample_shot_payload.copy()
    payload["target_zone"] = invalid_zone
    with pytest.raises(MessageValidationError) as exc_info:
        parse_and_validate_shot(payload)
    assert "Invalid target_zone" in exc_info.value.message


@pytest.mark.parametrize("missing_field", [
    "event_id", "device_id", "session_id", "timestamp",
    "ball_speed_kmh", "pressure_reading", "target_zone",
    "impact_intensity", "firmware_version"
])
def test_missing_mandatory_fields_rejected(sample_shot_payload, missing_field):
    """All required fields from section 6 must be present."""
    payload = sample_shot_payload.copy()
    del payload[missing_field]
    with pytest.raises(MessageValidationError):
        parse_and_validate_shot(payload)


def test_invalid_raw_adc_values_rejected(sample_shot_payload):
    """Raw ADC readings cannot be negative."""
    payload_neg_pressure = sample_shot_payload.copy()
    payload_neg_pressure["pressure_reading"] = -1
    with pytest.raises(MessageValidationError):
        parse_and_validate_shot(payload_neg_pressure)

    payload_neg_impact = sample_shot_payload.copy()
    payload_neg_impact["impact_intensity"] = -50
    with pytest.raises(MessageValidationError):
        parse_and_validate_shot(payload_neg_impact)


def test_malformed_json_rejected():
    """Malformed JSON string or non-UTF8 input must raise MessageValidationError."""
    with pytest.raises(MessageValidationError):
        parse_and_validate_shot("{invalid json")

    with pytest.raises(MessageValidationError):
        parse_and_validate_shot(b"\xff\xfe\x00")
