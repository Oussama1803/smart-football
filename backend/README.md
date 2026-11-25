# Backend FastAPI & Ingestion MQTT (Étape 5)

Ce composant assure l'ingestion temps réel des tirs provenant du but intelligent ESP32, la validation des données selon le cahier des charges et les exigences de cybersécurité, le stockage persistant dans PostgreSQL sans doublon, et l'exposition d'une API REST pour le tableau de bord web.

---

## 1. Architecture et technologies

```text
[ESP32 / Wokwi] ---> (MQTT QoS 1) ---> [Mosquitto Broker]
                                              |
                                     (smartfootball/+/events)
                                              v
                                  [FastAPI Background Worker]
                                              |
                          +-------------------+-------------------+
                          |                                       |
                          v                                       v
               [Validation & Sécurité]                 [Anti-doublon SUR1]
               - Vitesse: [5, 150] km/h (SEC2)         - Unicité event_id
               - 10 champs obligatoires                - Idempotence stricte
               - Zones: 4 zones / none / 9 zones
               - Valeurs ADC brutes (non-Newtons)
                          |                                       |
                          +-------------------+-------------------+
                                              |
                                              v
                                  [PostgreSQL 17 Database]
                                              ^
                                              | (Requêtes REST)
                                  [API REST FastAPI /api/...]
                                              ^
                                              |
                                  [Tableau de bord Nginx]
```

- **Framework Web** : FastAPI (Python 3.11 / 3.14) avec Uvicorn.
- **ORM & Persistance** : SQLAlchemy 2.0 connecté à PostgreSQL 17.
- **Client MQTT** : `paho-mqtt` v2.x (exécuté en thread asynchrone non-bloquant via le cycle de vie `lifespan` de FastAPI).
- **Validation & Typage** : Pydantic v2.
- **Tests** : Pytest avec 53 tests unitaires et d'intégration validés.

---

## 2. Structure du code

```text
backend/
├── app/
│   ├── __init__.py
│   ├── main.py              # Point d'entrée FastAPI, cycle de vie (lifespan), routage et healthcheck
│   ├── config.py            # Configuration centralisée (pydantic-settings, variables d'environnement)
│   ├── database.py          # Session SQLAlchemy, moteur PostgreSQL, fonction d'initialisation init_db
│   ├── models.py            # Modèles de données relationnels (Shot, DeviceStatus)
│   ├── schemas.py           # Schémas de validation et de sérialisation Pydantic
│   ├── validation.py        # Règles métier, validation des vitesses, des zones et des types
│   ├── services.py          # Logique d'écriture idempotente (SUR1) et calcul des statistiques
│   ├── mqtt_service.py      # Client abonné MQTT, boucle d'écoute, décodage JSON et horodatage
│   └── api/
│       ├── __init__.py
│       ├── shots.py         # Endpoints GET /api/shots, GET /api/shots/{id}, POST /api/shots
│       ├── stats.py         # Endpoints GET /api/stats, GET /api/stats/compare, GET /api/sessions
│       └── devices.py       # Endpoints GET /api/devices, GET /api/devices/{id}/status
├── tests/
│   ├── conftest.py          # Fixtures de test SQLite en mémoire et client HTTP de test
│   ├── test_validation.py   # Tests des contraintes de vitesse (SEC2), des zones et des champs
│   ├── test_deduplication.py# Tests du comportement anti-doublon (SUR1, tir unique et rafale de 10)
│   ├── test_api.py          # Tests des routes REST (/health, /api/shots, /api/stats, /api/devices)
│   └── test_mqtt.py         # Tests du décodage MQTT, gestion des erreurs et Last Will
├── Dockerfile               # Image conteneur optimisée Python 3.11-slim
├── pytest.ini               # Configuration de l'environnement de test
└── requirements.txt         # Dépendances Python épinglées
```

---

## 3. Pipeline de validation et conformité aux exigences

Chaque tir reçu sur le topic MQTT `smartfootball/<device_id>/events` ou envoyé via `POST /api/shots` subit un contrôle strict avant écriture en base :

### 3.1 Contrôle des champs obligatoires
Le message doit être un objet JSON valide contenant exactement les 10 champs définis au cahier des charges :
`event_id`, `device_id`, `session_id`, `event_type`, `timestamp`, `ball_speed_kmh`, `pressure_reading`, `target_zone`, `impact_intensity`, `firmware_version`.

### 3.2 Plage de vitesse valide (Exigence SEC2)
$$\text{Vitesse autorisée} \in [5.0, 150.0] \text{ km/h}$$
Tout tir dont la vitesse mesurée est inférieure à 5 km/h ou supérieure à 150 km/h est rejeté par `validate_speed`. Le message n'est pas inséré dans la base et une entrée de journal d'alerte sécurité est enregistrée.

### 3.3 Zones cibles autorisées
- **Prototype 4 zones** : `upper_left`, `upper_right`, `lower_left`, `lower_right`.
- **Tir non cadré / sans impact** : `none`.
- **Extension 9 zones supportée sans modification de schéma** : `center`, `upper_center`, `lower_center`, `middle_left`, `middle_right`.

