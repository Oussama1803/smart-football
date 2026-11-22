# Smart Football : contexte pour Claude Code

Projet d'équipe (ESI, module systèmes embarqués), tout en simulation Wokwi. Chef de projet : Oussama. Lis `README.md` (format du message MQTT, architecture) avant d'écrire du code.

## Règle principale

Le projet est noté par rapport au cahier des charges. Ne remplace aucun capteur ni aucune technologie du cahier des charges. Si une chose est impossible en simulation, propose la solution la plus proche, explique pourquoi et attends l'accord d'Oussama. Les seuls ajouts acceptés viennent des fiches de TD (4 LED, 1 buzzer, exigences chiffrées).

## Où on en est

- Étape 1 faite : squelette, README, `.gitignore`, `.env.example` (commit `55c7350`).
- Étapes 2 et 3 faites, commit `456e753` : 3 custom chips Wokwi (`chips/`), `firmware/diagram.json`, firmware sans réseau, 28 tests unitaires, 4 scénarios Wokwi CI.
- Les 28 tests ont été lancés avec g++ et Unity, jamais avec `pio test -e native`.
- JAMAIS exécutés : `pio run -e esp32dev`, la simulation Wokwi, les scénarios CI. Aucune affirmation « ça marche » avant de les avoir lancés.
- Pas encore poussé sur GitHub (dépôt privé `Oussama1803/smart-football`, qui ne contient que son README de départ). Le script `envoyer-sur-github.ps1` fait le push.

## Prochaines actions

1. Pousser sur GitHub (pull avec `--allow-unrelated-histories`, puis push).
2. Installer/vérifier PlatformIO, compiler `firmware/`, corriger ce qui casse.
3. Lancer `pio test -e native`, puis la simulation Wokwi et les 4 scénarios dans `firmware/scenarios/`.
4. Vérifier les points non confirmés : noms de broches du `diagram.json` (`esp:VP`, `esp:TX`), attribut `role` des chips, seuil piézo (300) et échelle ADC, buzzer (API LEDC avec `espressif32@6.9.0`).
5. Seulement après : étape 4 (Wi-Fi, NTP, MQTT, heartbeat). Ne pas dépasser l'étape que valide Oussama.

## Décisions à faire valider par l'équipe

- Tir sans impact : publié avec `target_zone = "none"` et `impact_intensity = 0` (pour le taux de précision). Pas dans le format du cahier des charges.
- `timestamp` sans NTP : temps de fonctionnement compté depuis 1970, pas une vraie date, remplacé à l'étape 4.
- LED : elles indiquent la zone touchée, pas une zone à viser.
- FSR lu au passage de la barrière 2, pas à la frappe exacte.
- Les `.wasm` des chips sont versionnés dans `firmware/chips/`.
- La passerelle privée de Wokwi (accès à une machine locale) est payante : le VPS est indispensable à l'étape 7.

## Règles de travail

- Étape par étape, tester avant de passer à la suite, un commit par étape qui marche.
- Distinguer mesure, estimation et valeur brute (`pressure_reading` et `impact_intensity` sont des valeurs ADC brutes).
- N'affirme pas qu'une fonction marche si tu ne l'as pas exécutée. Si quelque chose du brief paraît faux, dis-le avant de coder.
- Aucun secret dans le dépôt. Code et commentaires en anglais, documentation et messages pour l'équipe en français.
- Dans le dossier Documents, ne touche à rien en dehors de `smart-football`.
