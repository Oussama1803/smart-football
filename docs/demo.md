# Déroulé de la démonstration (étape 12b)

Les 10 points du cahier des charges, dans l'ordre où on les montre, avec qui fait quoi et ce qu'on voit à l'écran. Durée visée : 10 à 12 minutes.

Deux plans :
- **Plan A** : le VPS est en ligne avec le broker sécurisé. Toute la chaîne est réelle, de Wokwi jusqu'à Grafana.
- **Plan B** : pas de VPS. Les points 1 à 4 se font dans Wokwi, et les points 5 à 9 en local sur le PC d'Oussama, avec le simulateur de tirs d'Alae à la place de l'ESP32. Il faut le dire au jury : la passerelle privée de Wokwi est payante, donc l'ESP32 simulé ne peut pas joindre un broker installé sur un PC.

## Écrans à ouvrir avant de commencer

| Fenêtre | Plan A | Plan B |
|---|---|---|
| Wokwi (VS Code), moniteur série visible | oui | oui |
| Tableau de bord | `https://<VPS>/` | http://localhost/ |
| Grafana, tableau « Smart Football : supervision » | http://localhost:3000 par tunnel SSH | http://localhost:3000 |
| Terminal pour le point 10 | oui | oui |

## Les 10 points

| # | Point | Qui | Ce qu'on fait | Ce qu'on montre |
|---|---|---|---|---|
| 1 | Un tir | Saad | Dans Wokwi, banc `firmware/wokwi-test`, scénario 1 (`upper_right`), on passe « Run shot » de 0 à 1 | Le JSON `"event_type": "shot"` dans le moniteur série |
| 2 | Pression FSR | Saad | Même tir | `pressure_reading` dans le JSON. On précise que c'est une valeur ADC brute, pas des newtons. |
| 3 | Vitesse par les barrières IR | Saad | Même tir | `ball_speed_kmh` vaut environ 72 : 20 cm parcourus en 10 ms |
| 4 | Zone par les piézos | Saad | On rejoue avec le scénario 4 (aucun impact) puis le scénario 5 (deux impacts simultanés) | `target_zone` change, et la LED de la zone touchée s'allume |
| 5 | Envoi MQTT sécurisé | Mohamed | Plan A : le tir part vers le VPS sur le port 8883 | Plan A : la ligne « connected » du firmware et le log Mosquitto. Plan B : le simulateur publie en local, sans TLS (on le dit). |
| 6 | Validation par le backend | Alae | Mode `sec2-test` du simulateur : un tir à 3 km/h et un tir hors limite haute (la règle accepte de 5 à 150 km/h) | L'API refuse les deux tirs, et rien n'arrive en base |
| 7 | Enregistrement dans PostgreSQL | Alae | Après un tir valide | La ligne du tir dans `GET /api/shots`, avec le même `event_id` |
| 8 | Affichage dans le tableau de bord | Alae | Rafraîchir le tableau de bord | Le tir dans l'historique et la heatmap du but |
| 9 | Métriques dans Grafana | Oussama | On arrête la simulation | En 2 à 3 minutes, « Messages MQTT reçus » et « Appareils connectés » passent au rouge, et l'alerte `NoDeviceTraffic` se déclenche |
| 10 | Refus d'un accès MQTT non autorisé | Oussama | Publication sans mot de passe, puis sur le topic d'un autre appareil | « Connection Refused: not authorised », puis rien n'arrive chez l'abonné ni en base |

Point 9 : la baisse prend 2 à 3 minutes. On arrête donc la simulation juste après le point 8, puis on fait le point 10 pendant l'attente, avant de revenir sur Grafana.

## Commandes

### Plan B : lancer la pile sur le PC d'Oussama

Depuis `infra` (voir `docs/monitoring.md` pour le fichier `.env`) :

```text
docker compose -f compose.yaml -f compose.monitoring.yaml up -d
```

### Points 5 à 8 : simuler des tirs (plan B)

Depuis la racine du dépôt (il faut `pip install paho-mqtt` une seule fois) :

```text
python scripts/simulate_shots.py --mqtt --broker localhost --port 1883 --mode seed-demo --count 15
python scripts/simulate_shots.py --mode sec2-test --api http://localhost:8000
```

La première commande publie 15 tirs par MQTT, répartis sur deux séances, pour remplir le tableau de bord. La seconde envoie en HTTP, directement à l'API, des tirs dont la vitesse sort des limites. Le refus s'affiche dans le terminal (point 6). En plan A, remplacer `localhost` par l'adresse du VPS.

### Point 10 : accès refusé

Plan A, depuis n'importe quel PC (le fichier `ca.crt` vient de Mohamed) :

```text
mosquitto_pub -h <VPS> -p 8883 --cafile ca.crt -t smartfootball/goal-esp32-01/events -m test
```

Attendu : « Connection Refused: not authorised ». Ensuite, avec le compte de `goal-esp32-01`, on publie sur `smartfootball/autre-appareil/events`. Le client ne signale aucune erreur (c'est normal en MQTT 3.1.1), mais l'abonné du backend ne reçoit rien. Pour les commandes exactes, voir les tests T1 et T2 dans `docs/security-tests.md`.

Plan B : sur `main`, Mosquitto accepte encore les connexions anonymes. Le point 10 ne se démontre donc pas tant que l'étape 8a (Mohamed) n'est pas appliquée. Pour montrer quand même le mécanisme, on peut utiliser l'ACL testée en local, décrite dans `docs/rbac.md`, partie 6.

## Checklist la veille

- [ ] Plan A ou B décidé (le VPS répond-il ?)
- [ ] `pio run -e esp32dev` passe, et le banc `firmware/wokwi-test` émet un tir pour les scénarios 1, 4 et 5
- [ ] Pile Docker lancée, 9 conteneurs « Up », tableau Grafana avec l'API, le site et le broker au vert
- [ ] `.env` rempli. Aucun secret affiché à l'écran pendant la démo : ne pas ouvrir `.env` ni `secrets.h` devant le jury.
- [ ] Tableau de bord vidé ou rempli exprès (le mode `seed-demo` ajoute 15 tirs)
- [ ] Une répétition complète, chronométrée
- [ ] Captures de secours de chaque point dans `docs/captures/`, au cas où une partie ne répond pas le jour J

## Ce qu'il faut dire au jury, sans attendre qu'il le demande

- `pressure_reading` et `impact_intensity` sont des valeurs ADC brutes, pas des forces.
- Le FSR est lu au passage de la barrière 2, pas à la frappe exacte.
- Un tir sans impact est publié avec `target_zone = "none"` et `impact_intensity = 0`. Ce n'est pas dans le format du cahier des charges, on l'a ajouté pour calculer le taux de précision.
- Pas traité : le risque M5 (firmware modifié par OTA).
- Si on est en plan B, tout ce qui n'a pas pu être montré de bout en bout, et pourquoi.
