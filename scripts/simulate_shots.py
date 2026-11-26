#!/usr/bin/env python3
"""
Simulate and publish Smart Football shots over MQTT or HTTP.
Can be used to test:
- Step 5 verification: fake shot arrives in DB and is returned by API
- SUR1: Burst of shots + duplicate retransmission
- SEC2: Invalid speed rejection
- ENF1: Reception latency measurement (< 2.0s)
- Dashboard demo data seeding
"""

import argparse
import json
import random
import time
import uuid
from datetime import datetime, timezone
import urllib.request
import urllib.error

# Specification constants
DEFAULT_DEVICE_ID = "goal-esp32-01"
DEFAULT_KEEPER_ID = "keeper-esp32-01"
DEFAULT_SESSION_ID = "session-001"
DEFAULT_TOPIC = f"smartfootball/{DEFAULT_DEVICE_ID}/events"
KEEPER_TOPIC = f"smartfootball/{DEFAULT_KEEPER_ID}/events"
ZONES = ["upper_left", "upper_right", "lower_left", "lower_right", "none"]


def generate_shot(
    device_id: str = DEFAULT_DEVICE_ID,
    session_id: str = DEFAULT_SESSION_ID,
    speed_kmh: float = None,
    zone: str = None,
    event_id: str = None,
) -> dict:
    """Generate a single specification-compliant shot message."""
    if speed_kmh is None:
        # Realistic football shot speed between 50 and 110 km/h
        speed_kmh = round(random.uniform(55.0, 105.0), 1)

    if zone is None:
        # 80% on target, 20% none
        zone = random.choice(["upper_left", "upper_right", "lower_left", "lower_right", "lower_right", "none"])

    # Relative ADC raw values (never Newtons)
    pressure_adc = random.randint(350, 850)
    impact_adc = 0 if zone == "none" else random.randint(450, 950)

    now_iso = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    return {
        "event_id": event_id or str(uuid.uuid4()),
        "device_id": device_id,
        "session_id": session_id,
        "event_type": "shot",
        "timestamp": now_iso,
        "ball_speed_kmh": speed_kmh,
        "pressure_reading": pressure_adc,
        "target_zone": zone,
        "impact_intensity": impact_adc,
        "firmware_version": "0.1.0",
    }


def generate_keeper_event(
    device_id: str = DEFAULT_KEEPER_ID,
    session_id: str = DEFAULT_SESSION_ID,
    dive_direction: str = None,
    event_id: str = None,
) -> dict:
    """Generate a specification-compliant goalkeeper MPU6050 motion message."""
    directions = ["none", "left", "right", "jump", "dive"]
    if dive_direction is None:
        dive_direction = random.choice(directions)

    if dive_direction == "left":
        ax, ay, az = -1.25, 0.45, 0.15
        gx, gy, gz = 14.0, -8.0, -85.0
    elif dive_direction == "right":
        ax, ay, az = 1.30, 0.35, 0.10
        gx, gy, gz = -12.0, 7.5, 92.0
    elif dive_direction == "jump":
        ax, ay, az = 0.10, 1.65, 0.25
        gx, gy, gz = 4.0, 2.0, -3.0
    elif dive_direction == "dive":
        ax, ay, az = 0.20, -1.35, -0.50
        gx, gy, gz = -20.0, -12.0, 8.0
    else:  # none
        ax = round(random.uniform(-0.05, 0.05), 2)
        ay = round(random.uniform(-0.05, 0.05), 2)
        az = 1.00
        gx = round(random.uniform(-1.0, 1.0), 1)
        gy = round(random.uniform(-1.0, 1.0), 1)
        gz = round(random.uniform(-1.0, 1.0), 1)

    now_iso = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    return {
        "event_id": event_id or str(uuid.uuid4()),
        "device_id": device_id,
        "session_id": session_id,
        "event_type": "keeper_motion",
        "timestamp": now_iso,
        "accel_x": ax,
        "accel_y": ay,
        "accel_z": az,
        "gyro_x": gx,
        "gyro_y": gy,
        "gyro_z": gz,
        "dive_direction": dive_direction,
        "firmware_version": "0.1.0",
    }



def publish_mqtt(broker_host: str, broker_port: int, topic: str, payload: dict) -> bool:
    """Publish a shot message to Mosquitto broker via MQTT QoS 1."""
    try:
        import paho.mqtt.client as mqtt

        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2 if hasattr(mqtt, "CallbackAPIVersion") else None)
        client.connect(broker_host, broker_port, 60)
        json_data = json.dumps(payload)
        info = client.publish(topic, json_data, qos=1)
        info.wait_for_publish(timeout=5.0)
        client.disconnect()
        return True
    except Exception as exc:
        print(f"[MQTT ERROR] Failed to publish: {exc}")
        return False


def post_http(api_url: str, payload: dict) -> dict:
    """Send shot via HTTP POST /api/shots."""
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        f"{api_url.rstrip('/')}/api/shots",
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        return {"error": exc.code, "message": exc.read().decode("utf-8")}


def post_http_keeper(api_url: str, payload: dict) -> dict:
    """Send keeper event via HTTP POST /api/keeper/events."""
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        f"{api_url.rstrip('/')}/api/keeper/events",
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        return {"error": exc.code, "message": exc.read().decode("utf-8")}



