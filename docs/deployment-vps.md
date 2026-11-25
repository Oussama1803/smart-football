# Guide de déploiement sur VPS (Étape 7)

**Auteur** : Alae (Cloud & Données)  
**Avec** : Oussama (Chef de projet, Pare-feu) & Mohamed (Sécurité, Certificats TLS)  
**Projet** : Smart Football (ESI 5e année, Cybersécurité)  
**Dernière mise à jour** : 25 novembre 2026

---

## 1. Pourquoi le déploiement sur VPS est obligatoire

Dans Wokwi, la simulation ESP32 s'exécute dans un bac à sable utilisant la **passerelle publique Wokwi**, qui autorise uniquement les connexions sortantes vers Internet. Une passerelle privée (pour joindre une machine hôte locale sur `localhost` ou `192.168.x.x`) nécessite un abonnement payant.

Pour que l'ESP32 simulé par Saad puisse joindre le broker Mosquitto, ce dernier doit posséder une **adresse IP publique** ou un nom de domaine joignable sur Internet via un serveur VPS Ubuntu.

---

## 2. Préparation du serveur VPS Ubuntu

### 2.1 Mise à jour et prérequis système
Connectez-vous en SSH au VPS :
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y docker.io docker-compose-v2 git ufw curl gzip
sudo systemctl enable --now docker
```

### 2.2 Configuration du pare-feu UFW (Oussama / Alae)
Exécutez le script d'initialisation du pare-feu disponible dans le dépôt :
```bash
cd /opt/smart-football
sudo chmod +x infra/security/ufw-setup.sh
sudo ./infra/security/ufw-setup.sh
```

Vérifiez que seuls les ports autorisés sont ouverts :
```bash
sudo ufw status verbose
```
Règles attendues :
- `22/tcp` : SSH (administration)
- `80/tcp` : HTTP (tableau de bord et reverse proxy Nginx)
- `443/tcp` : HTTPS (après configuration SSL avec Mohamed)
- `1883/tcp` : Mosquitto MQTT (phase initiale et simulation)
- `8883/tcp` : Mosquitto MQTT sécurisé TLS (étape 8a)

*Attention : PostgreSQL (5432) et l'API FastAPI (8000) sont liés strictement à `127.0.0.1` dans `infra/compose.yaml` afin de ne jamais être exposés publiquement.*

---

## 3. Déploiement de la pile applicative Docker Compose

### 3.1 Cloner le dépôt et configurer l'environnement
```bash
cd /opt
sudo git clone git@github.com:Oussama1803/smart-football.git
cd /opt/smart-football
cp .env.example .env
nano .env
```
Définissez des mots de passe robustes pour la production :
- `POSTGRES_PASSWORD`
- `MQTT_BACKEND_PASSWORD`
- `API_SECRET_KEY`

### 3.2 Lancer les conteneurs
```bash
cd /opt/smart-football/infra
docker compose up -d --build
```

Vérifiez le statut opérationnel de la pile :
```bash
docker compose ps
```
Tous les conteneurs (`smartfootball-db`, `smartfootball-mosquitto`, `smartfootball-backend`, `smartfootball-nginx`) doivent afficher le statut `Up` (avec `(healthy)` pour la base de données).

---

## 4. Procédure de validation de l'étape 7 (Fini quand un tir apparaît en ligne)

### 4.1 Vérification de la santé du backend
```bash
curl -s http://localhost/health | jq .
```
Réponse attendue :
```json
{
  "status": "healthy",
  "database": "connected",
  "mqtt_service": "running",
  "mqtt_connected": true,
  "version": "0.1.0"
}
```

### 4.2 Injection d'un tir d'essai
Exécutez le script de simulation pour émettre un tir d'essai :
```bash
python3 /opt/smart-football/scripts/simulate_shots.py --mode single --api http://localhost
```

### 4.3 Validation sur le tableau de bord
Ouvrez votre navigateur web sur l'adresse IP publique de votre VPS :
```text
http://<IP_DU_VPS>/
```
- Le voyant de connexion doit être vert (**Connecté**).
- Le tir d'essai doit immédiatement apparaître dans le tableau d'historique et illuminer la zone correspondante du but interactif.
- Les indicateurs KPIs (vitesse, précision, pression relative) doivent se mettre à jour en direct.

---

## 5. Raccordement et coordination avec l'équipe

### 5.1 Pour Saad (Simulateur Wokwi / Firmware ESP32)
Dès que le broker est en ligne sur le VPS, testez sa disponibilité distante depuis votre poste local :
```bash
mosquitto_sub -h <IP_DU_VPS> -p 1883 -t 'smartfootball/#' -v
```
Puis configurez l'adresse IP du VPS dans `firmware/include/secrets.h` :
```c
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define MQTT_HOST "<IP_DU_VPS>"
#define MQTT_PORT 1883
#define MQTT_DEVICE_ID "goal-esp32-01"
```

### 5.2 Pour Mohamed (Sécurité MQTT TLS & Étape 8a)
- La configuration de Mosquitto est centralisée dans `infra/mosquitto/mosquitto.conf`.
- Le volume pour les certificats SSL/TLS est prêt à accueillir `ca.crt`, `server.crt` et `server.key`.
- Le port 8883 est déjà réservé et exposé dans `infra/compose.yaml`.

---

## 6. Automatisation des sauvegardes PostgreSQL

Planifiez la sauvegarde automatique quotidienne via `cron` (`sudo crontab -e`) :
```cron
# Sauvegarde quotidienne à 03h15 avec conservation sur 7 jours
15 3 * * * cd /opt/smart-football/infra && ./security/backup-postgres.sh >> backups/backup.log 2>&1
```

Vérifiez manuellement le bon fonctionnement du script :
```bash
cd /opt/smart-football/infra
sudo ./security/backup-postgres.sh
ls -lh backups/
```
