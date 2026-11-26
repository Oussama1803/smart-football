"""FastAPI application entrypoint for Smart Football backend."""

import logging
from contextlib import asynccontextmanager
from pathlib import Path
from fastapi import FastAPI, Depends
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse, Response
from sqlalchemy import text
from sqlalchemy.orm import Session

from app.config import settings
from app.database import get_db, init_db
from app.mqtt_service import mqtt_service
from app.api import shots, stats, devices
from app.metrics import get_metrics_data

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger("smartfootball")


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Lifecycle manager handling startup initialization and shutdown cleanup."""
    logger.info("Initializing database schema...")
    try:
        init_db()
        logger.info("Database schema initialized successfully.")
    except Exception as exc:
        logger.error("Failed to initialize database schema: %s", exc)

    if settings.ENABLE_MQTT_SUBSCRIBER:
        logger.info("Starting MQTT subscriber service...")
        mqtt_service.start()

    yield

    if settings.ENABLE_MQTT_SUBSCRIBER:
        logger.info("Stopping MQTT subscriber service...")
        mqtt_service.stop()


app = FastAPI(
    title="Smart Football Telemetry API",
    description="Backend service for Smart Football: validates, ingests, and provides analytics for ball shots.",
    version="0.1.0",
    lifespan=lifespan,
)

# Enable CORS for frontend integration
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include API routers
app.include_router(shots.router)
app.include_router(stats.router)
app.include_router(devices.router)


@app.get("/health", tags=["system"])
def health_check(db: Session = Depends(get_db)):
    """Health check endpoint checking DB connection and MQTT service status."""
    db_ok = False
    try:
        db.execute(text("SELECT 1"))
        db_ok = True
    except Exception as exc:
        logger.error("DB health check error: %s", exc)

    return {
        "status": "healthy" if db_ok else "degraded",
        "database": "connected" if db_ok else "unreachable",
        "mqtt_service": "running" if (settings.ENABLE_MQTT_SUBSCRIBER and mqtt_service.client is not None) else "disabled",
        "mqtt_connected": mqtt_service.is_connected,
        "version": "0.1.0",
    }


@app.get("/metrics", tags=["system"], include_in_schema=False)
def get_metrics():
    """Prometheus metrics endpoint for scrapers (internal only)."""
    content, media_type = get_metrics_data()
    return Response(content=content, media_type=media_type)


# Mount frontend static files if directory exists
frontend_dir = Path(__file__).resolve().parent.parent.parent / "frontend"
if frontend_dir.exists() and (frontend_dir / "index.html").exists():
    app.mount("/static", StaticFiles(directory=str(frontend_dir)), name="static")
    if (frontend_dir / "css").exists():
        app.mount("/css", StaticFiles(directory=str(frontend_dir / "css")), name="css")
    if (frontend_dir / "js").exists():
        app.mount("/js", StaticFiles(directory=str(frontend_dir / "js")), name="js")

    @app.get("/", include_in_schema=False)
    def serve_frontend_root():
        return FileResponse(str(frontend_dir / "index.html"))
