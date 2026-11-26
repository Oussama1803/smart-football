"""SQLAlchemy models for Smart Football."""

from datetime import datetime, timezone
from sqlalchemy import Column, DateTime, Float, Index, Integer, String, Text
from sqlalchemy.sql import func

from app.database import Base


def utcnow():
    """Return timezone-aware current UTC time."""
    return datetime.now(timezone.utc)


class Shot(Base):
    """Database model for ball shots measured by the smart goal."""

    __tablename__ = "shots"

    id = Column(Integer, primary_key=True, index=True, autoincrement=True)
    # Deduplication requirement (SUR1): event_id must be strictly unique.
    event_id = Column(String(64), unique=True, index=True, nullable=False)
    device_id = Column(String(64), index=True, nullable=False)
    session_id = Column(String(64), index=True, nullable=False)
    event_type = Column(String(32), default="shot", nullable=False)
    timestamp = Column(DateTime(timezone=True), nullable=False)
    ball_speed_kmh = Column(Float, nullable=False)
    # pressure_reading: raw ADC value (relative pressure, not Newtons)
    pressure_reading = Column(Integer, nullable=False)
    # target_zone: upper_left, upper_right, lower_left, lower_right, or none
    target_zone = Column(String(32), nullable=False)
    # impact_intensity: raw ADC value (relative impact, not Newtons)
    impact_intensity = Column(Integer, nullable=False)
    firmware_version = Column(String(32), nullable=False)
    # Server receipt timestamp used to verify latency ENF1 (< 2s)
    received_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)

    __table_args__ = (
        Index("idx_shots_session_timestamp", "session_id", "timestamp"),
        Index("idx_shots_device_timestamp", "device_id", "timestamp"),
    )


class DeviceStatus(Base):
    """Database model for device heartbeats, status, errors and Last Will."""

    __tablename__ = "device_status"

    id = Column(Integer, primary_key=True, index=True, autoincrement=True)
    device_id = Column(String(64), index=True, nullable=False)
    status = Column(String(32), nullable=False)  # online, heartbeat, offline, error
    firmware_version = Column(String(32), nullable=True)
    uptime_seconds = Column(Integer, nullable=True)
    payload = Column(Text, nullable=True)
    timestamp = Column(DateTime(timezone=True), nullable=True)
    received_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)


class KeeperEvent(Base):
    """Database model for goalkeeper MPU6050 motion and dive telemetry (Étape 11)."""

    __tablename__ = "keeper_events"

    id = Column(Integer, primary_key=True, index=True, autoincrement=True)
    event_id = Column(String(64), unique=True, index=True, nullable=False)
    device_id = Column(String(64), index=True, nullable=False)
    session_id = Column(String(64), index=True, nullable=False)
    event_type = Column(String(32), default="keeper_motion", nullable=False)
    timestamp = Column(DateTime(timezone=True), nullable=False)
    # Accelerations in g (+/- 2g range)
    accel_x = Column(Float, nullable=False)
    accel_y = Column(Float, nullable=False)
    accel_z = Column(Float, nullable=False)
    # Angular velocity in deg/s (+/- 250 dps range)
    gyro_x = Column(Float, nullable=False)
    gyro_y = Column(Float, nullable=False)
    gyro_z = Column(Float, nullable=False)
    # Inferred or reported action: none, left, right, jump, dive
    dive_direction = Column(String(32), default="none", nullable=False)
    firmware_version = Column(String(32), nullable=False)
    received_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)

    __table_args__ = (
        Index("idx_keeper_session_timestamp", "session_id", "timestamp"),
        Index("idx_keeper_device_timestamp", "device_id", "timestamp"),
    )
