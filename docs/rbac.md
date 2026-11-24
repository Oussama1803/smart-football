# Rôles et droits (étape 8b)

Ce document propose les rôles de l'application, les droits MQTT et les droits de la base. C'est une proposition : les points qui touchent l'API (Alae) ou Mosquitto (Mohamed) sont à valider avec eux avant d'être codés. Il sert de base au test T7 de `docs/security-tests.md`.

## 1. Les trois rôles

| Rôle | Qui | En bref |
|---|---|---|
| `admin` | l'équipe projet | gère les comptes, les appareils et la configuration |
| `coach` | l'entraîneur | voit et compare les tirs de tous les joueurs |
| `player` | un joueur (ou le gardien) | ne voit que ses propres tirs et séances |

Un compte a un seul rôle. Pas de rôle « invité » : sans compte, on ne voit rien (test T3).

## 2. Tableau des droits

| Action | admin | coach | player |
|---|---|---|---|
| Voir ses propres tirs et séances | oui | oui | oui |
| Voir les tirs de tous les joueurs | oui | oui | non |
| Comparer des séances | oui | oui | ses propres séances |
| Créer ou fermer une séance d'entraînement | oui | oui | non |
| Voir l'état des appareils (`/api/devices`) | oui | oui | non |
| Gérer les comptes utilisateurs | oui | non | non |
| Enregistrer un tir à la main (`POST /api/shots`) | oui | non | non |
| Configurer les appareils et le broker | oui | non | non |

## 3. Rôle requis par route de l'API

Routes de la branche `feature/step5-cloud-backend` (Alae). Aujourd'hui, aucune n'est protégée.

| Route | Rôle proposé | Remarque |
|---|---|---|
| `GET /health` | public | utile pour Prometheus (étape 10). Ne renvoie aucune donnée de tir. |
| `GET /api/shots` | connecté | un `player` est forcé sur ses propres tirs, même s'il demande ceux d'un autre |
| `GET /api/shots/{event_id}` | connecté | 403 si un `player` demande le tir d'un autre joueur |
| `GET /api/stats` | connecté | même filtre que `/api/shots` |
| `GET /api/sessions` | connecté | un `player` ne voit que ses séances |
| `GET /api/devices` | `coach`, `admin` | |
| `POST /api/shots` | `admin` | la vraie arrivée des tirs passe par MQTT. Ouverte à tous, cette route permet d'injecter de faux tirs. |

Réponses attendues : 401 sans authentification, 403 quand le rôle ne suffit pas.

## 4. Lien entre joueurs, séances et tirs

Le message MQTT du cahier des charges ne contient pas de joueur : il contient un `session_id`. Le modèle actuel (`Shot`, `DeviceStatus`) n'a ni utilisateur ni joueur. Pour qu'un joueur ne voie que ses tirs, il faut relier une séance à un joueur.

Proposition la plus simple, sans changer le format du message :

```text
users              (id, username, password_hash, role, player_id NULL)
players            (id, display_name)
training_sessions  (session_id PK, player_id FK -> players, coach_id FK -> users, started_at, ended_at)
shots.session_id   -> training_sessions.session_id
```

Une séance appartient à un seul joueur. Le coach crée la séance avant les tirs, et le `session_id` envoyé par l'ESP32 désigne cette séance. Questions pour Alae et Saad : comment l'ESP32 apprend-il le `session_id` courant (constante, topic de commande, saisie) ? Un tir dont le `session_id` est inconnu est-il refusé ou rangé à part ?

## 5. Exemple FastAPI

