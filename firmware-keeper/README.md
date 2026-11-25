# Acquisition du gardien (étape 11)

Ce projet lit le MPU6050 à l'adresse I²C `0x68` sur SDA GPIO21 / SCL GPIO22.
L'identifiant est vérifié, le capteur est réveillé et configuré en plages ±2 g
et ±250 °/s. Les six axes sont lus en rafale à 10 Hz ; les lectures I²C expirent
après 10 ms et une panne provoque une nouvelle initialisation. Un watchdog de
3 secondes surveille la boucle. Les conversions brutes sont couvertes par les
tests natifs.

Compiler depuis ce dossier avec `pio run -e esp32dev`, puis tester avec
`pio test -e native`. Pour Wokwi, démarrer le projet et lancer
`wokwi-cli . --scenario scenarios/motion.test.yaml`.

L'intégration de publication en base reste dépendante d'un contrat backend pour
les événements du gardien. Le parseur MQTT existant n'accepte que les tirs.
Cette étape n'invente donc ni topic, ni charge utile, ni changement au protocole.
