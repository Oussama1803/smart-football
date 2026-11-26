# Acquisition du gardien (étape 11)

Ce projet lit le MPU6050 à l'adresse I²C `0x68` sur SDA GPIO21 / SCL GPIO22.
L'identifiant est vérifié, le capteur est réveillé et configuré en plages ±2 g
et ±250 °/s. Les six axes sont lus en rafale à 10 Hz ; les lectures I²C expirent
après 10 ms et une panne provoque une nouvelle initialisation. Un watchdog de
3 secondes surveille la boucle. Les conversions brutes sont couvertes par les
tests natifs.

## Contrat MQTT & Intégration Backend

Le contrat avec le backend (FastAPI + PostgreSQL + Mosquitto) est établi :
- **Topic d'événements** : `smartfootball/keeper-esp32-01/events` (QoS 1)
- **Topic d'état / Heartbeat** : `smartfootball/keeper-esp32-01/status` (QoS 1)
- **Format JSON du message** :
```json
{
  "event_id": "keeper-evt-001",
  "device_id": "keeper-esp32-01",
  "session_id": "session-001",
  "event_type": "keeper_motion",
  "timestamp": "2026-11-26T10:00:00Z",
  "accel_x": 0.12,
  "accel_y": -0.98,
  "accel_z": 0.05,
  "gyro_x": 12.5,
  "gyro_y": -4.2,
  "gyro_z": 45.0,
  "dive_direction": "right",
  "firmware_version": "0.1.0"
}
```

Les mouvements et plongeons sont classifiés automatiquement (`none`, `left`, `right`, `jump`, `dive`) et enregistrés en base dans la table `keeper_events` avec déduplication stricte (SUR1).

## Compilation et tests

Compiler depuis ce dossier avec `pio run -e esp32dev`, puis tester avec
`pio test -e native`. Pour Wokwi, démarrer le projet et lancer
`wokwi-cli . --scenario scenarios/motion.test.yaml`.
