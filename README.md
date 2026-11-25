# Smart Football

Projet d'équipe du module « gestion des systèmes embarqués » (ESI, 5e année, cybersécurité). Tout est en simulation sous Wokwi, sans matériel réel.

Le système mesure les tirs à l'entraînement : vitesse du ballon, zone du but touchée, intensité relative de l'impact, pression à la frappe et, en extension, les mouvements du gardien.

## Trajet de la donnée

```text
Capteurs -> ESP32 (acquisition, traitement, horodatage) -> Wi-Fi + MQTT/TLS
-> Mosquitto -> FastAPI -> PostgreSQL -> tableau de bord -> joueur, gardien, entraîneur
```

## Équipe

| Personne | Rôle |
|---|---|
| Oussama | Chef de projet, sécurité / qualité / DevOps avec Mohamed |
| Mohamed | Sécurité, qualité, DevOps |
| Saad | Embarqué : circuit Wokwi, firmware, Wi-Fi, MQTT |
| Alae | Cloud et données : broker, base, API, tableau de bord, déploiement |

Les tâches sont dans les issues du dépôt (un titre `[Prénom] Étape N : ...`).

## Capteurs (cahier des charges, section 5)

| Capteur | Quantité | Ce qu'il mesure |
|---|---|---|
| Barrière IR break-beam | 2 | Vitesse du ballon entre deux points |
| Piézoélectrique | 4 (une par zone) | Zone probable et intensité relative de l'impact |
| FSR | 1 | Pression relative à la frappe |
| MPU6050 (extension) | 1 | Accélérations et rotations du gardien |

Ce que les capteurs ne mesurent pas : le FSR ne donne ni la vitesse ni une puissance de tir, les piézos ne donnent pas une force en Newton, le MPU6050 ne donne ni position ni temps de réaction direct. `pressure_reading` et `impact_intensity` sont des valeurs ADC brutes.

Vitesse : `v = D / (t2 - t1)` puis `v_kmh = v * 3.6`. Avec `D = 0.20 m` et `t2 - t1 = 0.01 s` : 20 m/s, soit 72 km/h (valeur de référence des tests).

## Message MQTT (cahier des charges, section 6)

```json
{
  "event_id": "b3f1c2a0-7d4e-4f6a-9c1e-2a5d8e7f0b11",
  "device_id": "goal-esp32-01",
  "session_id": "session-001",
  "event_type": "shot",
  "timestamp": "2026-11-10T14:30:00Z",
  "ball_speed_kmh": 72.0,
  "pressure_reading": 640,
  "target_zone": "upper_right",
  "impact_intensity": 845,
  "firmware_version": "0.1.0"
}
```

Zones du prototype : `upper_left`, `upper_right`, `lower_left`, `lower_right`. L'extension à 9 zones ne doit pas changer le format.

Topics :
- `smartfootball/<device_id>/events` : les tirs, QoS 1
- `smartfootball/<device_id>/status` : heartbeat (30 s), erreurs, Last Will

## Architecture

Ubuntu VPS, Eclipse Mosquitto, FastAPI, PostgreSQL (volume persistant), tableau de bord Web, Nginx (HTTPS), Docker Compose, GitHub Actions, Prometheus et Grafana, UFW.

## Organisation du dépôt

```text
firmware/         ESP32 du but : PlatformIO (Arduino), diagram.json, wokwi.toml
firmware-keeper/  ESP32 du gardien avec MPU6050 (extension)
chips/            custom chips Wokwi : ir-break-beam, piezo-sensor, fsr-sensor
backend/          FastAPI, abonné MQTT, tests pytest (voir backend/README.md)
frontend/         tableau de bord web interactif (voir frontend/README.md)
infra/            compose.yaml, Mosquitto, Nginx, UFW, sauvegardes (voir infra/README.md)
docs/             schémas, résultats des essais, guides de déploiement
scripts/          outils de test et simulateurs (simulate_shots.py)
.github/workflows/
```

## Documentations détaillées

- **Cloud & Données (Étapes 5, 6, 7)** :
  - [Documentation Backend & Ingestion MQTT](backend/README.md)
  - [Documentation Tableau de bord Web](frontend/README.md)
  - [Documentation Infrastructure & Docker Compose](infra/README.md)
  - [Architecture Cloud & Traçabilité des exigences](docs/cloud-backend.md)
  - [Guide de déploiement sur VPS Ubuntu](docs/deployment-vps.md)
- **Embarqué (Étapes 2, 3, 4, 11)** :
  - [Firmware ESP32 du but](firmware/README.md)
  - [Custom chips Wokwi](chips/README.md)
  - [Firmware Gardien MPU6050](firmware-keeper/README.md)
- **Sécurité & Coordination** :
  - [Tests de sécurité T1 à T8](docs/security-tests.md)
  - [Briefs d'équipe](docs/briefs/README.md)

## Règles

- Aucun secret dans le dépôt : `.env` et `firmware/include/secrets.h` sont ignorés, seuls les fichiers d'exemple sont versionnés.
- Constantes nommées, un commit par étape qui marche.
- Code et commentaires en anglais, documentation en français.
