"""Tests for REST API endpoints (shots, statistics, sessions, devices)."""

import uuid


def test_health_check(client):
    """Verify /health endpoint returns healthy status and DB connectivity."""
    response = client.get("/health")
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "healthy"
    assert data["database"] == "connected"


def test_ingest_shot_api_success(client, sample_shot_payload):
    """POST /api/shots successfully stores a valid shot."""
    response = client.post("/api/shots", json=sample_shot_payload)
    assert response.status_code == 201
    data = response.json()
    assert data["status"] == "created"
    assert data["event_id"] == sample_shot_payload["event_id"]


def test_ingest_shot_api_deduplication(client, sample_shot_payload):
    """POST /api/shots safely ignores duplicate shot without error."""
    # First post
    r1 = client.post("/api/shots", json=sample_shot_payload)
    assert r1.status_code == 201
    assert r1.json()["status"] == "created"

    # Second post (duplicate)
    r2 = client.post("/api/shots", json=sample_shot_payload)
    assert r2.status_code == 201
    assert r2.json()["status"] == "duplicate_ignored"

    # Verify only 1 shot exists in list
    list_res = client.get("/api/shots")
    assert list_res.status_code == 200
    assert list_res.json()["total"] == 1


def test_ingest_shot_api_invalid_speed_rejected(client, sample_shot_payload):
    """POST /api/shots rejects out-of-range speed (SEC2)."""
    invalid_payload = sample_shot_payload.copy()
    invalid_payload["ball_speed_kmh"] = 190.0  # > 150 km/h
    response = client.post("/api/shots", json=invalid_payload)
    assert response.status_code == 422


def test_list_shots_and_filters(client, sample_shot_payload):
    """Verify listing shots and applying filters for session and target_zone."""
    # Insert 3 shots across 2 sessions and zones
    s1 = sample_shot_payload.copy()
    s1["event_id"] = str(uuid.uuid4())
    s1["session_id"] = "session-001"
    s1["target_zone"] = "upper_left"
    client.post("/api/shots", json=s1)

    s2 = sample_shot_payload.copy()
    s2["event_id"] = str(uuid.uuid4())
    s2["session_id"] = "session-001"
    s2["target_zone"] = "none"  # missed
    client.post("/api/shots", json=s2)

    s3 = sample_shot_payload.copy()
    s3["event_id"] = str(uuid.uuid4())
    s3["session_id"] = "session-002"
    s3["target_zone"] = "lower_right"
    client.post("/api/shots", json=s3)

    # All shots
    res_all = client.get("/api/shots")
    assert res_all.json()["total"] == 3

    # Filter session-001
    res_s1 = client.get("/api/shots?session_id=session-001")
    assert res_s1.json()["total"] == 2

    # Filter target_zone none
    res_none = client.get("/api/shots?target_zone=none")
    assert res_none.json()["total"] == 1


def test_get_shot_by_id(client, sample_shot_payload):
    """Verify retrieving a specific shot by event_id."""
    client.post("/api/shots", json=sample_shot_payload)
    res = client.get(f"/api/shots/{sample_shot_payload['event_id']}")
    assert res.status_code == 200
    assert res.json()["event_id"] == sample_shot_payload["event_id"]
    assert res.json()["ball_speed_kmh"] == 72.0

    # Unknown ID
    res_404 = client.get("/api/shots/unknown-uuid")
    assert res_404.status_code == 404


def test_stats_and_accuracy_calculation(client, sample_shot_payload):
    """
    Verify accuracy rate, speed averages, and zone breakdown.
    4 shots: 3 on target, 1 none -> accuracy = 75.0%
    """
    shots = [
        {"speed": 60.0, "zone": "upper_left", "pressure": 500, "impact": 700},
        {"speed": 80.0, "zone": "upper_right", "pressure": 600, "impact": 800},
        {"speed": 100.0, "zone": "lower_left", "pressure": 700, "impact": 900},
        {"speed": 70.0, "zone": "none", "pressure": 400, "impact": 0},  # Miss
    ]

    for item in shots:
        p = sample_shot_payload.copy()
        p["event_id"] = str(uuid.uuid4())
        p["ball_speed_kmh"] = item["speed"]
        p["target_zone"] = item["zone"]
        p["pressure_reading"] = item["pressure"]
        p["impact_intensity"] = item["impact"]
        client.post("/api/shots", json=p)

    res = client.get("/api/stats")
    assert res.status_code == 200
    data = res.json()

    assert data["total_shots"] == 4
    assert data["shots_on_target"] == 3
    assert data["missed_shots"] == 1
    assert data["accuracy_rate_pct"] == 75.0
    assert data["avg_speed_kmh"] == 77.5
    assert data["max_speed_kmh"] == 100.0
    assert data["min_speed_kmh"] == 60.0
    assert data["zone_distribution"]["upper_left"] == 1
    assert data["zone_distribution"]["none"] == 1
    assert data["data_classification"]["pressure"].startswith("Valeur brute")


def test_session_comparison_endpoint(client, sample_shot_payload):
    """Verify session summary endpoint for comparing sessions."""
    # Session 1: 1 shot at 80 km/h
    p1 = sample_shot_payload.copy()
    p1["event_id"] = str(uuid.uuid4())
    p1["session_id"] = "session-A"
    p1["ball_speed_kmh"] = 80.0
    client.post("/api/shots", json=p1)

    # Session 2: 1 shot at 110 km/h
    p2 = sample_shot_payload.copy()
    p2["event_id"] = str(uuid.uuid4())
    p2["session_id"] = "session-B"
    p2["ball_speed_kmh"] = 110.0
    client.post("/api/shots", json=p2)

    res = client.get("/api/sessions")
    assert res.status_code == 200
    sessions = res.json()
    assert len(sessions) == 2
    assert sessions[0]["session_id"] == "session-A"
    assert sessions[1]["session_id"] == "session-B"


def test_device_list_endpoint(client, sample_shot_payload):
    """Verify listing devices with their shot counts."""
    p = sample_shot_payload.copy()
    p["device_id"] = "goal-esp32-alpha"
    client.post("/api/shots", json=p)

    res = client.get("/api/devices")
    assert res.status_code == 200
    devices = res.json()
    assert len(devices) >= 1
    dev = next(d for d in devices if d["device_id"] == "goal-esp32-alpha")
    assert dev["shot_count"] == 1


def test_metrics_endpoint(client):
    """Verify /metrics endpoint returns Prometheus exposition format."""
    res = client.get("/metrics")
    assert res.status_code == 200
    body = res.text
    assert "smartfootball_mqtt_messages_total" in body
    assert "smartfootball_processing_errors_total" in body
    assert "smartfootball_device_offline_total" in body


def test_frontend_static_serving(client):
    """Verify frontend HTML, CSS, and JS static assets are served properly."""
    # Root index
    res_root = client.get("/")
    assert res_root.status_code == 200
    assert "Smart Football" in res_root.text

    # /static/css/styles.css
    res_css = client.get("/static/css/styles.css")
    assert res_css.status_code == 200
    assert "text/css" in res_css.headers.get("content-type", "")

    # /static/js/app.js
    res_js = client.get("/static/js/app.js")
    assert res_js.status_code == 200
    assert "javascript" in res_js.headers.get("content-type", "")

    # Fallback /css/styles.css and /js/app.js
    res_css_direct = client.get("/css/styles.css")
    assert res_css_direct.status_code == 200

    res_js_direct = client.get("/js/app.js")
    assert res_js_direct.status_code == 200

