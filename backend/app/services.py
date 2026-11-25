"""Business services for persisting shots and handling deduplication."""

import logging
from datetime import datetime, timezone
from typing import Optional, Tuple
from sqlalchemy.orm import Session
from sqlalchemy.exc import IntegrityError

from app.models import Shot, DeviceStatus
from app.schemas import ShotCreate, DeviceStatusCreate

logger = logging.getLogger(__name__)


def record_shot(db: Session, shot_in: ShotCreate) -> Tuple[Optional[Shot], bool]:
    """
    Persist a validated shot into the database with strict deduplication (SUR1).

    Returns:
        (shot_instance, is_created)
        is_created is True if newly inserted, False if duplicate was ignored.
    """
    # Check if event_id already exists (Anti-doublon)
    existing = db.query(Shot).filter(Shot.event_id == shot_in.event_id).first()
    if existing:
        logger.info(
            "SUR1 Anti-doublon: Duplicate event_id=%s from device=%s ignored.",
            shot_in.event_id,
            shot_in.device_id,
        )
        return existing, False

    now_utc = datetime.now(timezone.utc)
    new_shot = Shot(
        event_id=shot_in.event_id,
        device_id=shot_in.device_id,
        session_id=shot_in.session_id,
        event_type=shot_in.event_type,
        timestamp=shot_in.timestamp,
        ball_speed_kmh=shot_in.ball_speed_kmh,
        pressure_reading=shot_in.pressure_reading,
        target_zone=shot_in.target_zone,
        impact_intensity=shot_in.impact_intensity,
        firmware_version=shot_in.firmware_version,
        received_at=now_utc,
    )

    try:
        db.add(new_shot)
        db.commit()
        db.refresh(new_shot)
        
        # Verify ENF1 requirement: latency between shot timestamp and reception < 2.0 s
        if shot_in.timestamp.tzinfo is None:
            shot_ts = shot_in.timestamp.replace(tzinfo=timezone.utc)
        else:
            shot_ts = shot_in.timestamp
        latency_sec = abs((now_utc - shot_ts).total_seconds())
        logger.info(
            "Shot stored: event_id=%s, speed=%.1f km/h, zone=%s, ENF1 latency=%.3f s",
            new_shot.event_id,
            new_shot.ball_speed_kmh,
            new_shot.target_zone,
            latency_sec,
        )
        return new_shot, True

    except IntegrityError:
        db.rollback()
        # Handle concurrent race condition for unique constraint
        existing = db.query(Shot).filter(Shot.event_id == shot_in.event_id).first()
        logger.info(
            "SUR1 Anti-doublon (Race condition): event_id=%s already in DB, skipped duplicate.",
            shot_in.event_id,
        )
        return existing, False


def record_device_status(db: Session, status_in: DeviceStatusCreate) -> DeviceStatus:
    """Store device status, heartbeat, or error in database."""
    now_utc = datetime.now(timezone.utc)
    record = DeviceStatus(
        device_id=status_in.device_id,
        status=status_in.status,
        firmware_version=status_in.firmware_version,
        uptime_seconds=status_in.uptime_seconds,
        payload=status_in.payload,
        timestamp=status_in.timestamp or now_utc,
        received_at=now_utc,
    )
    db.add(record)
    db.commit()
    db.refresh(record)
    return record
