"""Validation helpers for incoming MQTT and HTTP payloads."""

import json
import logging
from typing import Any, Dict, Tuple
from pydantic import ValidationError

from app.schemas import ShotCreate, DeviceStatusCreate, KeeperEventCreate

logger = logging.getLogger(__name__)


class MessageValidationError(Exception):
    """Custom exception raised when payload validation fails."""

    def __init__(self, message: str, raw_payload: Any = None):
        super().__init__(message)
        self.message = message
        self.raw_payload = raw_payload


def parse_and_validate_shot(payload: bytes | str | dict) -> ShotCreate:
    """Parse JSON and validate shot structure, types, and business constraints."""
    if isinstance(payload, (bytes, bytearray)):
        try:
            payload_str = payload.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise MessageValidationError(f"Payload is not valid UTF-8: {exc}") from exc
        try:
            data = json.loads(payload_str)
        except json.JSONDecodeError as exc:
            raise MessageValidationError(f"Payload is not valid JSON: {exc}") from exc
    elif isinstance(payload, str):
        try:
            data = json.loads(payload)
        except json.JSONDecodeError as exc:
            raise MessageValidationError(f"Payload is not valid JSON: {exc}") from exc
    elif isinstance(payload, dict):
        data = payload
    else:
        raise MessageValidationError(f"Unsupported payload type: {type(payload)}")

    if not isinstance(data, dict):
        raise MessageValidationError(f"JSON payload must be an object, got {type(data).__name__}")

    try:
        shot = ShotCreate(**data)
        return shot
    except ValidationError as exc:
        errors = "; ".join(f"{err['loc']}: {err['msg']}" for err in exc.errors())
        raise MessageValidationError(f"Validation failed: {errors}", raw_payload=data) from exc


def parse_and_validate_status(payload: bytes | str | dict, device_id_from_topic: str) -> DeviceStatusCreate:
    """Parse device status or heartbeat message."""
    data: Dict[str, Any] = {}
    if isinstance(payload, (bytes, bytearray)):
        try:
            payload_str = payload.decode("utf-8")
            data = json.loads(payload_str) if payload_str.strip().startswith("{") else {"status": payload_str}
        except Exception:
            data = {"status": str(payload)}
    elif isinstance(payload, str):
        try:
            data = json.loads(payload) if payload.strip().startswith("{") else {"status": payload}
        except Exception:
            data = {"status": payload}
    elif isinstance(payload, dict):
        data = payload

    device_id = data.get("device_id") or device_id_from_topic
    return DeviceStatusCreate(
        device_id=device_id,
        status=str(data.get("status", "heartbeat")),
        firmware_version=data.get("firmware_version"),
        uptime_seconds=data.get("uptime_seconds"),
        timestamp=data.get("timestamp"),
        payload=json.dumps(data) if isinstance(data, dict) else str(data),
    )


def parse_and_validate_keeper_event(payload: bytes | str | dict) -> KeeperEventCreate:
    """Parse JSON and validate goalkeeper MPU6050 motion structure."""
    if isinstance(payload, (bytes, bytearray)):
        try:
            payload_str = payload.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise MessageValidationError(f"Payload is not valid UTF-8: {exc}") from exc
        try:
            data = json.loads(payload_str)
        except json.JSONDecodeError as exc:
            raise MessageValidationError(f"Payload is not valid JSON: {exc}") from exc
    elif isinstance(payload, str):
        try:
            data = json.loads(payload)
        except json.JSONDecodeError as exc:
            raise MessageValidationError(f"Payload is not valid JSON: {exc}") from exc
    elif isinstance(payload, dict):
        data = payload
    else:
        raise MessageValidationError(f"Unsupported payload type: {type(payload)}")

    if not isinstance(data, dict):
        raise MessageValidationError(f"JSON payload must be an object, got {type(data).__name__}")

    try:
        event = KeeperEventCreate(**data)
        return event
    except ValidationError as exc:
        errors = "; ".join(f"{err['loc']}: {err['msg']}" for err in exc.errors())
        raise MessageValidationError(f"Keeper validation failed: {errors}", raw_payload=data) from exc

