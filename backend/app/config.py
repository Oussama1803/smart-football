"""Application configuration loaded from environment variables."""

import os
from typing import Optional


class Settings:
    # Database
    DATABASE_URL: str = os.getenv(
        "DATABASE_URL",
        "postgresql://smartfootball:change-me@localhost:5432/smartfootball",
    )

    # Mosquitto MQTT
    MQTT_HOST: str = os.getenv("MQTT_HOST", "localhost")
    MQTT_PORT: int = int(os.getenv("MQTT_PORT", "1883"))
    MQTT_BACKEND_USER: Optional[str] = os.getenv("MQTT_BACKEND_USER", None)
    MQTT_BACKEND_PASSWORD: Optional[str] = os.getenv("MQTT_BACKEND_PASSWORD", None)
    MQTT_CLIENT_ID: str = os.getenv("MQTT_CLIENT_ID", "smartfootball-backend")
    MQTT_TOPIC_EVENTS: str = os.getenv("MQTT_TOPIC_EVENTS", "smartfootball/+/events")
    MQTT_TOPIC_STATUS: str = os.getenv("MQTT_TOPIC_STATUS", "smartfootball/+/status")
    ENABLE_MQTT_SUBSCRIBER: bool = os.getenv("ENABLE_MQTT_SUBSCRIBER", "true").lower() in (
        "true",
        "1",
        "yes",
    )

    # API Security
    API_SECRET_KEY: str = os.getenv("API_SECRET_KEY", "change-me")

    # Business Rules & Constraints
    MIN_SPEED_KMH: float = 5.0
    MAX_SPEED_KMH: float = 150.0

    # 4 zones for prototype, designed to easily extend to 9 zones
    ALLOWED_TARGET_ZONES: tuple = (
        "upper_left",
        "upper_right",
        "lower_left",
        "lower_right",
        "none",
        # 9-zone extensions supported
        "upper_center",
        "middle_left",
        "center",
        "middle_right",
        "lower_center",
    )


settings = Settings()