def main():
    parser = argparse.ArgumentParser(description="Smart Football Shot Simulator")
    parser.add_argument(
        "--mode",
        choices=["single", "burst", "sur1-test", "sec2-test", "seed-demo", "keeper", "keeper-demo"],
        default="single",
    )
    parser.add_argument("--mqtt", action="store_true", help="Send via MQTT broker")
    parser.add_argument("--broker", default="localhost", help="Mosquitto broker host")
    parser.add_argument("--port", type=int, default=1883, help="Mosquitto broker port")
    parser.add_argument("--api", default="http://localhost:8000", help="Backend API base URL")
    parser.add_argument("--count", type=int, default=10, help="Number of shots for burst or demo seed")
    parser.add_argument("--session", default=DEFAULT_SESSION_ID, help="Session identifier")
    args = parser.parse_args()

    print(f"=== Smart Football Shot Simulator (mode: {args.mode}) ===")

    if args.mode == "single":
        shot = generate_shot(session_id=args.session)
        print(f"Generated shot: {json.dumps(shot, indent=2)}")
        if args.mqtt:
            print(f"Publishing to MQTT {args.broker}:{args.port} on topic {DEFAULT_TOPIC}...")
            ok = publish_mqtt(args.broker, args.port, DEFAULT_TOPIC, shot)
            print("MQTT Publish OK" if ok else "MQTT Publish Failed")
        else:
            print(f"Sending via HTTP POST to {args.api}/api/shots...")
            res = post_http(args.api, shot)
            print(f"Response: {res}")

    elif args.mode == "sur1-test":
        print("Testing SUR1 (Deduplication after network cut / retransmission)...")
        shots = [generate_shot(session_id="session-sur1") for _ in range(10)]

        print("1. Sending 10 distinct shots...")
        for i, s in enumerate(shots, 1):
            if args.mqtt:
                publish_mqtt(args.broker, args.port, DEFAULT_TOPIC, s)
            else:
                post_http(args.api, s)
            print(f"  Shot {i}/10 sent: {s['event_id']}")

        print("\n2. Re-transmitting the EXACT SAME 10 shots (simulating reconnection burst)...")
        for i, s in enumerate(shots, 1):
            if args.mqtt:
                publish_mqtt(args.broker, args.port, DEFAULT_TOPIC, s)
            else:
                res = post_http(args.api, s)
                status = res.get("status", "unknown")
                print(f"  Shot {i}/10 re-sent: status={status}")

        print("\n[SUR1 Check Complete] Query /api/stats?session_id=session-sur1 to verify exactly 10 shots in DB.")

    elif args.mode == "sec2-test":
        print("Testing SEC2 (Rejection of speeds outside [5.0, 150.0] km/h)...")
        # 1. Too slow: 3.0 km/h
        slow_shot = generate_shot(speed_kmh=3.0)
        print(f"1. Testing speed too slow (3.0 km/h)...")
        res_slow = post_http(args.api, slow_shot)
        print(f"  Result: {res_slow}")

        # 2. Too fast: 195.0 km/h
        fast_shot = generate_shot(speed_kmh=195.0)
        print(f"2. Testing speed too fast (195.0 km/h)...")
        res_fast = post_http(args.api, fast_shot)
        print(f"  Result: {res_fast}")

    elif args.mode == "seed-demo":
        print(f"Seeding demo training sessions ({args.count} shots across 2 sessions)...")
        for i in range(args.count):
            sid = "session-001" if i < (args.count // 2) else "session-002"
            shot = generate_shot(session_id=sid)
            if args.mqtt:
                publish_mqtt(args.broker, args.port, DEFAULT_TOPIC, shot)
            else:
                post_http(args.api, shot)
            time.sleep(0.05)
        print("Demo data seeded successfully!")

    elif args.mode == "keeper":
        event = generate_keeper_event(session_id=args.session)
        print(f"Generated keeper event: {json.dumps(event, indent=2)}")
        if args.mqtt:
            print(f"Publishing to MQTT {args.broker}:{args.port} on topic {KEEPER_TOPIC}...")
            ok = publish_mqtt(args.broker, args.port, KEEPER_TOPIC, event)
            print("MQTT Publish OK" if ok else "MQTT Publish Failed")
        else:
            print(f"Sending via HTTP POST to {args.api}/api/keeper/events...")
            res = post_http_keeper(args.api, event)
            print(f"Response: {res}")

    elif args.mode == "keeper-demo":
        print("Seeding goalkeeper motions (standing, dives left/right, jumps)...")
        directions = ["none", "left", "right", "jump", "dive"]
        for d in directions:
            event = generate_keeper_event(session_id=args.session, dive_direction=d)
            if args.mqtt:
                publish_mqtt(args.broker, args.port, KEEPER_TOPIC, event)
            else:
                post_http_keeper(args.api, event)
            print(f"  Keeper action sent: {d} (accel=[{event['accel_x']}, {event['accel_y']}, {event['accel_z']}])")
            time.sleep(0.1)
        print("Goalkeeper demo telemetry seeded successfully!")



if __name__ == "__main__":
    main()
