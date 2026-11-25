# Smart Football : contexte pour Claude Code

Projet d'équipe (ESI, module systèmes embarqués), tout en simulation Wokwi. Chef de projet : Oussama Benachour. Lis `README.md` (format du message MQTT, architecture) avant d'écrire du code, puis `docs/briefs/BRIEF-oussama.md`.

## Règle principale

Le projet est noté par rapport au cahier des charges. Ne remplace aucun capteur ni aucune technologie du cahier des charges. Si une chose est impossible en simulation, propose la solution la plus proche, explique pourquoi et attends l'accord d'Oussama. Les seuls ajouts acceptés viennent des fiches de TD (4 LED, 1 buzzer, exigences chiffrées).

## Qui fait quoi

Dépôt privé : `Oussama1803/smart-football`. Les issues #1 à #14 portent les étapes.

- Oussama (chef de projet) : étapes 1, 8b (droits, UFW, sauvegardes, tests de sécurité), 10 (Prometheus et Grafana), 12b (démo, pas de rapport), coordination.
- Saad (`saadoooox`) : ESP32 et firmware, étapes 2, 3, 4, 11.
- Alae (`AlaeCa`) : API FastAPI, PostgreSQL, dashboard, étapes 5, 6, 7.
- Mohamed (`mohamedsamari`) : Mosquitto TLS, Nginx, Docker Compose, CI, étapes 8a et 9.

Oussama m'a demandé de travailler uniquement sur SA partie. Si une question touche la partie de quelqu'un d'autre, ne la décide pas : formule-la pour la personne concernée.

## Où on en est (à jour du 24 novembre 2026)

Sur GitHub : étapes 1 à 3, les briefs d'équipe (`docs/briefs/`), et la préparation de l'étape 8b (`infra/security/ufw-setup.sh`, `infra/security/backup-postgres.sh`, `docs/security-tests.md`).

Vérifié sur le PC d'Oussama (Windows) :
- `pio run -e esp32dev` : SUCCESS (RAM 6,8 %, flash 21,9 %).
- `pio test -e native` : 28 tests sur 28 passent.
- Wokwi CLI 0.29.1 fonctionne avec le jeton. Le firmware démarre en simulation (`[BOOT]` reconnu) et les trois chips se chargent.

NON vérifié, ne pas dire que ça marche :
- Les 4 scénarios `firmware/scenarios/*.test.yaml` finissent tous en timeout (30 s). Après le démarrage, les broches des barrières restent à 1 : le chip `ir-break-beam` ne coupe jamais le faisceau.
- Cause probable, non prouvée : `set-control` ne marche peut-être pas avec les contrôles des chips personnalisés (la doc Wokwi ne le confirme pas). Pistes : tester un chip qui se déclenche tout seul (attribut du `diagram.json`), ou piloter le tir autrement.
- Pas encore testés : noms de broches du `diagram.json` (`esp:VP`, `esp:TX`), seuil piézo 300 et échelle ADC, buzzer (LEDC avec `espressif32@6.9.0`), port 8883 depuis Wokwi.
- C'est du travail sur les étapes de Saad : ne pas s'y enfoncer sans l'accord d'Oussama.

Branche `feature/step5-cloud-backend` (Alae, étape 5) : compose (db, mosquitto, backend, nginx), API FastAPI, 53 tests pytest qui passent (lancés le 24 novembre). Pas encore fusionnée dans `main`. Points de sécurité relevés dans `docs/rbac.md`, partie 8.

Étape 8b, ajouté le 24 novembre : `docs/rbac.md` (rôles, droits, lien séance-joueur, exemple FastAPI testé, ACL Mosquitto testée sur un Mosquitto 2.0.18 local, droits PostgreSQL) et `infra/security/db-roles.sql` (testé sur PostgreSQL 16 jetable). Ni l'un ni l'autre n'a été essayé dans les conteneurs du compose.

