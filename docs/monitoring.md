# Supervision : Prometheus et Grafana (étape 10)

L'étape est finie quand l'arrêt de la simulation se voit dans Grafana. Ce document dit ce qui est en place, comment le lancer et ce qui a été testé.

## Ce qui est surveillé

| Indicateur du cahier | D'où vient la mesure | Panneau Grafana |
|---|---|---|
| CPU et RAM | node-exporter | CPU, RAM, courbe « CPU et RAM » |
| Espace disque | node-exporter | Disque / |
| Disponibilité du serveur | node-exporter, blackbox sur Nginx | Serveur allumé depuis, Tableau de bord (Nginx) |
| Disponibilité de l'API | blackbox sur `/health` | API (/health) |
| État du broker | blackbox (TCP), mosquitto-exporter (`$SYS`) | Broker MQTT, Broker allumé depuis |
| Messages reçus | mosquitto-exporter | Messages MQTT reçus (2 min), Publications reçues par minute |
| Déconnexions des appareils | mosquitto-exporter, en partie | Appareils connectés, Clients connectés au broker |
| Erreurs de traitement | pas encore | nécessite `/metrics` dans le backend (voir plus bas) |

La sonde de l'API vérifie aussi le contenu de la réponse : `/health` répond 200 même quand la base est injoignable (`"status": "degraded"`). La sonde ne compte donc l'API comme disponible que si la réponse contient `"status": "healthy"`.

L'ESP32 envoie un heartbeat toutes les 30 s (`firmware/src/network.cpp`). Si le broker ne reçoit aucune publication pendant 2 minutes, la simulation est arrêtée ou l'appareil est déconnecté. Le panneau « Messages MQTT reçus (2 min) » passe alors au rouge et l'alerte `NoDeviceTraffic` se déclenche.

Alertes (onglet Alerts de Prometheus) : `ApiDown`, `BrokerDown`, `NoDeviceTraffic`, `DiskAlmostFull`.

## Fichiers

```text
infra/compose.monitoring.yaml                 services prometheus, grafana, node-exporter, blackbox-exporter, mosquitto-exporter
infra/monitoring/prometheus.yml               cibles
infra/monitoring/alerts.yml                   alertes
infra/monitoring/blackbox.yml                 sondes HTTP et TCP
infra/monitoring/grafana/provisioning/        source de données et chargement du tableau
infra/monitoring/grafana/dashboards/smartfootball.json   le tableau « Smart Football : supervision »
```

Le `compose.yaml` d'Alae n'est pas modifié : la supervision s'ajoute par-dessus.

## Lancer

Sur le VPS, depuis `infra/`, avec `GRAFANA_ADMIN_PASSWORD` renseigné dans `.env` (le démarrage échoue s'il manque) :

```bash
docker compose -f compose.yaml -f compose.monitoring.yaml up -d
```

Grafana (3000) et Prometheus (9090) n'écoutent que sur `127.0.0.1`. Ils ne sont pas ouverts sur Internet, ce qui reste cohérent avec `ufw-setup.sh` (Docker contourne UFW). Pour y accéder depuis un PC, ouvrir un tunnel SSH :

```bash
ssh -L 3000:127.0.0.1:3000 -L 9090:127.0.0.1:9090 utilisateur@VPS
```

Ensuite, ouvrir http://localhost:3000, se connecter avec `admin` et le mot de passe du `.env`, puis aller dans le dossier Smart Football.

## Pour la démo (point 9)

1. Ouvrir le tableau Grafana avec la simulation Wokwi en marche : messages reçus au vert, au moins un appareil connecté.
2. Arrêter la simulation dans Wokwi.
3. En 2 à 3 minutes, « Messages MQTT reçus (2 min) » et « Appareils connectés » passent à 0, en rouge. La courbe des publications tombe à 0, et l'alerte `NoDeviceTraffic` passe en `firing` dans Prometheus.

## Testé le 25 novembre (en local, pas sur le VPS)

Pile complète lancée avec Docker : `compose.yaml` d'Alae, plus `compose.monitoring.yaml`.

- Les 6 cibles Prometheus sont `up`.
- Un faux ESP32 (conteneur `mosquitto_pub`, même JSON de heartbeat, un message toutes les 10 s) : 11 publications sur 2 minutes, 1 appareil connecté, pas d'alerte. Le backend enregistre bien les heartbeats.
- Faux ESP32 arrêté : 0 publication, 0 appareil, et `NoDeviceTraffic` passe en `firing` après environ 3 minutes.
- Backend arrêté : « API (/health) » affiche EN PANNE, et `ApiDown` passe en `firing`.
- Sans connexion, Grafana affiche la page de login.

Pas encore testé :
- Sur le VPS, avec le vrai ESP32 simulé dans Wokwi.
- Avec le Mosquitto sécurisé de l'étape 8a. Le test a tourné avec `allow_anonymous true`, la configuration actuelle de `main`.
- Le montage `/:/host:ro,rslave` du node-exporter. Il a dû être remplacé par `/:/host:ro` dans le conteneur de test. C'est la configuration recommandée par node-exporter pour un vrai Ubuntu, mais elle n'a pas été lancée ici.

## Ce qui dépend des autres

Pour Mohamed (étape 8a) : quand l'accès anonyme sera coupé, le mosquitto-exporter aura besoin d'un compte en lecture seule sur `$SYS/#`. Il se connecte au port interne 1883, dans le réseau Docker. Proposition d'ACL :

```text
user monitor
topic read $SYS/#
```

Les identifiants vont dans `.env` (`MQTT_MONITOR_USER`, `MQTT_MONITOR_PASSWORD`).

Pour Alae : les erreurs de traitement et les déconnexions annoncées par le Last Will ne sont connues que du backend. Proposition, non codée et non testée :
- Ajouter `prometheus-client` aux dépendances.
- Exposer `/metrics` avec `make_asgi_app()`.
- Compter dans `mqtt_service.py` :
  - `smartfootball_mqtt_messages_total` (par type : `shot`, `status`) ;
  - `smartfootball_processing_errors_total` (validation refusée, erreur de base) ;
  - `smartfootball_device_offline_total` (statut `offline` reçu).
- Ne pas rendre `/metrics` accessible par Nginx. La configuration actuelle n'expose que `/api/`, `/docs`, `/openapi.json` et `/health`, donc c'est déjà le cas.

Une fois `/metrics` en place, il suffit d'ajouter cette cible dans `prometheus.yml`, puis de remplacer le panneau texte « À venir » par deux courbes :

```yaml
  - job_name: backend
    static_configs:
      - targets: ["backend:8000"]
```
