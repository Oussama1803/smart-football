# Briefs d'équipe

Un brief par personne. Chacun dit ce qui est déjà fait, ce qu'il reste à faire, comment on saura que c'est fini, et ce dont on dépend. Les tâches détaillées sont dans les issues GitHub (`[Prénom] Étape N : ...`).

| Brief | Étapes |
|---|---|
| [Oussama](BRIEF-oussama.md) | 1, 8b, 10, 12b, et la coordination |
| [Saad](BRIEF-saad.md) | 2, 3, 4, 11 |
| [Alae](BRIEF-alae.md) | 5, 6, 7 |
| [Mohamed](BRIEF-mohamed.md) | 8a, 9 |

## Règles communes

- Le projet est noté par rapport au cahier des charges. On ne remplace aucun capteur ni aucune technologie. Si une chose est impossible en simulation, on propose la solution la plus proche et on attend l'accord d'Oussama avant de l'appliquer.
- Les seuls ajouts acceptés viennent de nos fiches de TD : 4 LED, 1 buzzer, les exigences chiffrées et les risques.
- Aucun secret dans le dépôt (mots de passe, clés, `.env`, `secrets.h`). On versionne les fichiers d'exemple.
- Un commit par étape qui marche. On teste avant de passer à la suite, et on n'écrit pas « ça marche » pour une chose qu'on n'a pas lancée.
- On distingue une mesure, une estimation et une valeur brute. `pressure_reading` et `impact_intensity` sont des valeurs ADC brutes, pas des Newtons.
- Code et commentaires en anglais, documentation et messages pour l'équipe en français.

## Le message MQTT (ne pas changer les noms)

Voir le README à la racine : `event_id`, `device_id`, `session_id`, `event_type`, `timestamp`, `ball_speed_kmh`, `pressure_reading`, `target_zone`, `impact_intensity`, `firmware_version`. Topics : `smartfootball/<device_id>/events` (QoS 1) et `smartfootball/<device_id>/status`.

## Exigences à justifier avec un résultat noté

| N° | Exigence | Qui la teste |
|---|---|---|
| EF1 | Vitesse de 5 à 150 km/h, à 5 % près | Saad |
| EF2 | Zone touchée parmi 4 en moins de 100 ms | Saad |
| ENF1 | Tir publié en MQTT en moins de 2 s | Saad et Alae |
| ENF2 | Moins de 500 mA sous 5 V (par calcul) | Saad |
| SEC1 | 100 % des publications sans identifiants refusées | Mohamed |
| SEC2 | Vitesse hors de 5 à 150 km/h rejetée et signalée | Saad |
| SUR1 | 20 tirs gardés pendant une coupure réseau, renvoyés sans doublon | Saad et Alae |
| SUR2 | Redémarrage tout seul en moins de 5 s si le programme se bloque | Saad |