Étape 10, ajouté le 25 novembre : `infra/compose.monitoring.yaml`, `infra/monitoring/` et `docs/monitoring.md`. Testé en local avec Docker sur le compose de `main` : cibles up, l'arrêt d'un faux ESP32 se voit dans Grafana (alerte `NoDeviceTraffic`), la panne de l'API aussi. Pas testé sur le VPS ni avec le vrai ESP32. Erreurs de traitement : attendent un `/metrics` dans le backend (proposition pour Alae dans `docs/monitoring.md`).

Les invitations GitHub de Saad, Alae et Mohamed étaient en attente le 22 novembre. Tant qu'elles ne sont pas acceptées, on ne peut pas leur assigner d'issues.

## Prochaines actions pour la partie d'Oussama

1. Étape 8b : rôles de l'API, droits MQTT, utilisateur de base à privilèges minimaux, UFW sur le VPS, sauvegardes, tests T1 à T8 avec captures. Dépend du `compose.yaml` et de l'API d'Alae (pas encore là).
2. Étape 10 : faite en local. Reste : lancer sur le VPS, compte MQTT `monitor` avec Mohamed, `/metrics` backend avec Alae.
3. Étape 12b : la démo des 10 points seulement, pas de rapport (décision d'Oussama, 25 novembre). Avant la démo, l'étape 12a (tests d'ensemble). À la fin, Oussama veut une page interactive de l'architecture (pas avant).
4. `docs/rbac.md` écrit : faire valider le tableau des droits et transmettre la partie 8 à Alae et Mohamed.

## Décisions à faire valider par l'équipe (pas à Oussama seul)

- Tir sans impact publié avec `target_zone = "none"` et `impact_intensity = 0` : à confirmer par Alae et Saad.
- Frontend : React ou HTML/JS simple (Alae).
- LED : zone touchée ou zone visée (Saad). Le firmware allume la zone touchée.
- `timestamp` sans NTP : temps de fonctionnement compté depuis 1970, pas une vraie date, remplacé à l'étape 4.
- FSR lu au passage de la barrière 2, pas à la frappe exacte.
- Les `.wasm` des chips sont versionnés dans `firmware/chips/`.
- La passerelle privée de Wokwi est payante : un VPS public est indispensable pour le broker.
- MQTT : une publication refusée par l'ACL est ignorée sans erreur (`mosquitto_pub` sort avec 0). Vérifié sur Mosquitto 2.0.18 local, pas encore sur le broker du compose.
- Docker contourne UFW sur les ports qu'il publie : vérifier avec un scan depuis l'extérieur.

## Spécificités du PC d'Oussama (Windows)

- Dossier du projet : `Documents\smart-football`. Ne touche à rien en dehors.
- PlatformIO est installé avec pip pour l'utilisateur. `pio` n'est pas dans le PATH : l'exécutable est dans `%APPDATA%\Python\Python314\Scripts\pio.exe`. Le script `firmware\.vscode\pio.ps1` le trouve tout seul.
- gcc (WinLibs) installé avec winget, nécessaire pour `pio test -e native`. `pio.ps1` l'ajoute au PATH.
- `firmware\.vscode\sim.ps1` télécharge Wokwi CLI si besoin (dans `.vscode\bin`) et lance les scénarios. Il lit le jeton dans la variable d'environnement utilisateur `WOKWI_CLI_TOKEN`. Ne jamais écrire ce jeton dans un fichier ni dans le chat.
- Git : `core.fileMode=false`, `autocrlf=true`, `.gitattributes` force LF pour les scripts shell et YAML.
- `.pio/`, `.vscode/` et `.env` sont ignorés par Git.

## Règles de travail

- Étape par étape, tester avant de passer à la suite, un commit par étape qui marche. Pull avant push, l'équipe pousse aussi.
- Distinguer mesure, estimation et valeur brute (`pressure_reading` et `impact_intensity` sont des valeurs ADC brutes).
- N'affirme pas qu'une fonction marche si tu ne l'as pas exécutée. Si quelque chose du brief paraît faux, dis-le avant de coder.
- Aucun secret dans le dépôt. Code et commentaires en anglais, documentation et messages pour l'équipe en français.
- Textes rédigés pour Oussama : naturels, sans tirets longs, sans formules d'IA.
