"""Endpoints for querying and creating shot events."""

from typing import Optional
from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy.orm import Session

from app.database import get_db
from app.models import Shot
from app.schemas import IngestionResult, ShotCreate, ShotListResponse, ShotResponse
from app.services import record_shot

router = APIRouter(prefix="/api/shots", tags=["shots"])


@router.get("", response_model=ShotListResponse)
def list_shots(
    session_id: Optional[str] = Query(None, description="Filter by training session ID"),
    device_id: Optional[str] = Query(None, description="Filter by device ID"),
    target_zone: Optional[str] = Query(None, description="Filter by target zone"),
    limit: int = Query(50, ge=1, le=500, description="Number of results to return"),
    offset: int = Query(0, ge=0, description="Offset for pagination"),
    db: Session = Depends(get_db),
):
    """Retrieve paginated list of shots, ordered from newest to oldest."""
    query = db.query(Shot)
    if session_id:
        query = query.filter(Shot.session_id == session_id)
    if device_id:
        query = query.filter(Shot.device_id == device_id)
    if target_zone:
        query = query.filter(Shot.target_zone == target_zone)

    total = query.count()
    shots = query.order_by(Shot.timestamp.desc()).offset(offset).limit(limit).all()

    # Calculate latency in ms between shot timestamp and received_at
    shot_responses = []
    for s in shots:
        item = ShotResponse.model_validate(s)
        if s.timestamp and s.received_at:
            delta = (s.received_at - s.timestamp).total_seconds() * 1000.0
            item.latency_ms = round(delta, 1)
        shot_responses.append(item)

    return ShotListResponse(total=total, limit=limit, offset=offset, shots=shot_responses)


@router.get("/{event_id}", response_model=ShotResponse)
def get_shot(event_id: str, db: Session = Depends(get_db)):
    """Retrieve details of a specific shot by event_id."""
    shot = db.query(Shot).filter(Shot.event_id == event_id).first()
    if not shot:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Shot not found")
    res = ShotResponse.model_validate(shot)
    if shot.timestamp and shot.received_at:
        res.latency_ms = round((shot.received_at - shot.timestamp).total_seconds() * 1000.0, 1)
    return res


@router.post("", response_model=IngestionResult, status_code=status.HTTP_201_CREATED)
def ingest_shot(shot_in: ShotCreate, db: Session = Depends(get_db)):
    """
    Ingest a shot via HTTP (equivalent to MQTT reception).
    Implements anti-doublon deduplication: duplicate event_id is safely ignored.
    """
    shot, is_created = record_shot(db, shot_in)
    if is_created:
        return IngestionResult(
            status="created",
            event_id=shot.event_id,
            message="Shot recorded successfully",
        )
    return IngestionResult(
        status="duplicate_ignored",
        event_id=shot_in.event_id,
        message="SUR1: Duplicate event_id detected and ignored",
    )
