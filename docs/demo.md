# Déroulé de la démonstration (étape 12b)

Les 10 points du cahier des charges, dans l'ordre où on les montre, avec qui fait quoi et ce qu'on voit à l'écran. Durée visée : 10 à 12 minutes.

**Décision du 26 novembre : la démo se fait en local, sans VPS.** La pile Docker complète (Mosquitto avec TLS et ACL, backend, PostgreSQL, Nginx en HTTPS, Prometheus et Grafana) tourne sur le PC d'Oussama.

L'ESP32 simulé dans Wokwi ne peut joindre qu'un serveur accessible sur Internet. La passerelle privée de Wokwi, qui permettrait d'atteindre un PC, est payante. Donc :

- les points 1 à 4 se montrent dans Wokwi (moniteur série) ;
- les points 5 à 9 se montrent sur la pile locale. Les messages sont publiés avec le vrai compte de l'ESP32 (`goal-esp32-01`), en TLS sur le port 8883, depuis le conteneur Mosquitto.

Il faut le dire au jury : le VPS du cahier est remplacé par la même pile en local, déployable telle quelle (`docs/deployment-vps.md`).

## Écrans à ouvrir avant de commencer

| Fenêtre | Adresse ou commande |
|---|---|
| Wokwi (VS Code), banc `firmware/wokwi-test`, moniteur série visible | Saad |
| Tableau de bord | https://localhost (accepter l'avertissement : certificat de notre propre CA) |
| Grafana, tableau « Smart Football : supervision », période « Last 15 minutes » | http://localhost:3000 |
| Alertes de Prometheus | http://localhost:9090/alerts |
| Deux fenêtres PowerShell dans `infra/`, avec le mot de passe de l'ESP32 chargé (ci-dessous) | Oussama |

Charger le mot de passe sans l'afficher, dans chaque fenêtre PowerShell :

```text
$de = (Select-String '^MQTT_DEVICE_PASSWORD=' .env).Line.Split('=',2)[1].Trim()
```

Ne jamais ouvrir `.env`, `passwords` ni `secrets.h` à l'écran.

## Les 10 points

| # | Point | Qui | Ce qu'on fait | Ce qu'on montre |
|---|---|---|---|---|
| 1 | Un tir | Saad | Wokwi, banc `firmware/wokwi-test`, scénario 1 (`upper_right`), « Run shot » de 0 à 1 | Le JSON `"event_type": "shot"` dans le moniteur série |
| 2 | Pression FSR | Saad | Même tir | `pressure_reading` : valeur ADC brute, pas des newtons |
| 3 | Vitesse par les barrières IR | Saad | Même tir | `ball_speed_kmh` d'environ 72 : 20 cm en 10 ms |
| 4 | Zone par les piézos | Saad | Scénarios 4 (aucun impact) et 5 (impacts simultanés) | `target_zone` change, et la LED de la zone touchée s'allume |
| 5 | Envoi MQTT sécurisé | Oussama, Mohamed | Commande « Points 5 à 8 » ci-dessous : un tir au format du cahier, publié en TLS (8883), avec le compte `goal-esp32-01` | La commande réussit avec le certificat de la CA et le compte de l'appareil |
| 6 | Validation par le backend | Alae | Mode `sec2-test` du simulateur (vitesses hors limites), et un message qui n'est pas du JSON | Les tirs sont refusés, et « Erreurs de traitement » passe à l'orange dans Grafana |
| 7 | Enregistrement dans PostgreSQL | Alae | Après le tir du point 5 | `https://localhost/api/shots?session_id=session-demo` montre le tir, avec le même `event_id` |
| 8 | Affichage dans le tableau de bord | Alae | Séance `session-demo` dans le tableau de bord | Le tir dans l'historique et la heatmap du but |
| 9 | Métriques dans Grafana | Oussama | Heartbeat de l'appareil pendant 2 minutes, puis arrêt | Vert pendant l'envoi (messages, appareil en ligne), puis rouge et alerte `NoDeviceTraffic` |
| 10 | Refus d'un accès MQTT non autorisé | Mohamed, Oussama | Publication sans compte, puis sur le topic d'un autre appareil | « Connection Refused: not authorised », puis le refus de l'ACL |

Point 9 : la baisse prend 2 à 3 minutes. On lance le heartbeat dès le début de la démo de la plateforme, puis on l'arrête juste avant le point 10. On revient sur Grafana après le point 10.

## Commandes (depuis `infra/`, PowerShell)

### Avant : la pile tourne

```text
docker compose -f compose.yaml -f compose.monitoring.yaml up -d
docker ps --format "table {{.Names}}\t{{.Ports}}"
```

Dans Grafana, « Broker allumé depuis » doit afficher une durée. Si c'est « No data » : `docker restart smartfootball-mosquitto-exporter`.

### Points 5 à 8 : un tir publié en TLS avec le compte de l'ESP32

