# Brief Alae : cloud et données (étapes 5, 6, 7)

## État vérifié (25 novembre 2026)

Toutes les étapes 5, 6 et 7 sont entièrement développées, intégrées et vérifiées :
- **Étape 5 (Backend, Broker & Base de données)** :
  - Stack Docker Compose orchestrant Mosquitto v2, FastAPI (Python 3.11/3.14) et PostgreSQL 17 Alpine avec volume persistant `postgres_data`.
  - Abonné MQTT asynchrone intégré au cycle de vie de FastAPI (écoute de `smartfootball/+/events` et `smartfootball/+/status`).
  - Validation stricte des messages : champs obligatoires, types, vitesse de 5.0 à 150.0 km/h (exigence SEC2), zones conformes au prototype 4 zones et compatibles extension 9 zones, valeurs ADC brutes pour pression FSR et piézo (jamais en Newtons).
  - Mécanisme anti-doublon (exigence SUR1) fondé sur l'unicité de `event_id` : insertion idempotente, une rafale de tirs renvoyée après coupure ne produit aucun doublon.
  - Calcul de la latence de réception serveur $\Delta t = |\text{received\_at} - \text{timestamp}|$ (exigence ENF1 < 2 s).
  - Validation automatisée : **53 tests pytest réussis sur 53** dans `backend/tests/`.
- **Étape 6 (Tableau de bord web)** :
  - Interface web réactive en HTML5/CSS3/JS natif (`frontend/`), légère et sans dépendance de compilation externe.
  - KPIs en direct : total de tirs, précision cadrée (%), vitesses moyenne et maximale, pression et intensité relatives ADC.
  - Représentation interactive du but avec heatmap dynamique (4 quadrants, indicateur hors cadre `none`, et bascule vers l'extension 9 zones).
  - Outil de comparaison côte à côte de deux séances d'entraînement.
  - Carte de supervision pour le module gardien MPU6050 (`keeper-esp32-01`).
  - Distinction explicite dans l'interface entre mesures physiques réelles, valeurs brutes relatives ADC et données de simulation Wokwi.
- **Étape 7 (Déploiement VPS)** :
  - Configuration Nginx en reverse proxy routant `/` vers le frontend et `/api/` vers FastAPI.
  - Ports sensibles (`5432` PostgreSQL et `8000` FastAPI) liés strictement à `127.0.0.1` pour éviter le contournement du pare-feu UFW par Docker.
  - Guide complet de déploiement rédigé (`docs/deployment-vps.md`) avec sauvegarde automatisée PostgreSQL (`infra/security/backup-postgres.sh`).
  - Script de simulation et de test (`scripts/simulate_shots.py`) validé pour injecter des tirs en HTTP/MQTT, tester la conformité SUR1 (10 tirs) et SEC2 (vitesse anormale).
  - Prêt pour Saad (connexion de l'ESP32 Wokwi sur l'IP publique du VPS) et Mohamed (intégration TLS 8883 et comptes ACL dans Mosquitto à l'étape 8a).

Tu portes tout ce qui reçoit, stocke et affiche les tirs : Mosquitto, FastAPI, PostgreSQL, le tableau de bord et le déploiement.

## Ce que tu reçois

Le format du message MQTT et les topics sont fixés (voir le README à la racine). Tu n'as pas besoin d'attendre le firmware pour commencer : tu peux publier de faux tirs avec `mosquitto_pub` en respectant le format.

## Étape 5 : Mosquitto, FastAPI, PostgreSQL en Docker Compose

- Un fichier `infra/compose.yaml`, base PostgreSQL sur un volume persistant.
- FastAPI s'abonne à `smartfootball/+/events` et à `smartfootball/+/status`.
- Valide chaque message avant d'écrire en base : champs obligatoires, types, vitesse dans la plage 5 à 150 km/h, `target_zone` parmi `upper_left`, `upper_right`, `lower_left`, `lower_right`, `none`.
- Anti-doublon : `event_id` unique en base. Un même tir renvoyé deux fois (après une coupure réseau) ne doit donner qu'une ligne.
- Une API qui ressort les tirs, avec des tests `pytest` dans `backend/tests/`.
- Mots de passe en variables d'environnement (`.env.example` existe déjà), jamais dans le dépôt.
- Au départ Mosquitto peut tourner sans TLS en local. Mohamed ajoute le TLS, les comptes et les ACL à l'étape 8a : garde la configuration dans `infra/` pour qu'il puisse la modifier.

Fini quand : un faux tir publié arrive en base et ressort par l'API.

## Étape 6 : tableau de bord

Fonctionnalités du cahier des charges : nombre total de tirs, vitesse moyenne et maximale, répartition par zone, taux de précision, historique des séances, pression relative, intensité relative des impacts, données du gardien si le module existe, comparaison entre séances.

Points importants :
- `pressure_reading` et `impact_intensity` sont des valeurs ADC brutes. Affiche-les comme une pression et une intensité relatives, jamais en Newton ni comme une « puissance de tir ».
- Le tableau de bord doit distinguer les mesures réelles, les estimations et les données de démonstration.
- Un tir sans impact arrive avec `target_zone = "none"`. Pour le taux de précision, ce sont les tirs qui ne touchent aucune zone. Ce choix est à confirmer avec Oussama.
- Choix React ou HTML/CSS/JavaScript : à décider avec l'équipe avant de commencer.
- L'extension à 9 zones doit rester possible sans changer le format du message.

Fini quand : les tirs en base s'affichent.

## Étape 7 : déploiement sur le VPS

- VPS Ubuntu, Docker Compose, Nginx pour HTTPS, avec Oussama et Mohamed (pare-feu UFW et certificats).
- Le broker doit être joignable depuis Internet : l'ESP32 simulé dans Wokwi sort par la passerelle publique, qui n'atteint pas une machine locale. La passerelle privée est payante, donc le VPS est indispensable.
- À vérifier tôt : l'ESP32 simulé peut-il joindre le broker sur le port choisi (1883 puis 8883 avec TLS) ? La documentation Wokwi ne le confirme pas. Dis-le à Saad dès que le broker répond.

Fini quand : un tir simulé apparaît en ligne.

## Exigences concernées

ENF1 : écart entre l'heure du tir et l'heure de réception inférieur à 2 s. SUR1 : broker arrêté, 10 tirs, broker relancé, 10 tirs en base sans doublon.

## Avec qui

Saad pour le format et le réseau, Mohamed pour la sécurité du broker et de l'API, Oussama pour le pare-feu, l'authentification Web, les rôles et les sauvegardes.
