# Guide de déploiement sur VPS (Étape 7)

Auteur : Alae (Cloud & Données)  
Avec : Oussama (Chef de projet, Pare-feu) & Mohamed (Sécurité, Certificats TLS)

---

## 1. Pourquoi le déploiement sur VPS est obligatoire

Dans Wokwi, la simulation ESP32 utilise la **passerelle publique Wokwi**, qui autorise uniquement les connexions sortantes vers Internet. Une passerelle privée (pour joindre une machine sur le réseau local `localhost`) est payante.

Pour que l'ESP32 simulé par Saad puisse joindre le broker Mosquitto, ce dernier doit posséder une **IP publique** ou un nom de domaine joignable sur Internet via le VPS Ubuntu.

---

## 2. Préparation du serveur VPS Ubuntu

### 2.1 Mise à jour et prérequis
Connectez-vous en SSH au VPS :
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y docker.io docker-compose-v2 git ufw curl gzip
sudo systemctl enable --now docker
```

### 2.2 Configuration du pare-feu UFW (Oussama / Alae)
Exécutez le script préparé dans le dépôt :
```bash
cd /opt/smart-football
sudo chmod +x infra/security/ufw-setup.sh
sudo ./infra/security/ufw-setup.sh
```

Vérifiez que seuls les ports 22, 80, 443 et 8883 (ou 1883 en phase de test initiale) sont ouverts :
```bash
sudo ufw status verbose
```

---

## 3. Déploiement de la pile Docker Compose

### 3.1 Cloner le dépôt et configurer `.env`
```bash
cd /opt/smart-football
cp .env.example .env
nano .env
```
Renseignez les mots de passe de production (`POSTGRES_PASSWORD`, `MQTT_BACKEND_PASSWORD`, `API_SECRET_KEY`).

### 3.2 Lancer les conteneurs
```bash
cd /opt/smart-football/infra
docker compose up -d --build
```

Vérifiez le statut des conteneurs :
```bash
docker compose ps
```

### 3.3 Vérification initiale (Fini de l'étape 5 & 7)
1. **Santé de l'API** :
   ```bash
   curl http://localhost/health
   # Réponse attendue : {"status":"healthy","database":"connected",...}
   ```
2. **Publication d'un faux tir de test** :
   ```bash
   python3 ../scripts/simulate_shots.py --mode single --api http://localhost
   ```
3. **Vérification dans le tableau de bord** :
   Ouvrez votre navigateur sur `http://<IP_DU_VPS>/` pour voir le tir apparaître en temps réel dans le but et dans l'historique.

---

## 4. Test de connectivité pour Saad (Simulateur Wokwi)

Dès que le broker répond sur le VPS, testez la joignabilité depuis une machine distante :
```bash
mosquitto_sub -h <IP_DU_VPS> -p 1883 -t 'smartfootball/#' -v
```
Puis informez Saad qu'il peut configurer l'adresse IP du VPS dans `firmware/include/secrets.h`.

---

## 5. Sauvegardes automatisées de PostgreSQL

Ajoutez le script de sauvegarde à la table cron de root (`crontab -e`) pour une sauvegarde quotidienne à 03h15 :
```cron
15 3 * * * cd /opt/smart-football/infra && ./security/backup-postgres.sh >> backups/backup.log 2>&1
```