Code testé à part, avec 5 tests (sans jeton, joueur forcé sur ses tirs, joueur refusé sur un autre joueur, coach qui lit un autre joueur, création de compte réservée à l'admin) : 5 sur 5 passent. Le jeton factice `FAKE_TOKENS` remplace l'authentification réelle (JWT ou cookie de session), qui reste à choisir par Alae.

```python
from enum import Enum
from fastapi import Depends, HTTPException, status


class Role(str, Enum):
    ADMIN = "admin"
    COACH = "coach"
    PLAYER = "player"


def get_current_user(...) -> User:
    # Reads the JWT or session cookie, returns the user, raises 401 otherwise.
    ...


def require_role(*allowed: Role):
    def checker(user: User = Depends(get_current_user)) -> User:
        if user.role not in allowed:
            raise HTTPException(status.HTTP_403_FORBIDDEN, "Forbidden for this role")
        return user
    return checker


@router.get("/api/devices")
def list_devices(user: User = Depends(require_role(Role.COACH, Role.ADMIN)), db=Depends(get_db)):
    ...


@router.get("/api/shots")
def list_shots(player_id: int | None = None, user: User = Depends(get_current_user), db=Depends(get_db)):
    if user.role == Role.PLAYER:
        if player_id is not None and player_id != user.player_id:
            raise HTTPException(status.HTTP_403_FORBIDDEN, "Players only see their own shots")
        player_id = user.player_id
    ...
```

Le filtre doit se faire côté serveur, dans la requête SQL. Cacher un bouton dans le tableau de bord ne protège rien.

## 6. Droits MQTT (ACL Mosquitto)

Un compte par appareil, avec le `device_id` comme nom d'utilisateur, et un compte pour le backend. Fichier `acl_file` proposé (le chemin est déjà prévu dans `infra/mosquitto/mosquitto.conf`, partie de Mohamed) :

```text
# Backend: reads every device's events and status, publishes nothing.
user backend
topic read smartfootball/+/events
topic read smartfootball/+/status

# Each device logs in with its device_id as username and only writes its own topics.
pattern write smartfootball/%u/events
pattern write smartfootball/%u/status
```

Testé le 24 novembre sur un Mosquitto 2.0.18 local (sans TLS, avec `allow_anonymous false`, ce fichier et un `password_file`) :

| Essai | Résultat |
|---|---|
| Publication sans identifiants | refusée : « Connection Refused: not authorised » |
| Mauvais mot de passe | refusée, même message |
| `goal-esp32-01` publie sur son topic | reçu par le backend |
| `goal-esp32-01` publie sur le topic de `goal-esp32-02` | non reçu, mais `mosquitto_pub` sort avec le code 0 |
| Le backend publie sur un topic d'appareil | non reçu |
| `goal-esp32-02` s'abonne à `smartfootball/#` | ne reçoit rien |

La ligne 4 confirme le point noté dans `CLAUDE.md` : une publication refusée par l'ACL est ignorée sans erreur côté client. Pour T2, il faut donc vérifier avec un abonné et en base, pas avec le message du client.

Pas encore testé : la même ACL dans l'image `eclipse-mosquitto:2` du compose, avec TLS sur 8883.

## 7. Droits de la base PostgreSQL

Script : `infra/security/db-roles.sql`. Il crée trois rôles.

| Rôle | Connexion | Droits |
|---|---|---|
| `sf_owner` | non | propriétaire des tables, seul à pouvoir créer ou modifier le schéma |
| `sf_app` | oui | `SELECT` et `INSERT` sur les tables, rien d'autre |
| `sf_backup` | oui | lecture seule, pour `pg_dump` |

Testé le 24 novembre sur un PostgreSQL 16 jetable, avec des tables `shots` et `device_status` créées avant le script, comme le fait `init_db` :

- `sf_app` insère et lit un tir.
- `sf_app` est refusé pour `DELETE`, `UPDATE`, `DROP TABLE` et `CREATE TABLE`.
- `sf_backup` est refusé pour `INSERT`, et `pg_dump` avec ce rôle sort bien les données.
- Une table créée plus tard par `sf_owner` est tout de suite lisible et inscriptible par `sf_app`.

Pas encore testé : le script dans le conteneur `postgres:17-alpine` du compose. L'authentification par mot de passe n'a pas été testée non plus, car la base jetable était en mode `trust`.

Ce que ça change pour Alae : `DATABASE_URL` du backend utilise `sf_app` et non plus le superutilisateur. `init_db` (`create_all`) ne crée que les tables manquantes. Avec `sf_app`, il ne peut plus en créer : une nouvelle table (par exemple `users`) doit être créée une fois avec `SET ROLE sf_owner`, ou par une migration. Une table modifiable (`training_sessions.ended_at`, `users`) demandera un `GRANT UPDATE` ciblé, à ajouter au script quand le modèle sera fixé.

## 8. Points relevés sur la branche de l'étape 5

Les 53 tests de `backend/tests` passent (lancés le 24 novembre avec pytest, Python 3.13). Points de sécurité à transmettre, sans rien changer à leur code :

Pour Mohamed (étape 8a) :
- `mosquitto` publie `1883:1883` sur toutes les interfaces avec `allow_anonymous true`. Docker contourne UFW, donc le port 1883 sans mot de passe serait ouvert sur Internet. T1 et T4 échouent tant que ce n'est pas corrigé. Le backend peut garder 1883 à l'intérieur du réseau Docker, sans le publier.
- `compose.yaml` monte `./nginx/nginx.conf`, qui n'existe pas encore dans le dépôt.

Pour Alae :
- Aucune authentification sur l'API (T3 et T7 échouent pour l'instant).
- `POST /api/shots` est ouvert à tous : n'importe qui peut ajouter des tirs.
- CORS avec `allow_origins=["*"]` et `allow_credentials=True`. Si l'authentification passe par un cookie, il faudra limiter `allow_origins` au domaine du tableau de bord.
- Le backend se connecte avec le superutilisateur PostgreSQL (voir partie 7).
- `compose.yaml` a des valeurs par défaut `change-me` pour les mots de passe. Si `.env` manque, tout démarre avec ces mots de passe. La syntaxe `${POSTGRES_PASSWORD:?}` ferait échouer le démarrage à la place.

## 9. À décider

1. Les droits du tableau (partie 2) : à valider par l'équipe.
2. Le lien séance-joueur (partie 4) : Alae et Saad.
3. JWT ou cookie de session : Alae.
4. L'ACL (partie 6) dans la configuration de Mohamed.
5. `backup-postgres.sh` utilise aujourd'hui `POSTGRES_USER`. Il pourrait passer sur `sf_backup`.
