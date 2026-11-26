# Acquisition embarquée du gardien (extension MPU6050)

## Objectif et état actuel

Ce projet PlatformIO indépendant lit un MPU6050 et affiche les accélérations et
vitesses angulaires du gardien sur le port série. Il s'agit d'une acquisition
locale : **aucun événement gardien n'est envoyé au broker ou au backend**. Le
contrat MQTT du backend actuel décrit les tirs; il faut une décision d'équipe
sur le topic et le schéma des mouvements avant d'ajouter cette publication.

Cette extension compile séparément du firmware du but. Deux tests natifs
couvrent la conversion des mots signés big-endian et les échelles d'accélération
et de rotation. Ils ne valident ni un capteur physique ni une simulation Wokwi.

## Architecture et câblage

| Élément | Broche / paramètre | Rôle |
|---|---|---|
| ESP32 DevKit | SDA GPIO21, SCL GPIO22 | Bus I²C à 400 kHz |
| MPU6050 | adresse I²C `0x68` (AD0 à GND) | Accéléromètre et gyroscope 6 axes |
| Accéléromètre | plage ±2 g, sensibilité 16384 LSB/g | Mesure brute convertie en `g` |
| Gyroscope | plage ±250 °/s, sensibilité 131 LSB/(°/s) | Rotation convertie en degrés/seconde |

La lecture du registre WHO_AM_I vérifie l'identifiant `0x68`, puis le firmware
sort le capteur de veille et configure ses deux plages. Une rafale de 14 octets
à partir de `ACCEL_XOUT_H` donne accélérations, température (non utilisée) et
rotations.

## Comportement et délais

- I²C est initialisé avec un timeout de 10 ms; un échec de lecture ramène l'état
  à « non initialisé », puis le firmware tente de réinitialiser le capteur.
- Les tentatives d'initialisation sont espacées d'environ 1 seconde.
- Après initialisation, les mesures sont demandées toutes les 100 ms (10 Hz).
- Un watchdog de tâche de 3 secondes surveille la boucle.
- La sortie série `[IMU]` contient `accel_g` et `rotation_dps`; il s'agit de
  mesures d'accélération et de rotation, pas d'une position ni d'un temps de
  réaction direct.

## Organisation

```text
firmware-keeper/
  src/main.cpp                 initialisation I²C, lecture et watchdog
  lib/imu_logic/src/imu_logic.h décodage pur des mots MPU6050
  test/test_imu/test_main.cpp  tests natifs Unity
  diagram.json                ESP32, MPU6050 et câblage I²C Wokwi
  scenarios/motion.test.yaml  stimulus Wokwi CLI
  platformio.ini              profils esp32dev et native
  wokwi.toml                  chemins du firmware de simulation
```

## Compilation et tests

Depuis ce dossier :

```sh
pio test -e native
pio run -e esp32dev
```

Pour une simulation Wokwi VS Code, compiler le profil `esp32dev`, ouvrir
`firmware-keeper/` comme projet, lancer **Wokwi: Start Simulator**, attendre
`[IMU] ready`, puis modifier les contrôles du MPU6050 dans le diagramme. Le
scénario YAML `scenarios/motion.test.yaml` est un test CLI et nécessite Wokwi CI;
il n'est pas présenté comme exécuté ici.

## Limites et dépendances d'intégration

- L'intégration broker/backend attend un contrat gardien convenu par les
  équipes. Le code n'invente pas de topic, de payload ni de table de stockage.
- Le firmware du but et son client MQTT ne consomment actuellement que les
  messages de type shot.
- Les tests actuels portent sur l'algorithme de conversion; aucun capteur
  physique, filtrage de mouvement, détection de plongeon, calibration ou
  publication réseau n'est validé par ce projet.
