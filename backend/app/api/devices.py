"""Endpoints for inspecting connected devices and their operational status."""

from typing import Any, Dict, List
from fastapi import APIRouter, Depends
from sqlalchemy import func
from sqlalchemy.orm import Session

from app.database import get_db
from app.models import DeviceStatus, Shot

router = APIRouter(prefix="/api/devices", tags=["devices"])


@router.get("", response_model=List[Dict[str, Any]])
def list_devices(db: Session = Depends(get_db)):
    """
    List all known devices with their total shot counts, last reported status,
    and last seen timestamp.
    """
    device_ids_from_shots = [d[0] for d in db.query(Shot.device_id).distinct().all()]
    device_ids_from_status = [d[0] for d in db.query(DeviceStatus.device_id).distinct().all()]
    all_device_ids = sorted(list(set(device_ids_from_shots + device_ids_from_status)))

    devices_data = []
    for dev_id in all_device_ids:
        shot_count = db.query(Shot).filter(Shot.device_id == dev_id).count()
        last_shot = (
            db.query(Shot)
            .filter(Shot.device_id == dev_id)
            .order_by(Shot.timestamp.desc())
            .first()
        )
        last_status = (
            db.query(DeviceStatus)
            .filter(DeviceStatus.device_id == dev_id)
            .order_by(DeviceStatus.received_at.desc())
            .first()
        )

        last_seen = None
        if last_status and last_shot:
            last_seen = max(
                last_status.received_at,
                last_shot.received_at or last_shot.timestamp,
            )
        elif last_status:
            last_seen = last_status.received_at
        elif last_shot:
            last_seen = last_shot.received_at or last_shot.timestamp

        devices_data.append(
            {
                "device_id": dev_id,
                "shot_count": shot_count,
                "status": last_status.status if last_status else "active",
                "firmware_version": (
                    last_status.firmware_version
                    if last_status and last_status.firmware_version
                    else (last_shot.firmware_version if last_shot else None)
                ),
                "last_seen": last_seen,
            }
        )

    return devices_data
