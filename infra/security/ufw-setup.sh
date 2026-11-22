#!/usr/bin/env bash
# Step 8b: firewall of the Smart Football VPS (Ubuntu + UFW).
# Run once as root on the VPS, from a session that is NOT locked out by this script:
# SSH is allowed BEFORE the firewall is enabled.
#
# Only these ports are open to the Internet:
#   SSH (22), HTTP (80, redirect to HTTPS), HTTPS (443), MQTT over TLS (8883).
# Everything else stays closed: plain MQTT (1883), PostgreSQL (5432), the API (8000),
# Prometheus (9090) and Grafana (3000) are reached through the Docker network or Nginx.
#
# WARNING: Docker publishes container ports by editing iptables directly, which BYPASSES UFW.
# In compose.yaml, do not publish db, api, prometheus or grafana ports on 0.0.0.0. Use
# "127.0.0.1:PORT:PORT" or no `ports:` at all. Only Nginx (80, 443) and Mosquitto (8883)
# should publish a port. Check with: ss -tlnp  and an outside port scan (see security-tests.md).
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
  echo "Run as root (sudo)." >&2
  exit 1
fi

SSH_PORT="${SSH_PORT:-22}"

ufw default deny incoming
ufw default allow outgoing

ufw allow "${SSH_PORT}/tcp" comment 'SSH'
ufw allow 80/tcp comment 'HTTP (redirect to HTTPS)'
ufw allow 443/tcp comment 'HTTPS dashboard and API'
ufw allow 8883/tcp comment 'MQTT over TLS'

ufw --force enable
ufw status verbose
