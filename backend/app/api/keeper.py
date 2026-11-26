"""Endpoints for goalkeeper MPU6050 motion and dive telemetry (Étape 11)."""

from typing import Any, Dict, List, Optional
from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy.orm import Session

from app.database import get_db
from app.models import KeeperEvent
from app.schemas import (
    KeeperEventCreate,
    KeeperEventResponse,
    KeeperIngestionResult,
)
from app.services import record_keeper_event

router = APIRouter(prefix="/api/keeper", tags=["keeper"])


@router.get("/events", response_model=List[KeeperEventResponse])
def get_keeper_events(
    session_id: Optional[str] = Query(None, description="Filter by training session"),
    device_id: Optional[str] = Query(None, description="Filter by device ID"),
    limit: int = Query(50, ge=1, le=500, description="Max events to return"),
    db: Session = Depends(get_db),
):
    """Retrieve historical goalkeeper motion events sorted chronologically desc."""
    query = db.query(KeeperEvent)
    if session_id:
        query = query.filter(KeeperEvent.session_id == session_id)
    if device_id:
        query = query.filter(KeeperEvent.device_id == device_id)
    return query.order_by(KeeperEvent.timestamp.desc()).limit(limit).all()


@router.get("/latest", response_model=Optional[KeeperEventResponse])
def get_latest_keeper_event(
    device_id: Optional[str] = Query(None, description="Filter by device ID"),
    db: Session = Depends(get_db),
):
    """Retrieve the most recent goalkeeper motion reading."""
    query = db.query(KeeperEvent)
    if device_id:
        query = query.filter(KeeperEvent.device_id == device_id)
    event = query.order_by(KeeperEvent.timestamp.desc()).first()
    if not event:
        return None
    return event


@router.post(
    "/events",
    response_model=KeeperIngestionResult,
    status_code=status.HTTP_201_CREATED,
)
def ingest_keeper_event(
    event_in: KeeperEventCreate,
    db: Session = Depends(get_db),
):
    """Ingest a goalkeeper event via HTTP with SUR1 deduplication."""
    event, is_created = record_keeper_event(db, event_in)
    if not is_created:
        return KeeperIngestionResult(
            status="duplicate_ignored",
            event_id=event_in.event_id,
            message="Duplicate event_id ignored per SUR1 requirement.",
        )
    return KeeperIngestionResult(
        status="created",
        event_id=event.event_id,
        message="Keeper event recorded successfully.",
    )
