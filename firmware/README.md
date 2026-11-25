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
# Partie embarquée ESP32 — état et validation

## Circuit principal

Le diagramme Wokwi utilise une ESP32 DevKit. Les deux barrières IR ont leur
sortie active à LOW : IR1 va sur GPIO25 et IR2 sur GPIO26 ; SYNC relie le maître
beam1 à l'esclave beam2. À 72 km/h, le custom chip produit les fronts descendants
avec 10 ms d'écart. Le maître est déclenché par le passage de `shoot` de 0 à 1.

Les quatre sorties piézo vont sur ADC1 : piezo1/GPIO32 upper_left,
piezo2/GPIO33 upper_right, piezo3/GPIO34 lower_left et piezo4/GPIO35
lower_right. Le firmware conserve les maxima sur 60 ms après IR2 ; à partir de
300 ADC, la plus forte zone gagne (premier indice en cas d'égalité). Le capteur
FSR va sur GPIO36/VP et donne une valeur ADC relative. Il ne mesure pas des
newtons. Les LED sont sur GPIO16–19 avec 220 ohms. Le buzzer est sur GPIO23.

La vitesse utilise 0,20 m divisés par le temps entre IR1 et IR2, convertie en
km/h. Le passage inverse, les vitesses hors 5–150 km/h, les fronts anti-rebond
et les mesures incomplètes sont rejetés. Un débordement de la file ISR abandonne
la mesure et produit un état d'erreur.

## Communication Wi-Fi et MQTT/TLS

Le profil livré par défaut reste `esp32dev` pour la simulation locale. Le profil
`esp32net` active le réseau ; avant de l'utiliser, copier
`include/secrets.h.example` vers `include/secrets.h` et fournir l'hôte du broker,
le port TLS, le compte appareil et son CA PEM de confiance. Ce fichier est ignoré
par Git. Sans hôte, identifiants et certificat, le client reste désactivé. La
validation du certificat et du nom d'hôte demeure activée.

Le Wi-Fi se reconnecte périodiquement sans bloquer l'acquisition. ESP-MQTT est
fourni par le SDK Arduino ESP32 2.0.17 ; sa tâche réseau utilise l'enqueue QoS 1
et traite PUBACK, reconnecte le client et configure un Last Will `offline` sur
`smartfootball/<device_id>/status`. Un heartbeat `online` est mis en file toutes
les 30 secondes. La SNTP callback fournit une base d'heure : les événements
gardent leur instant monotone d'acquisition et attendent la synchronisation avant
d'être publiés avec un timestamp UTC.

La file RAM garde au plus 20 JSON de tir. Les entrées ne sont retirées qu'après
PUBACK ; lors d'une perte de connexion, elles conservent leur `event_id` et
retentent. Cette file n'est pas persistante : un redémarrage perd les entrées en
RAM. Elle n'assure donc pas la persistance stricte à travers une coupure
d'alimentation.

Le profil `esp32watchdog` compile le test de blocage UART `!`; le profil normal
ne contient pas cette commande. Le watchdog matériel est initialisé à 3 secondes.

## Compilation et tests

Depuis la racine : `bash chips/build.sh`, puis
`node --test chips/tests/ir-break-beam.test.cjs chips/tests/piezo-sensor.test.cjs firmware/scripts/check-shot.test.cjs`.
Depuis `firmware/` : `pio test -e native`, `pio run -e esp32dev`, ou
`pio run -e esp32net -e esp32watchdog`. Pour les scénarios, un jeton Wokwi CI est
requis : `node scripts/run-impact-scenarios.cjs`. Les scénarios d'impact exigent
un lancement frais par tir.

La validation locale de queue et de logique ne remplace pas une publication
testée contre le broker de l'équipe. Wokwi CI et TLS de bout en bout n'ont pas
été validés dans cet environnement : le jeton Wokwi et le CA/compte broker
n'étaient pas configurés.
