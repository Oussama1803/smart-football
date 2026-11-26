"""Endpoints for inspecting connected devices and their operational status."""

from typing import Any, Dict, List
from fastapi import APIRouter, Depends
from sqlalchemy import func
from sqlalchemy.orm import Session

from app.database import get_db
from app.models import DeviceStatus, Shot, KeeperEvent

router = APIRouter(prefix="/api/devices", tags=["devices"])


@router.get("", response_model=List[Dict[str, Any]])
def list_devices(db: Session = Depends(get_db)):
    """
    List all known devices with their total shot counts, last reported status,
    and last seen timestamp.
    """
    device_ids_from_shots = [d[0] for d in db.query(Shot.device_id).distinct().all()]
    device_ids_from_status = [d[0] for d in db.query(DeviceStatus.device_id).distinct().all()]
    device_ids_from_keeper = [d[0] for d in db.query(KeeperEvent.device_id).distinct().all()]
    all_device_ids = sorted(list(set(device_ids_from_shots + device_ids_from_status + device_ids_from_keeper)))

    devices_data = []
    for dev_id in all_device_ids:
        shot_count = db.query(Shot).filter(Shot.device_id == dev_id).count()
        keeper_count = db.query(KeeperEvent).filter(KeeperEvent.device_id == dev_id).count()
        last_shot = (
            db.query(Shot)
            .filter(Shot.device_id == dev_id)
            .order_by(Shot.timestamp.desc())
            .first()
        )
        last_keeper = (
            db.query(KeeperEvent)
            .filter(KeeperEvent.device_id == dev_id)
            .order_by(KeeperEvent.timestamp.desc())
            .first()
        )
        last_status = (
            db.query(DeviceStatus)
            .filter(DeviceStatus.device_id == dev_id)
            .order_by(DeviceStatus.received_at.desc())
            .first()
        )

        seen_candidates = []
        if last_status:
            seen_candidates.append(last_status.received_at)
        if last_shot:
            seen_candidates.append(last_shot.received_at or last_shot.timestamp)
        if last_keeper:
            seen_candidates.append(last_keeper.received_at or last_keeper.timestamp)

        last_seen = max(seen_candidates) if seen_candidates else None

        fw_version = None
        if last_status and last_status.firmware_version:
            fw_version = last_status.firmware_version
        elif last_shot and last_shot.firmware_version:
            fw_version = last_shot.firmware_version
        elif last_keeper and last_keeper.firmware_version:
            fw_version = last_keeper.firmware_version

        devices_data.append(
            {
                "device_id": dev_id,
                "shot_count": shot_count,
                "keeper_event_count": keeper_count,
                "status": last_status.status if last_status else "active",
                "firmware_version": fw_version,
                "last_seen": last_seen,
            }
        )

    return devices_data
