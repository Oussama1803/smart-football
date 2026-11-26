# Sécurité MQTT et Web (étape 8a)

Responsable : Mohamed. Branche : `mohamed/etape-8a-securite-mqtt`.

Ce document explique ce qui a été mis en place, comment l'installer (en local et sur le VPS), et ce que chaque membre de l'équipe doit savoir. Les résultats des tests sont dans [`security-tests.md`](security-tests.md).

## 1. Ce qui est protégé

| Repère | Risque (fiches de TD) | Réponse |
|---|---|---|
| M3 | Publier de faux tirs sur le broker | Connexion anonyme interdite, un compte par appareil, ACL par topic (SEC1) |
| M2 | Lire les identifiants en mémoire par USB | Un compte par appareil : un ESP32 compromis ne peut publier que sur ses propres topics. Aucun secret dans Git |
| — | Écoute du réseau | MQTT en TLS (8883) et Web en HTTPS (443) |
| M5 | Pousser un firmware modifié par OTA | **Non traité**, à signaler dans le rapport |

Avant l'étape 8a, n'importe qui pouvait publier sur le port 1883, sans mot de passe et en clair (captures `avant-8a-*.png`).

## 2. Architecture

```text
ESP32 ── MQTT/TLS 8883 ──► Mosquitto ◄── MQTT 1883 (réseau Docker interne) ── backend
                           (comptes + ACL)
Navigateur ── HTTPS 443 ──► Nginx ── HTTP (réseau Docker interne) ──► backend
Navigateur ── HTTP 80 ────► Nginx : redirection 301 vers HTTPS
```

| Port | Service | Ouvert vers l'extérieur | Chiffré | Authentification |
|---|---|---|---|---|
| 8883 | Mosquitto (appareils) | oui | TLS 1.2 minimum | compte + ACL |
| 1883 | Mosquitto (backend) | **non**, réseau Docker seulement | non | compte + ACL |
| 443 | Nginx (tableau de bord, API) | oui | TLS 1.2 / 1.3, HSTS | étape 8b (Oussama) |
| 80 | Nginx | oui | — | redirige vers 443 |

Le port 1883 reste en clair, mais il n'est joignable que par les conteneurs du réseau Docker du projet. Il exige quand même un compte.

## 3. Comptes et droits (ACL)

| Compte | Peut publier sur | Peut lire |
|---|---|---|
| `goal-esp32-01` | `smartfootball/goal-esp32-01/events` et `.../status` | rien |
| `backend` | rien | `smartfootball/+/events` et `smartfootball/+/status` |
| sans compte | rien (connexion refusée) | rien |

Tout ce qui n'est pas écrit dans `infra/mosquitto/acl` est interdit. Le Last Will (`offline`) et le heartbeat de l'ESP32 passent par `.../status`, donc ils sont autorisés.

## 4. Fichiers

| Fichier | Dans Git | Contenu |
|---|---|---|
| `infra/mosquitto/mosquitto.conf` | oui | listeners 1883 et 8883, TLS, `allow_anonymous false` |
| `infra/mosquitto/acl` | oui | règles par compte (aucun secret) |
| `infra/compose.yaml` | oui | montage des fichiers, 1883 non publié, 443 ajouté |
| `infra/nginx/nginx.conf` | oui | HTTPS, redirection 80 → 443, HSTS |
| `infra/mosquitto/passwords` | **non** | comptes MQTT (mots de passe hachés) |
| `infra/mosquitto/certs/` | **non** | `ca.crt`, `ca.key`, `server.crt`, `server.key` |
| `infra/nginx/certs/` | **non** | `web.crt`, `web.key` |
| `.env` | **non** | mots de passe en clair (`MQTT_BACKEND_PASSWORD`, `MQTT_DEVICE_PASSWORD`, …) |

`ca.key` est la clé de l'autorité de certification. Elle ne doit jamais être copiée sur le VPS ni envoyée à quelqu'un : elle sert seulement à signer les certificats, sur la machine de Mohamed.

## 5. Installation

Les commandes sont données pour PowerShell (Windows). Sur le VPS (Linux), remplacer `${PWD}\...` par `$(pwd)/...`. OpenSSL : `C:\Program Files\Git\usr\bin\openssl.exe` sous Windows, `openssl` sous Linux.

