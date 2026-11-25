"""MQTT subscriber service connecting to Mosquitto."""

import logging
import re
import threading
from typing import Optional
import paho.mqtt.client as mqtt

from app.config import settings
from app.database import SessionLocal
from app.services import record_shot, record_device_status
from app.validation import (
    MessageValidationError,
    parse_and_validate_shot,
    parse_and_validate_status,
)

logger = logging.getLogger(__name__)

# Topic regex patterns
EVENTS_PATTERN = re.compile(r"^smartfootball/([^/]+)/events$")
STATUS_PATTERN = re.compile(r"^smartfootball/([^/]+)/status$")


class MQTTService:
    """Manages Mosquitto connection and incoming MQTT message routing."""

    def __init__(self):
        self.client: Optional[mqtt.Client] = None
        self.is_connected = False
        self._lock = threading.Lock()

    def _create_client(self) -> mqtt.Client:
        # Use VERSION2 if supported by paho-mqtt
        if hasattr(mqtt, "CallbackAPIVersion"):
            client = mqtt.Client(
                mqtt.CallbackAPIVersion.VERSION2,
                client_id=settings.MQTT_CLIENT_ID,
                clean_session=False,
            )
        else:
            client = mqtt.Client(
                client_id=settings.MQTT_CLIENT_ID,
                clean_session=False,
            )

        if settings.MQTT_BACKEND_USER and settings.MQTT_BACKEND_PASSWORD:
            client.username_pw_set(
                settings.MQTT_BACKEND_USER,
                settings.MQTT_BACKEND_PASSWORD,
            )

        client.on_connect = self._on_connect
        client.on_disconnect = self._on_disconnect
        client.on_message = self._on_message
        return client

    def _on_connect(self, client, userdata, flags, reason_code, properties=None):
        rc = getattr(reason_code, "value", reason_code)
        if rc == 0:
            self.is_connected = True
            logger.info("Connected to Mosquitto broker (%s:%s)", settings.MQTT_HOST, settings.MQTT_PORT)
            # Subscribe to QoS 1 for events as per brief
            client.subscribe(settings.MQTT_TOPIC_EVENTS, qos=1)
            client.subscribe(settings.MQTT_TOPIC_STATUS, qos=1)
            logger.info("Subscribed to %s and %s", settings.MQTT_TOPIC_EVENTS, settings.MQTT_TOPIC_STATUS)
        else:
            self.is_connected = False
            logger.error("Failed to connect to Mosquitto broker: return code %s", rc)

    def _on_disconnect(self, client, userdata, flags_or_rc, reason_code=None, properties=None):
        self.is_connected = False
        logger.warning("Disconnected from Mosquitto broker.")

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload
        logger.debug("Received message on topic %s (QoS %d, len %d)", topic, msg.qos, len(payload))

        events_match = EVENTS_PATTERN.match(topic)
        if events_match:
            device_id = events_match.group(1)
            self._handle_event_message(device_id, payload)
            return

        status_match = STATUS_PATTERN.match(topic)
        if status_match:
            device_id = status_match.group(1)
            self._handle_status_message(device_id, payload)
            return

        logger.warning("Received message on unhandled topic: %s", topic)

    def _handle_event_message(self, topic_device_id: str, payload: bytes):
        """Validate and persist shot event with deduplication."""
        try:
            shot_data = parse_and_validate_shot(payload)
        except MessageValidationError as exc:
            # SEC2 / Validation failure logged clearly
            logger.warning(
                "Invalid shot message on topic for %s: %s",
                topic_device_id,
                exc.message,
            )
            return

        # Check device ID consistency
        if shot_data.device_id != topic_device_id:
            logger.warning(
                "Mismatched device_id in payload ('%s') vs topic ('%s')",
                shot_data.device_id,
                topic_device_id,
            )

        db = SessionLocal()
        try:
            shot, is_created = record_shot(db, shot_data)
            if is_created:
                logger.info("Shot %s recorded successfully from %s", shot.event_id, shot.device_id)
            else:
                logger.info("Duplicate shot %s skipped (SUR1)", shot.event_id)
        except Exception as exc:
            logger.exception("Database error while recording shot: %s", exc)
        finally:
            db.close()

    def _handle_status_message(self, topic_device_id: str, payload: bytes):
        """Record status / heartbeat / Last Will from device."""
        try:
            status_data = parse_and_validate_status(payload, topic_device_id)
            db = SessionLocal()
            try:
                record_device_status(db, status_data)
                logger.info("Status recorded for device %s: %s", topic_device_id, status_data.status)
            finally:
                db.close()
        except Exception as exc:
            logger.exception("Failed to process status message for %s: %s", topic_device_id, exc)

    def start(self):
        """Start the background MQTT client network loop."""
        with self._lock:
            if self.client is None:
                self.client = self._create_client()
            try:
                logger.info("Connecting to Mosquitto at %s:%d ...", settings.MQTT_HOST, settings.MQTT_PORT)
                self.client.connect_async(settings.MQTT_HOST, settings.MQTT_PORT, keepalive=60)
                self.client.loop_start()
            except Exception as exc:
                logger.warning("Could not immediately connect to MQTT broker (%s:%s): %s",
                               settings.MQTT_HOST, settings.MQTT_PORT, exc)

    def stop(self):
        """Stop MQTT client loop gracefully."""
        with self._lock:
            if self.client is not None:
                try:
                    self.client.loop_stop()
                    self.client.disconnect()
                except Exception as exc:
                    logger.debug("Error stopping MQTT client: %s", exc)
                finally:
                    self.client = None
                    self.is_connected = False


mqtt_service = MQTTService()
