"""Endpoints for aggregated training analytics and session comparisons."""

from typing import List, Optional
from fastapi import APIRouter, Depends, Query
from sqlalchemy import func
from sqlalchemy.orm import Session

from app.database import get_db
from app.models import Shot
from app.schemas import AccuracyStats, SessionSummary

router = APIRouter(prefix="/api", tags=["statistics"])


@router.get("/stats", response_model=AccuracyStats)
def get_stats(
    session_id: Optional[str] = Query(None, description="Optional session filter"),
    db: Session = Depends(get_db),
):
    """
    Calculate global or session-specific performance indicators:
    Total shots, average and max speeds, accuracy rate (shots on target vs none),
    average raw relative pressure and impact intensity, and zone distribution.
    """
    query = db.query(Shot)
    if session_id:
        query = query.filter(Shot.session_id == session_id)

    total_shots = query.count()
    if total_shots == 0:
        return AccuracyStats(
            total_shots=0,
            shots_on_target=0,
            missed_shots=0,
            accuracy_rate_pct=0.0,
            avg_speed_kmh=0.0,
            max_speed_kmh=0.0,
            min_speed_kmh=0.0,
            avg_pressure_reading=0.0,
            avg_impact_intensity=0.0,
            zone_distribution={
                "upper_left": 0,
                "upper_right": 0,
                "lower_left": 0,
                "lower_right": 0,
                "none": 0,
            },
        )

    # Shots on target vs missed (none)
    missed_shots = query.filter(Shot.target_zone == "none").count()
    shots_on_target = total_shots - missed_shots
    accuracy_rate = round((shots_on_target / total_shots) * 100.0, 1)

    # Aggregates
    stats = (
        query.with_entities(
            func.avg(Shot.ball_speed_kmh).label("avg_speed"),
            func.max(Shot.ball_speed_kmh).label("max_speed"),
            func.min(Shot.ball_speed_kmh).label("min_speed"),
            func.avg(Shot.pressure_reading).label("avg_pressure"),
            func.avg(Shot.impact_intensity).label("avg_impact"),
        )
        .first()
    )

    # Zone breakdown
    zone_rows = (
        query.with_entities(Shot.target_zone, func.count(Shot.id))
        .group_by(Shot.target_zone)
        .all()
    )
    zone_distribution = {
        "upper_left": 0,
        "upper_right": 0,
        "lower_left": 0,
        "lower_right": 0,
        "none": 0,
    }
    for zone, count in zone_rows:
        zone_distribution[zone] = count

    return AccuracyStats(
        total_shots=total_shots,
        shots_on_target=shots_on_target,
        missed_shots=missed_shots,
        accuracy_rate_pct=accuracy_rate,
        avg_speed_kmh=round(float(stats.avg_speed or 0.0), 2),
        max_speed_kmh=round(float(stats.max_speed or 0.0), 2),
        min_speed_kmh=round(float(stats.min_speed or 0.0), 2),
        avg_pressure_reading=round(float(stats.avg_pressure or 0.0), 1),
        avg_impact_intensity=round(float(stats.avg_impact or 0.0), 1),
        zone_distribution=zone_distribution,
    )


@router.get("/sessions", response_model=List[SessionSummary])
def list_sessions(db: Session = Depends(get_db)):
    """
    Retrieve summarized analytics for all sessions to enable historical comparison.
    """
    sessions_raw = (
        db.query(Shot.session_id)
        .distinct()
        .all()
    )
    session_ids = [s[0] for s in sessions_raw if s[0]]

    summaries = []
    for sid in sorted(session_ids):
        query = db.query(Shot).filter(Shot.session_id == sid)
        total = query.count()
        if total == 0:
            continue
        missed = query.filter(Shot.target_zone == "none").count()
        on_target = total - missed
        acc = round((on_target / total) * 100.0, 1)

        row = (
            query.with_entities(
                func.avg(Shot.ball_speed_kmh).label("avg_spd"),
                func.max(Shot.ball_speed_kmh).label("max_spd"),
                func.avg(Shot.pressure_reading).label("avg_pres"),
                func.avg(Shot.impact_intensity).label("avg_imp"),
                func.min(Shot.timestamp).label("first_shot"),
                func.max(Shot.timestamp).label("last_shot"),
            )
            .first()
        )

        summaries.append(
            SessionSummary(
                session_id=sid,
                total_shots=total,
                shots_on_target=on_target,
                accuracy_rate_pct=acc,
                avg_speed_kmh=round(float(row.avg_spd or 0.0), 2),
                max_speed_kmh=round(float(row.max_spd or 0.0), 2),
                avg_pressure_reading=round(float(row.avg_pres or 0.0), 1),
                avg_impact_intensity=round(float(row.avg_imp or 0.0), 1),
                first_shot_at=row.first_shot,
                last_shot_at=row.last_shot,
            )
        )

    return summaries
