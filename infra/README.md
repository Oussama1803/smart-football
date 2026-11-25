# Infrastructure & Déploiement Docker Compose (Étapes 5 & 7)

Ce répertoire contient la configuration Docker Compose, le paramétrage du broker Mosquitto, la configuration du reverse proxy Nginx, ainsi que les scripts d'administration système et de sécurité pour le déploiement sur VPS Ubuntu.

---

## 1. Vue d'ensemble de la pile de conteneurs

Le fichier `infra/compose.yaml` orchestre les quatre briques applicatives fondamentales du système Smart Football :

```text
                                Internet / Clients Wokwi
                                           |
                                           v
       +-----------------------------------------------------------------------+
       | VPS Ubuntu (Règles pare-feu UFW)                                      |
       |                                                                       |
       |  Port 80/443 (HTTP/HTTPS)                  Port 1883 / 8883 (MQTT)    |
       |         |                                             |               |
       |         v                                             v               |
       |  +--------------+                             +---------------+       |
       |  |    nginx     |                             |   mosquitto   |       |
       |  +-------+------+                             +-------+-------+       |
       |          |                                            |               |
       |          | proxy /api/* -> 8000                       | QoS 1         |
       |          v                                            v               |
       |  +------------------------------------------------------------+       |
       |  |                    smartfootball-backend                   |       |
       |  |          (FastAPI REST API & MQTT Worker paho-mqtt)        |       |
       |  +-----------------------------+------------------------------+       |
       |                                |                                      |
       |                                | Connexion SQL (127.0.0.1:5432)       |
       |                                v                                      |
       |  +------------------------------------------------------------+       |
       |  |                      smartfootball-db                      |       |
       |  |                 (PostgreSQL 17 Alpine)                     |       |
       |  |             Volume persistant: postgres_data               |       |
       |  +------------------------------------------------------------+       |
       +-----------------------------------------------------------------------+
```

---

## 2. Description des services (`infra/compose.yaml`)

### 2.1 `db` (PostgreSQL 17 Alpine)
- **Rôle** : Persistance des tirs (`shots`), des sessions d'entraînement et des statuts des appareils (`device_status`).
- **Nom du service** : `db` (aligné avec le script de sauvegarde `backup-postgres.sh`).
- **Volume persistant** : `postgres_data` monté sur `/var/lib/postgresql/data`. Les données restent intactes en cas d'arrêt ou de mise à jour des conteneurs.
- **Sécurité réseau** : Port exposé uniquement sur `127.0.0.1:5432` (`ports: - "127.0.0.1:5432:5432"`). Cette précaution est essentielle car Docker manipule directement `iptables` et court-circuiterait le pare-feu UFW si le port était publié sur `0.0.0.0:5432`.
- **Healthcheck** : Utilise `pg_isready` toutes les 5 secondes pour retarder le démarrage du backend jusqu'à la disponibilité effective de PostgreSQL.

### 2.2 `mosquitto` (Eclipse Mosquitto v2)
- **Rôle** : Broker de messages MQTT. Reçoit les publications télémétriques de l'ESP32 et les achemine vers le backend.
- **Ports exposés** :
  - `1883:1883` : Écoute MQTT standard sans chiffrement (pour les tests initiaux et la simulation Wokwi locale).
  - `8883:8883` : Écoute sécurisée MQTT sur TLS (mise en œuvre par Mohamed à l'étape 8a).
- **Configuration** : `infra/mosquitto/mosquitto.conf` monté en lecture seule (`:ro`).

### 2.3 `backend` (FastAPI + Worker MQTT)
- **Rôle** : Point d'entrée applicatif. S'abonne aux topics `smartfootball/+/events` et `smartfootball/+/status`, applique la validation métier (SEC2), assure l'anti-doublon (SUR1), enregistre en base et sert les données via l'API REST.
- **Dépendances** : Attend que `db` soit `healthy` et que `mosquitto` soit démarré avant de lancer le serveur.
- **Sécurité réseau** : Port exposé uniquement sur `127.0.0.1:8000:8000`, accessible depuis l'extérieur exclusivement via Nginx.

### 2.4 `nginx` (Serveur Web & Reverse Proxy)
- **Rôle** : Distribution des fichiers statiques du tableau de bord (`frontend/`) et redirection transparente des requêtes d'API vers le backend.
- **Routage** :
  - `/` $\rightarrow$ Fichiers HTML/CSS/JS du dossier `frontend/`.
  - `/api/` $\rightarrow$ Proxy vers `http://backend:8000/api/`.
  - `/health` $\rightarrow$ Proxy vers `http://backend:8000/health`.
  - `/docs` $\rightarrow$ Proxy vers la documentation Swagger UI FastAPI.

---

## 3. Sécurité réseau et pare-feu UFW

Par défaut sous Linux, le démon Docker injecte des règles dans la chaîne `PREROUTING` de Netfilter/iptables, ce qui contourne les règles configurées dans `ufw`.

Pour garantir l'intégrité du serveur VPS :
1. **Ports internes liés au loopback** : PostgreSQL et FastAPI sont cantonnés à l'adresse locale `127.0.0.1`.
2. **Ports publics autorisés** : Seuls les ports nécessaires sont ouverts sur l'interface publique du VPS :
   - Port `22/tcp` : Accès SSH administratif
   - Port `80/tcp` et `443/tcp` : Accès web au tableau de bord (HTTP / HTTPS)
   - Port `8883/tcp` (et `1883/tcp` en phase de test) : Broker MQTT pour l'ESP32 Wokwi
3. **Application du script UFW** :
   ```bash
   sudo chmod +x infra/security/ufw-setup.sh
   sudo ./infra/security/ufw-setup.sh
   sudo ufw status verbose
   ```

---

## 4. Sauvegarde automatique de la base de données

Le script `infra/security/backup-postgres.sh` automatise la sauvegarde quotidienne de la base de données PostgreSQL :
- Exécute `pg_dump` directement dans le conteneur `smartfootball-db`.
- Compresse l'archive au format `gzip`.
- Nommage automatique horodaté : `smartfootball_YYYYMMDD_HHMMSS.sql.gz`.
- Purge automatique des sauvegardes de plus de 7 jours.
- Vérifie la non-vacuité du dump produit.

Planification quotidienne dans `crontab -e` :
```cron
15 3 * * * cd /opt/smart-football/infra && ./security/backup-postgres.sh >> backups/backup.log 2>&1
```

---

## 5. Guide d'exploitation

### 5.1 Démarrage de tous les services
```bash
cd infra
docker compose up -d --build
```

### 5.2 Vérification de l'état des conteneurs
```bash
docker compose ps
```
Tous les conteneurs doivent afficher le statut `Up` (avec `(healthy)` pour `smartfootball-db`).

### 5.3 Consultation des journaux d'exécution
```bash
# Suivre les journaux du backend et de l'ingestion MQTT
docker compose logs -f backend

# Suivre les connexions au broker Mosquitto
docker compose logs -f mosquitto
```

### 5.4 Arrêt propre de l'infrastructure
```bash
docker compose down
```
*(Remarque : les données de la base PostgreSQL sont conservées dans le volume `postgres_data`).*
