# Firmware de l'ESP32 du but (étape 3, sans réseau)

Mesure la vitesse (2 barrières IR), lit le FSR à la frappe, trouve la zone touchée (4 piézos), allume la LED de la zone, fait bipper le buzzer et affiche sur le port série le JSON qui sera publié en MQTT à l'étape 4.

## Lancer

Prérequis : VS Code avec l'extension PlatformIO et l'extension Wokwi.

1. Compiler : `pio run -e esp32dev` (produit `.pio/build/esp32dev/firmware.bin` et `.elf`).
2. Simuler : F1 puis « Wokwi: Start Simulator » dans le dossier `firmware/`.
3. Tests unitaires (sur le PC, sans carte) : `pio test -e native`.
4. Scénario Wokwi CI (nécessite `wokwi-cli` et un jeton `WOKWI_CLI_TOKEN`) :
   `wokwi-cli . --scenario scenarios/shot-72kmh.test.yaml`

Les custom chips sont déjà compilés dans `firmware/chips/` (`.wasm` + `.json`). Pour les recompiler : `chips/build.sh` (clang avec la cible wasm32 et `wasm-ld`).

## Utiliser les contrôles Wokwi pendant la démo

Un tir se simule avec la barrière 1 (`beam1`), qui est le maître :

1. Régler `ballSpeedKmh` (par exemple 72).
2. Passer `shoot` de 0 à 1. La barrière 1 se coupe, la barrière 2 se coupe `D / v` plus tard (10 ms à 72 km/h) : le bon écart sans coupler deux curseurs à la main. Les deux barrières restent deux composants distincts reliés par leur broche `SYNC`.
3. Pour l'impact : régler `peakVolts` du piézo de la zone voulue, puis passer son `trigger` de 0 à 1, dans les 60 ms après le passage.
4. Pression : curseur `pressure` du composant `fsr1` (avant le tir).
5. Remettre `shoot` et `trigger` à 0 avant le tir suivant.

## Format de sortie

Les lignes qui commencent par `[` sont des logs. Les lignes qui commencent par `{` sont du JSON : un tir (format du README racine) ou un message d'état (`device_id`, `timestamp`, `level`, `message`) pour les erreurs comme « vitesse rejetée ».

## Limites connues de cette étape

- Le `timestamp` n'est pas une vraie date : sans NTP, c'est le temps de fonctionnement compté depuis 1970. L'étape 4 le remplace.
- Un tir sans impact dans la fenêtre est publié avec `target_zone = "none"` et `impact_intensity = 0` (pour le taux de précision). Ce n'est pas dans le format du cahier des charges, à valider.
- Les LED indiquent la zone touchée. La fonction « zone à viser » des fiches de TD n'est pas faite.
- Ni le heartbeat, ni le watchdog, ni la file hors ligne ne sont faits (étape 4).