### 3.4 Valeurs ADC brutes (non converties en Newtons)
Conformément aux règles du projet, `pressure_reading` (capteur FSR) et `impact_intensity` (capteurs piézoélectriques) sont stockés comme des entiers représentant les lectures ADC brutes $[0, 4095]$. Elles ne sont jamais présentées comme des unités physiques de force (Newtons) ou de puissance.

### 3.5 Mécanisme anti-doublon (Exigence SUR1)
- La colonne `event_id` est unique dans la table `shots`.
- La méthode `record_shot()` intercepte les collisions d'identifiant de manière idempotente : si un tir avec le même `event_id` a déjà été enregistré, l'insertion est ignorée sans erreur.
- Lors d'une perte de connexion réseau, l'ESP32 stocke jusqu'à 20 tirs en mémoire tampon (SUR1). À la reconnexion, la réémission des tirs n'entraîne aucune duplication en base de données.

### 3.6 Horodatage et latence de réception (Exigence ENF1)
- Chaque tir enregistré reçoit automatiquement un horodatage serveur `received_at` à la milliseconde près.
- L'écart temporel $\Delta t = |\text{received\_at} - \text{timestamp}|$ est calculé à la restitution et exposé via le champ `latency_ms`. L'exigence ENF1 impose $\Delta t < 2.0\text{ s}$.

---

## 4. Référence des points d'accès API REST

| Méthode | Route | Description | Paramètres / Filtres |
|---|---|---|---|
| `GET` | `/health` | Santé du système (PostgreSQL, MQTT, version) | Aucun |
| `GET` | `/api/shots` | Liste paginée des tirs enregistrés (tri anti-chronologique) | `session_id`, `device_id`, `target_zone`, `limit` (défaut: 50), `offset` |
| `GET` | `/api/shots/{event_id}` | Détail complet d'un tir par son UUID | `event_id` |
| `POST` | `/api/shots` | Ingestion directe d'un tir (HTTP fallback avec anti-doublon) | Corps JSON du tir |
| `GET` | `/api/stats` | KPIs globaux ou filtrés par séance | `session_id` |
| `GET` | `/api/stats/compare` | Comparaison côte à côte de deux séances d'entraînement | `session_a`, `session_b` |
| `GET` | `/api/sessions` | Liste de tous les identifiants de séances uniques | Aucun |
| `GET` | `/api/devices` | Liste des capteurs/buts avec état, dernier tir et heartbeat | Aucun |
| `GET` | `/api/devices/{device_id}/status` | Dernier statut et heartbeat d'un appareil | `device_id` |

### Exemple de réponse `GET /api/stats` :
```json
{
  "total_shots": 42,
  "shots_on_target": 35,
  "missed_shots": 7,
  "accuracy_rate_pct": 83.3,
  "avg_speed_kmh": 78.4,
  "max_speed_kmh": 104.2,
  "min_speed_kmh": 58.1,
  "avg_pressure_reading": 620.5,
  "avg_impact_intensity": 745.8,
  "zone_distribution": {
    "upper_left": 8,
    "upper_right": 11,
    "lower_left": 9,
    "lower_right": 7,
    "none": 7
  }
}
```

---

## 5. Lancement et développement local

### 5.1 Prérequis
- Python 3.10 ou supérieur
- Un environnement virtuel recommandé

### 5.2 Installation des dépendances
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r backend/requirements.txt
```

### 5.3 Configuration de l'environnement (`.env`)
Copiez `.env.example` à la racine du projet ou définissez les variables :
```bash
export DATABASE_URL="postgresql://smartfootball:change-me@localhost:5432/smartfootball"
export MQTT_HOST="localhost"
export MQTT_PORT=1883
export ENABLE_MQTT_SUBSCRIBER="true"
```

### 5.4 Lancement du serveur en développement
```bash
cd backend
uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```
La documentation interactive OpenAPI est disponible sur `http://localhost:8000/docs`.

---

## 6. Exécution de la suite de tests

Les 53 tests unitaires et d'intégration valident la logique métier sans dépendre d'une base PostgreSQL externe (ils s'exécutent sur une base SQLite mémoire dédiée) :

```bash
pytest backend/tests/ -v
```

Résultats attendus :
```text
backend/tests/test_api.py::test_health_check PASSED
backend/tests/test_api.py::test_ingest_shot_api_success PASSED
backend/tests/test_api.py::test_ingest_shot_api_deduplication PASSED
backend/tests/test_deduplication.py::test_sur1_retransmission_ten_shots PASSED
backend/tests/test_validation.py::test_out_of_range_speed_rejected[4.9] PASSED
backend/tests/test_validation.py::test_out_of_range_speed_rejected[150.1] PASSED
backend/tests/test_mqtt.py::test_mqtt_handle_valid_event_message PASSED
...
======================== 53 passed in 0.47s ========================
```