Le JSON passe par un fichier, pour éviter les problèmes de guillemets de PowerShell. L'`event_id` change à chaque lancement (sinon, le backend ignore le doublon).

```text
$id = "demo-" + (Get-Date -Format "HHmmss")
$ts = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'")
$shot = '{"event_id":"' + $id + '","device_id":"goal-esp32-01","session_id":"session-demo","event_type":"shot","timestamp":"' + $ts + '","ball_speed_kmh":72.0,"pressure_reading":640,"target_zone":"upper_right","impact_intensity":845,"firmware_version":"0.1.0"}'
Set-Content -Encoding ascii -NoNewline shot.json $shot
docker cp shot.json smartfootball-mosquitto:/tmp/shot.json
docker exec smartfootball-mosquitto mosquitto_pub -h localhost -p 8883 --cafile /mosquitto/certs/ca.crt -u goal-esp32-01 -P $de -q 1 -t smartfootball/goal-esp32-01/events -f /tmp/shot.json
Remove-Item shot.json
```

Ce tir est une donnée de démonstration, avec les valeurs de l'exemple du README (72 km/h, `upper_right`), pas une mesure de Wokwi.

### Point 6 : validation

```text
python ..\scripts\simulate_shots.py --mode sec2-test --api http://localhost:8000
for ($i=0; $i -lt 5; $i++) { docker exec smartfootball-mosquitto mosquitto_pub -h localhost -p 8883 --cafile /mosquitto/certs/ca.crt -u goal-esp32-01 -P $de -t smartfootball/goal-esp32-01/events -m pasdujson; Start-Sleep 2 }
```

La première commande envoie en HTTP des tirs à 3 km/h et hors limite haute, et l'API les refuse. La seconde publie en MQTT des messages qui ne sont pas du JSON. Ils sont refusés et comptés dans « Erreurs de traitement ».

### Point 9 : heartbeat de l'appareil (2 minutes)

```text
for ($i=0; $i -lt 12; $i++) { docker exec smartfootball-mosquitto mosquitto_pub -h localhost -p 8883 --cafile /mosquitto/certs/ca.crt -u goal-esp32-01 -P $de -t smartfootball/goal-esp32-01/status -m heartbeat; Start-Sleep 10 }
```

### Point 10 : accès refusé

Sans compte :

```text
docker exec smartfootball-mosquitto mosquitto_pub -h localhost -p 8883 --cafile /mosquitto/certs/ca.crt -t smartfootball/goal-esp32-01/events -m test
```

Attendu : `Connection Refused: not authorised`.

Avec le compte de l'ESP32, sur le topic d'un autre appareil (MQTT v5, pour voir le code de refus) :

```text
docker exec smartfootball-mosquitto mosquitto_pub -V mqttv5 -d -h localhost -p 8883 --cafile /mosquitto/certs/ca.crt -u goal-esp32-01 -P $de -q 1 -t smartfootball/autre-appareil/events -m test
```

Attendu : un `PUBACK` avec le code 135 (Not authorized). En MQTT 3.1.1, le refus est silencieux pour le client : c'est l'abonné et la base qui prouvent que rien n'est passé (test T2 de Mohamed).

## Ce qui a été testé, et ce qui ne l'a pas été

- Testé sur le PC d'Oussama le 26 novembre : la pile avec le 8a, le heartbeat en TLS avec le compte de l'ESP32 (appareil « en ligne » dans Grafana), les messages non JSON comptés en erreurs, le compte `monitor`, `sf_backup` refusé sur `DELETE`, les ports.
- **Pas encore lancées** : la commande du tir des points 5 à 8, et les deux commandes du point 10 depuis le conteneur. À essayer pendant la répétition.

## Checklist avant la démo

- [ ] Pile lancée, « Broker allumé depuis » avec une durée dans Grafana
- [ ] Banc `firmware/wokwi-test` : un tir pour les scénarios 1, 4 et 5
- [ ] Mot de passe chargé dans les fenêtres PowerShell (`$de`), `.env` jamais ouvert à l'écran
- [ ] Une répétition complète, chronométrée
- [ ] Captures de secours de chaque point dans `docs/captures/`

## Ce qu'il faut dire au jury, sans attendre qu'il le demande

- `pressure_reading` et `impact_intensity` sont des valeurs ADC brutes, pas des forces.
- Le FSR est lu au passage de la barrière 2, pas à la frappe exacte.
- Un tir sans impact est publié avec `target_zone = "none"` et `impact_intensity = 0`. Ce n'est pas dans le format du cahier, on l'a ajouté pour calculer le taux de précision.
- Les tirs du tableau de bord sont des données de démonstration (simulateur et commande ci-dessus), pas des mesures de Wokwi.
- Pas de VPS : la même pile tourne en local (raison : la passerelle privée de Wokwi est payante).
- Pas encore fait : la connexion et les rôles dans l'API (tests T3 et T7), le module gardien publié en MQTT (extension), et le risque M5 (firmware modifié par OTA).
