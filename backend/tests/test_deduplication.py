"""Tests for deduplication and idempotence (SUR1 requirement)."""

import uuid
from app.models import Shot
from app.schemas import ShotCreate
from app.services import record_shot


def test_single_shot_creation(db_session, sample_shot_payload):
    """A valid shot creates a single row in the database."""
    shot_in = ShotCreate(**sample_shot_payload)
    shot, is_created = record_shot(db_session, shot_in)

    assert is_created is True
    assert shot.event_id == sample_shot_payload["event_id"]
    assert db_session.query(Shot).count() == 1


def test_duplicate_shot_ignored(db_session, sample_shot_payload):
    """SUR1 Requirement: A duplicate event_id must not create a duplicate row."""
    shot_in = ShotCreate(**sample_shot_payload)

    # First insertion
    shot1, is_created1 = record_shot(db_session, shot_in)
    assert is_created1 is True
    assert db_session.query(Shot).count() == 1

    # Second insertion with identical event_id (e.g., retransmission after network reconnect)
    shot2, is_created2 = record_shot(db_session, shot_in)
    assert is_created2 is False
    assert shot2.id == shot1.id
    # Ensure database still contains strictly 1 row
    assert db_session.query(Shot).count() == 1


def test_sur1_retransmission_ten_shots(db_session, sample_shot_payload):
    """
    SUR1 Requirement: 10 shots transmitted, then retransmitted after network recovery.
    Must result in exactly 10 rows in database, with 0 duplicates.
    """
    shot_list = []
    # Create 10 distinct shot payloads
    for i in range(10):
        p = sample_shot_payload.copy()
        p["event_id"] = str(uuid.uuid4())
        p["ball_speed_kmh"] = 60.0 + i * 2.0
        shot_list.append(ShotCreate(**p))

    # First burst (10 shots)
    for shot in shot_list:
        _, is_created = record_shot(db_session, shot)
        assert is_created is True

    assert db_session.query(Shot).count() == 10

    # Second burst: retransmission of all 10 shots
    for shot in shot_list:
        _, is_created = record_shot(db_session, shot)
        assert is_created is False

    # Count must remain exactly 10
    assert db_session.query(Shot).count() == 10
