# Brief Saad : embarqué (étapes 2, 3, 4, 11)

## État vérifié (24 novembre 2026)

Les barrières IR et les quatre piézos ont des contextes séparés par instance.
Leurs WASM réels passent les tests déterministes; le calcul de vitesse, les
zones, le JSON et la file de 20 tirs passent les tests PlatformIO natifs.
Les profils ESP32 hors ligne, réseau/TLS et watchdog compilent. Le projet keeper
MPU6050 lit et convertit les six axes, avec tests natifs et compilation réussis.

La publication sur le broker n'est pas encore validée : il faut fournir un CA,
un compte appareil et l'hôte. Les six scénarios Wokwi d'intégration restent à
lancer après configuration du jeton CLI. L'extension keeper ne publie pas vers
la base : le schéma backend distant accepte actuellement `event_type=shot` et
aucun contrat de mouvement gardien n'a été convenu.

Tu portes tout ce qui tourne sur l'ESP32 : le circuit Wokwi, le firmware, le Wi-Fi et le MQTT.

## Ce qui est déjà fait

Les étapes 2 et 3 ont été écrites avec l'aide de Claude. Rien n'a été lancé dans Wokwi, donc tu les prends comme un brouillon à vérifier, pas comme un résultat. Ce qui a été vérifié : 28 tests unitaires de la logique pure (vitesse, plage, zone, suivi des deux barrières, format du JSON), passés avec g++ et Unity. Ce qui ne l'a pas été : `pio test -e native`, `pio run -e esp32dev`, la simulation Wokwi et les scénarios CI.

À toi de relire ce code et de pouvoir l'expliquer à la prof.

## Par où commencer

1. Installe VS Code avec les extensions PlatformIO IDE et Wokwi for VS Code, et active la licence Wokwi (gratuite avec un compte).
2. Dans `firmware/` : `pio test -e native`, puis `pio run -e esp32dev`, puis F1 et « Wokwi: Start Simulator ».
3. Vérifie ces points, qui n'ont pas pu être confirmés :
   - les noms de broches du `diagram.json` (`esp:VP` pour le GPIO 36, `esp:TX`, `esp:RX`) ;
   - l'attribut `role = 2` de la barrière 2 (le maître est `beam1`, l'esclave `beam2`, reliés par `SYNC`) ;
   - l'échelle de l'ADC et le seuil des piézos (`PIEZO_THRESHOLD_ADC = 300` dans `config.h`) ;
   - le buzzer (il utilise l'API LEDC avec `espressif32@6.9.0`) ;
   - les 4 scénarios dans `firmware/scenarios/`.
4. Corrige ce qui casse, un commit par correction.

Comment on tire dans la simulation : régler `ballSpeedKmh` sur `beam1`, passer `shoot` de 0 à 1, puis passer `trigger` du piézo voulu de 0 à 1 dans les 60 ms. Détails dans `firmware/README.md`.

## Ce que tu dois construire ensuite (étape 4)

- Wi-Fi sur le réseau `Wokwi-GUEST` (ouvert, sans mot de passe), heure par NTP, MQTT avec TLS.
- Publication d'un tir sur `smartfootball/<device_id>/events` en QoS 1.
- Heartbeat toutes les 30 s, erreurs et Last Will sur `smartfootball/<device_id>/status`.
- Identifiants Wi-Fi et MQTT dans `firmware/include/secrets.h` (ignoré par git). Versionne `secrets.h.example`.
- SUR1 : garder jusqu'à 20 tirs en mémoire pendant une coupure réseau et les renvoyer en moins de 10 s au retour, sans doublon (le `event_id` sert à ça).
- SUR2 : watchdog, redémarrage en moins de 5 s. La commande de blocage qui sert à le tester ne doit exister que dans une compilation de test (drapeau de compilation). Dans la version normale ce serait une porte dérobée.
- Remplacer le `timestamp` provisoire par l'heure NTP (aujourd'hui c'est le temps de fonctionnement compté depuis 1970).

## Contraintes à respecter

- Piézos et FSR sur l'ADC1 uniquement (GPIO 32 à 36) : l'ADC2 ne marche pas avec le Wi-Fi.
- Éviter les broches de démarrage (0, 2, 5, 12, 15).
- Chaque capteur ne mesure que ce qu'il mesure : le FSR donne une pression relative, les barrières la vitesse, les piézos une zone probable et une intensité relative.
- Réseau Wokwi : la passerelle publique n'autorise que les connexions sortantes. Pour joindre une machine locale il faut la passerelle privée, qui est payante. Le broker doit donc être sur le VPS (voir avec Alae). La documentation Wokwi ne confirme pas que le port 8883 passe : à tester dès que le broker existe.

## Décisions à faire valider par Oussama

- Un tir sans impact dans la fenêtre est publié avec `target_zone = "none"` et `impact_intensity = 0`. Ce n'est pas dans le format du cahier des charges.
- Les LED montrent la zone touchée. La fonction « zone à viser » des fiches de TD n'est pas faite.
- Le FSR est lu au passage de la barrière 2, pas à la frappe exacte.
- Les `.wasm` des chips sont versionnés dans `firmware/chips/` (recompilation avec `chips/build.sh`).

## Fini quand

- Étape 3 : delta t = 10 ms affiche `72.0` km/h sur le port série.
- Étape 4 : le message apparaît avec `mosquitto_sub`.
- Étape 11 (extension) : un second projet `firmware-keeper/` avec le MPU6050, les mouvements du gardien arrivent en base.

## Exigences que tu testes

EF1 (delta t = 9, 10, 11 ms donne 80,0 / 72,0 / 65,5 km/h), EF2 (zone 3 touchée, seule la LED 3 s'allume), SEC2 (delta t = 1 ms et 200 ms : rien publié, « vitesse rejetée » sur le topic status), SUR1, SUR2, ENF1 et ENF2 (par calcul).
