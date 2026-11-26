"""Pydantic schemas for data validation and API serialization."""

from datetime import datetime
from typing import Any, Dict, List, Optional
from pydantic import BaseModel, ConfigDict, Field, field_validator

from app.config import settings


class ShotBase(BaseModel):
    """Base schema for football shot events."""

    event_id: str = Field(..., description="Unique UUID identifier for the shot")
    device_id: str = Field(..., min_length=1, description="Identifier of the emitting ESP32 device")
    session_id: str = Field(..., min_length=1, description="Training session identifier")
    event_type: str = Field(default="shot", description="Type of event ('shot')")
    timestamp: datetime = Field(..., description="ISO 8601 timestamp of when the shot occurred")
    ball_speed_kmh: float = Field(
        ...,
        description="Measured ball speed in km/h between IR beams (valid: 5.0 to 150.0 km/h)",
    )
    pressure_reading: int = Field(
        ...,
        ge=0,
        description="Raw ADC reading from the FSR sensor representing relative pressure (not Newtons)",
    )
    target_zone: str = Field(
        ...,
        description="Goal target zone: upper_left, upper_right, lower_left, lower_right, or none",
    )
    impact_intensity: int = Field(
        ...,
        ge=0,
        description="Raw ADC reading from piezoelectric sensor representing relative impact (not Newtons)",
    )
    firmware_version: str = Field(..., min_length=1, description="ESP32 firmware version")

    @field_validator("ball_speed_kmh")
    @classmethod
    def validate_speed(cls, value: float) -> float:
        if not (settings.MIN_SPEED_KMH <= value <= settings.MAX_SPEED_KMH):
            raise ValueError(
                f"Ball speed {value:.1f} km/h is out of valid range [{settings.MIN_SPEED_KMH}, {settings.MAX_SPEED_KMH}] km/h"
            )
        return round(value, 2)

    @field_validator("target_zone")
    @classmethod
    def validate_zone(cls, value: str) -> str:
        cleaned_zone = value.strip().lower()
        if cleaned_zone not in settings.ALLOWED_TARGET_ZONES:
            raise ValueError(
                f"Invalid target_zone '{value}'. Allowed zones: {settings.ALLOWED_TARGET_ZONES}"
            )
        return cleaned_zone

    @field_validator("event_type")
    @classmethod
    def validate_event_type(cls, value: str) -> str:
        if value.strip().lower() != "shot":
            raise ValueError(f"Invalid event_type '{value}', expected 'shot'")
        return value.strip().lower()


class ShotCreate(ShotBase):
    """Schema used when ingesting a new shot."""
    pass


class ShotResponse(ShotBase):
    """Schema returned by REST API for shot records."""

    id: int
    received_at: datetime
    latency_ms: Optional[float] = None
    model_config = ConfigDict(from_attributes=True)


class ShotListResponse(BaseModel):
    """Paginated list of shots."""

    total: int
    limit: int
    offset: int
    shots: List[ShotResponse]


class DeviceStatusCreate(BaseModel):
    """Schema for status/heartbeat messages."""

    device_id: str
    status: str = "heartbeat"
    firmware_version: Optional[str] = None
    uptime_seconds: Optional[int] = None
    timestamp: Optional[datetime] = None
    payload: Optional[str] = None


class DeviceStatusResponse(BaseModel):
    """Schema returned for device status."""

    id: int
    device_id: str
    status: str
    firmware_version: Optional[str] = None
    uptime_seconds: Optional[int] = None
    timestamp: Optional[datetime] = None
    received_at: datetime
    model_config = ConfigDict(from_attributes=True)


class AccuracyStats(BaseModel):
    """Statistics on shot accuracy and zone distribution."""

    total_shots: int
    shots_on_target: int
    missed_shots: int
    accuracy_rate_pct: float
    avg_speed_kmh: float
    max_speed_kmh: float
    min_speed_kmh: float
    avg_pressure_reading: float
    avg_impact_intensity: float
    zone_distribution: Dict[str, int]
    data_classification: Dict[str, str] = {
        "speed": "Mesure réelle (IR break-beam)",
        "zones": "Mesure réelle (Piézo)",
        "pressure": "Valeur brute relative ADC [0-4095] (jamais en N)",
        "impact": "Valeur brute relative ADC [0-4095] (jamais en N)",
    }


class SessionSummary(BaseModel):
    """Summary metrics for a specific training session."""

    session_id: str
    total_shots: int
    shots_on_target: int
    accuracy_rate_pct: float
    avg_speed_kmh: float
    max_speed_kmh: float
    avg_pressure_reading: float
    avg_impact_intensity: float
    first_shot_at: Optional[datetime] = None
    last_shot_at: Optional[datetime] = None


class IngestionResult(BaseModel):
    """Result of shot ingestion."""

    status: str  # "created" or "duplicate_ignored"
    event_id: str
    message: str


class KeeperEventBase(BaseModel):
    """Base schema for goalkeeper MPU6050 telemetry events (Étape 11)."""

    event_id: str = Field(..., description="Unique UUID identifier for the keeper event")
    device_id: str = Field(..., min_length=1, description="Identifier of the emitting keeper ESP32 device")
    session_id: str = Field(..., min_length=1, description="Training session identifier")
    event_type: str = Field(default="keeper_motion", description="Type of event ('keeper_motion')")
    timestamp: datetime = Field(..., description="ISO 8601 timestamp of when the motion was recorded")
    accel_x: float = Field(..., description="Acceleration X in g (+/- 2g range)")
    accel_y: float = Field(..., description="Acceleration Y in g (+/- 2g range)")
    accel_z: float = Field(..., description="Acceleration Z in g (+/- 2g range)")
    gyro_x: float = Field(..., description="Angular velocity X in deg/s (+/- 250 dps)")
    gyro_y: float = Field(..., description="Angular velocity Y in deg/s (+/- 250 dps)")
    gyro_z: float = Field(..., description="Angular velocity Z in deg/s (+/- 250 dps)")
    dive_direction: str = Field(
        default="none",
        description="Detected motion/dive direction: none, left, right, jump, dive",
    )
    firmware_version: str = Field(..., min_length=1, description="Keeper ESP32 firmware version")

    @field_validator("dive_direction")
    @classmethod
    def validate_dive(cls, value: str) -> str:
        cleaned = value.strip().lower()
        allowed = {"none", "left", "right", "jump", "dive"}
        if cleaned not in allowed:
            raise ValueError(f"dive_direction must be one of {allowed}, got '{value}'")
        return cleaned


class KeeperEventCreate(KeeperEventBase):
    """Schema for creating a keeper event from MQTT or HTTP."""
    pass


class KeeperEventResponse(KeeperEventBase):
    """Schema for returning keeper events via API."""

    id: int
    received_at: datetime

    model_config = ConfigDict(from_attributes=True)


class KeeperIngestionResult(BaseModel):
    """Result of keeper event ingestion."""

    status: str  # "created" or "duplicate_ignored"
    event_id: str
    message: str