### 5.1 Certificats MQTT

Dans `infra/mosquitto/certs/` :

```
openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.crt -days 365 -subj "/CN=SmartFootball-CA"
openssl req -newkey rsa:2048 -nodes -keyout server.key -out server.csr -subj "/CN=NOM_DU_SERVEUR"
```

Fichier `server.ext` (une seule ligne) :

```
subjectAltName = DNS:NOM_DU_SERVEUR, DNS:mosquitto, DNS:localhost
```

```
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365 -extfile server.ext
openssl verify -CAfile ca.crt server.crt
```

- En local, `NOM_DU_SERVEUR` = `localhost`.
- Sur le VPS, `NOM_DU_SERVEUR` = le **nom de domaine** du VPS. L'ESP32 vérifie que ce nom correspond à celui du certificat (`skip_cert_common_name_check = false` dans le firmware). Utiliser un nom DNS plutôt qu'une adresse IP.
- `DNS:mosquitto` permet aux conteneurs du réseau Docker de se connecter en TLS.

### 5.2 Comptes MQTT

Les mots de passe sont générés aléatoirement (24 caractères), écrits dans `.env`, puis lus depuis `.env` : jamais tapés à la main ni affichés.

```
-join ((48..57)+(65..90)+(97..122) | Get-Random -Count 24 | ForEach-Object {[char]$_})
```

Après avoir rempli `MQTT_BACKEND_PASSWORD` et `MQTT_DEVICE_PASSWORD` dans `.env` :

```
docker compose --env-file .env -f infra/compose.yaml stop mosquitto
$be = (Select-String '^MQTT_BACKEND_PASSWORD=' .env).Line.Split('=',2)[1].Trim()
$de = (Select-String '^MQTT_DEVICE_PASSWORD=' .env).Line.Split('=',2)[1].Trim()
docker run --rm -v "${PWD}\infra\mosquitto:/work" eclipse-mosquitto:2 mosquitto_passwd -c -b /work/passwords goal-esp32-01 $de
docker run --rm -v "${PWD}\infra\mosquitto:/work" eclipse-mosquitto:2 mosquitto_passwd -b /work/passwords backend $be
docker compose --env-file .env -f infra/compose.yaml up -d --force-recreate mosquitto backend
```

Pièges rencontrés avec Mosquitto 2.1 :
- `mosquitto_passwd -c` refuse d'écraser un fichier existant : supprimer l'ancien `passwords` d'abord.
- Arrêter le conteneur Mosquitto avant de supprimer ou recréer `passwords`, sinon Docker garde une trace du fichier monté (« File exists »). Si Docker démarre Mosquitto alors que `passwords` n'existe pas, il crée un **dossier** vide à sa place.
- `mosquitto_passwd` crée le fichier en mode `0600` pour `root`, alors que Mosquitto tourne en tant qu'utilisateur `mosquitto` : il ne peut pas le lire. C'est pour cela que `compose.yaml` monte les secrets dans `/mosquitto/secrets/` puis les copie au démarrage avec le bon propriétaire (`command: install -o mosquitto ...`).
- Ne pas coller un mot de passe à l'invite `Password:` : le collage peut transmettre un autre texte sans que rien ne s'affiche. Utiliser `-b` avec les variables lues depuis `.env`.

### 5.3 Certificat Web

