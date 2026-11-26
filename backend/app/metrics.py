"""Prometheus telemetry metrics for backend monitoring."""

import threading
from typing import Dict, Tuple

try:
    from prometheus_client import Counter, generate_latest, CONTENT_TYPE_LATEST
    HAVE_PROMETHEUS_CLIENT = True
except ImportError:
    HAVE_PROMETHEUS_CLIENT = False

if HAVE_PROMETHEUS_CLIENT:
    MQTT_MESSAGES_TOTAL = Counter(
        "smartfootball_mqtt_messages_total",
        "Total MQTT messages received by type",
        ["type"],
    )
    PROCESSING_ERRORS_TOTAL = Counter(
        "smartfootball_processing_errors_total",
        "Total message processing and ingestion errors",
        ["reason"],
    )
    DEVICE_OFFLINE_TOTAL = Counter(
        "smartfootball_device_offline_total",
        "Total device offline events received via LWT",
    )

    def record_mqtt_message(msg_type: str):
        MQTT_MESSAGES_TOTAL.labels(type=msg_type).inc()

    def record_processing_error(reason: str):
        PROCESSING_ERRORS_TOTAL.labels(reason=reason).inc()

    def record_device_offline():
        DEVICE_OFFLINE_TOTAL.inc()

    def get_metrics_data() -> Tuple[bytes, str]:
        return generate_latest(), CONTENT_TYPE_LATEST

else:
    # Pure Python fallback producing valid Prometheus exposition format
    _lock = threading.Lock()
    _msg_counts: Dict[str, int] = {"shot": 0, "status": 0}
    _err_counts: Dict[str, int] = {"validation": 0, "database": 0}
    _offline_count = 0

    def record_mqtt_message(msg_type: str):
        with _lock:
            _msg_counts[msg_type] = _msg_counts.get(msg_type, 0) + 1

    def record_processing_error(reason: str):
        with _lock:
            _err_counts[reason] = _err_counts.get(reason, 0) + 1

    def record_device_offline():
        global _offline_count
        with _lock:
            _offline_count += 1

    def get_metrics_data() -> Tuple[bytes, str]:
        with _lock:
            lines = [
                "# HELP smartfootball_mqtt_messages_total Total MQTT messages received by type",
                "# TYPE smartfootball_mqtt_messages_total counter",
                f'smartfootball_mqtt_messages_total{{type="shot"}} {_msg_counts.get("shot", 0)}',
                f'smartfootball_mqtt_messages_total{{type="status"}} {_msg_counts.get("status", 0)}',
                "# HELP smartfootball_processing_errors_total Total message processing and ingestion errors",
                "# TYPE smartfootball_processing_errors_total counter",
                f'smartfootball_processing_errors_total{{reason="validation"}} {_err_counts.get("validation", 0)}',
                f'smartfootball_processing_errors_total{{reason="database"}} {_err_counts.get("database", 0)}',
                "# HELP smartfootball_device_offline_total Total device offline events received via LWT",
                "# TYPE smartfootball_device_offline_total counter",
                f'smartfootball_device_offline_total {_offline_count}',
                "",
            ]
            content = "\n".join(lines).encode("utf-8")
            return content, "text/plain; version=0.0.4; charset=utf-8"