En local, `infra/nginx/certs/web.crt` est signé par notre CA (même méthode qu'en 5.1, avec `web.ext` = `subjectAltName = DNS:localhost, IP:127.0.0.1`). Le navigateur affiche un avertissement, c'est normal.

Sur le VPS, utiliser un certificat **Let's Encrypt** pour le nom de domaine (reconnu par les navigateurs), et le monter à la place de `web.crt` et `web.key`. À organiser avec Alae et Oussama (renouvellement automatique tous les 90 jours).

### 5.4 Vérification

```
docker logs smartfootball-mosquitto --tail 10
docker logs smartfootball-backend --tail 5
```

Attendu : `mosquitto version 2.x running`, ports 1883 et 8883 ouverts, et côté backend `Connected to Mosquitto broker` puis `Subscribed to ...`. Puis lancer les tests T1, T2, T5 et T6 de `security-tests.md`.

## 6. Ajouter un appareil (ou en retirer un)

1. Générer un mot de passe et l'ajouter au fichier (sans `-c`) :
   `mosquitto_passwd -b /work/passwords <device_id> <mot_de_passe>`
2. Ajouter son bloc dans `infra/mosquitto/acl` :
   ```
   user <device_id>
   topic write smartfootball/<device_id>/events
   topic write smartfootball/<device_id>/status
   ```
3. Redémarrer Mosquitto : `docker compose --env-file .env -f infra/compose.yaml restart mosquitto`.

Appareil perdu ou compromis : supprimer son compte (`mosquitto_passwd -D /work/passwords <device_id>`), retirer son bloc de l'ACL, puis redémarrer Mosquitto. Ses publications sont refusées immédiatement.

Le module gardien (`firmware-keeper`) ne publie pas encore en MQTT : il n'a pas de compte pour l'instant.

## 7. Pour chaque membre de l'équipe

### Saad (firmware ESP32)

Dans `firmware/include/secrets.h` (jamais dans Git) :

| Constante | Valeur |
|---|---|
| `SF_MQTT_HOST` | nom de domaine du VPS (le même que dans le certificat serveur) |
| `SF_MQTT_PORT` | `8883` |
| `SF_MQTT_USERNAME` | `goal-esp32-01` |
| `SF_MQTT_PASSWORD` | transmis en privé par Mohamed, jamais par écrit dans le dépôt ni dans un groupe |
| `SF_MQTT_CA_PEM` | contenu de `ca.crt` (le certificat **public** de la CA, jamais `ca.key`) |

Le `DEVICE_ID` du firmware doit rester `goal-esp32-01`, sinon l'ACL bloque les publications.

### Alae (cloud et données)

- Backend : aucun changement de code. Il se connecte avec `MQTT_BACKEND_USER` / `MQTT_BACKEND_PASSWORD` de `.env`.
- `infra/README.md` et `docs/deployment-vps.md` : mettre à jour les passages qui disent que 1883 est ouvert sans mot de passe.
- `scripts/simulate_shots.py --mqtt` publie sur 1883 sans identifiants : il est maintenant refusé. Il faut lui ajouter TLS (`ca.crt`), le port 8883 et un compte dédié (à créer par Mohamed).
- Sur le VPS : installer `.env`, `infra/mosquitto/passwords` et les certificats à la main (ils ne sont pas dans Git).

### Oussama (pare-feu, authentification Web)

- UFW : ouvrir 22, 80, 443 et 8883. **Ne pas ouvrir 1883.**
- `POST /api/shots` (via Nginx, `/api/`) accepte des tirs **sans authentification** : quelqu'un peut contourner la sécurité MQTT par HTTP. À protéger à l'étape 8b.
- `/metrics` (Prometheus) n'est pas exposé par Nginx aujourd'hui : à garder ainsi, ou à protéger.
- `compose.yaml` utilise des valeurs par défaut `:-change-me` : si `.env` manque sur le VPS, les services démarrent avec ce mot de passe. Vérifier `.env` à chaque déploiement.

## 8. Limites connues

- **Tests faits en local** : T1, T2, T5 et T6 sont à refaire sur le VPS, depuis l'extérieur, avec le vrai nom de domaine.
- **Liaison ESP32 → broker non testée de bout en bout** : la simulation Wokwi ne peut joindre qu'un serveur sur Internet, donc ce test se fera sur le VPS.
- **CA privée** : les certificats MQTT expirent après 365 jours et doivent être renouvelés. Le certificat Web local n'est pas reconnu par les navigateurs (Let's Encrypt sur le VPS).
- **Authentification de l'appareil par mot de passe**, pas par certificat client (mTLS). Un mot de passe extrait d'un ESP32 donne accès aux seuls topics de cet appareil, grâce à l'ACL.
- **Port 1883 interne en clair** : acceptable car limité au réseau Docker du serveur, avec compte obligatoire.
- **M5 (mise à jour OTA)** : non traité.