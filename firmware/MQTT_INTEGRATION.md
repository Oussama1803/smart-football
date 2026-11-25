# Contrat d'intégration MQTT — ESP32 du but

Ce document décrit le comportement présent dans `firmware/src/network.cpp`,
`firmware/src/main.cpp`, `firmware/lib/shot_logic/` et
`firmware/lib/delivery/`. Il sert de contrat de travail pour les équipes cloud,
backend et sécurité. Le firmware a été compilé et testé localement, mais **la
publication TLS vers le broker de staging/production n'a pas été vérifiée de
bout en bout**. Ne pas traiter les détails ci-dessous comme preuve de connectivité
réelle.

## Profils et paramètres

`esp32dev` est le profil local hors ligne (`SF_NETWORK_ENABLED=0`). Il imprime
les tirs sur le port série et ne démarre pas le client MQTT. `esp32net` active
ESP-MQTT avec `SF_NETWORK_ENABLED=1`. Ce profil ne doit être utilisé contre un
broker qu'après provisionnement local de `firmware/include/secrets.h`.

Les valeurs sont fournies par ces macros :

| Macro | Exemple non sensible | Utilisation |
|---|---|---|
| `SF_WIFI_SSID` | `Wokwi-GUEST` en simulation | SSID en mode station |
| `SF_WIFI_PASSWORD` | chaîne vide pour Wokwi-GUEST | Mot de passe Wi-Fi; à provisionner sur matériel |
| `SF_MQTT_HOST` | `mqtt.example.org` | Nom DNS couvert par le certificat serveur |
| `SF_MQTT_PORT` | `8883` | Port MQTT sur TLS |
| `SF_MQTT_USERNAME` | `device-placeholder` | Identifiant MQTT de l'appareil |
| `SF_MQTT_PASSWORD` | chaîne vide dans l'exemple | Secret MQTT à provisionner hors Git |
| `SF_NTP_SERVER` | `pool.ntp.org` | Serveur utilisé par SNTP |
| `SF_MQTT_CA_PEM` | vide dans l'exemple | Certificat CA de confiance du serveur, au format PEM |

`secrets.h.example` contient uniquement des valeurs de démonstration; son CA
est vide et la configuration échoue donc fermée. Copier ce fichier en
`secrets.h` local, remplacer toutes les valeurs avec celles fournies par le
responsable broker, puis ne jamais ajouter `secrets.h` à Git. `.gitignore`
l'exclut explicitement.

Exemple de forme, non utilisable pour une connexion :

```c
#define SF_WIFI_SSID "Wokwi-GUEST"
#define SF_WIFI_PASSWORD ""
#define SF_MQTT_HOST "mqtt.example.org"
#define SF_MQTT_PORT 8883
#define SF_MQTT_USERNAME "device-placeholder"
#define SF_MQTT_PASSWORD ""
#define SF_NTP_SERVER "pool.ntp.org"
static const char SF_MQTT_CA_PEM[] = R"PEM()PEM";
```

Le Wi-Fi est configuré en station avec reconnexion automatique; la tâche
réessaie `WiFi.reconnect()` après 5 secondes sans réseau. La connexion réseau
est séparée de la boucle d'acquisition.

## TLS et temps

Le client utilise ESP-MQTT, transport SSL, le port et le certificat racine
`SF_MQTT_CA_PEM`. La vérification du nom commun/SAN reste activée
(`skip_cert_common_name_check = false`). Le code n'installe pas de certificat
client ni de clé privée : l'authentification TLS est celle du serveur, complétée
par le nom d'utilisateur et le mot de passe MQTT. Ne pas désactiver la
validation du certificat ou du nom d'hôte pour les tests.

SNTP est démarré dans la tâche réseau. Seule une callback SNTP avec un epoch
plausible (au moins 2023 et dans la plage uint32) marque l'horloge comme fiable;
le client MQTT n'est créé qu'après cette synchronisation. Le timestamp de tir
est projeté à partir de l'heure monotone de capture et de la base SNTP. Sans
SNTP, le profil hors ligne utilise `millis()/1000`, ce qui explique les dates
`1970-01-01...` observées dans Wokwi. Un tir en attente du profil réseau conserve
son `event_id` et son instant de capture jusqu'à ce qu'une heure fiable soit
disponible.

## Topics et schémas JSON

Pour `DEVICE_ID = "goal-esp32-01"`, les topics sont :

| Topic | Usage |
|---|---|
| `smartfootball/goal-esp32-01/events` | Événements de tir |
| `smartfootball/goal-esp32-01/status` | Présence, erreurs et Last Will |

`<device_id>` est construit à partir de `DEVICE_ID`. Le payload shot est
produit par `build_shot_json()` :

| Champ | Type JSON | Unité / valeurs |
|---|---|---|
| `event_id` | string | UUID v4 généré par l'ESP32, stable pendant les retries de la même entrée |
| `device_id` | string | Identifiant configuré, actuellement `goal-esp32-01` |
| `session_id` | string | Identifiant configuré, actuellement `session-001` |
| `event_type` | string | Constante `shot` |
| `timestamp` | string | UTC ISO-8601 (`YYYY-MM-DDTHH:MM:SSZ`) |
| `ball_speed_kmh` | number | km/h, formaté à un chiffre après la virgule |
| `pressure_reading` | integer | lecture ADC FSR brute 0–4095, lue lors du second front IR d'un tir valide |
| `target_zone` | string | `upper_left`, `upper_right`, `lower_left`, `lower_right` ou `none` |
| `impact_intensity` | integer | pic ADC piézo brut 0–4095; 0 pour `none` |
| `firmware_version` | string | version embarquée, actuellement `0.1.0` |

Les lectures ADC ne sont pas des unités physiques calibrées. La zone est
sélectionnée par le plus haut pic ADC au-dessus du seuil 300; en cas d'égalité,
le premier index gagne.

Messages du topic status :

- Erreurs firmware : `device_id`, `timestamp`, `level` (`error` ou `warning`)
  et `message`.
- Présence après connexion et heartbeat : `device_id`, `timestamp`, `status`
  (`online` ou `heartbeat`), `firmware_version` et `uptime_seconds`.
- Last Will : `device_id` et `status: "offline"`. Le Will n'a pas de timestamp,
  car il est préparé avant que l'heure SNTP soit connue.

Le topic status a donc plusieurs formes de payload; le backend doit distinguer
les messages par les champs présents et par `status` ou `level`.

## QoS, files et gestion des doublons

| Message | QoS | Retained | Confirmation |
|---|---:|---:|---|
| Tirs | 1 | non | Entrée retirée après l'événement PUBACK correspondant |
| Présence `online` / heartbeat | 1 | oui | État courant conservé par le broker |
| Last Will `offline` | 1 | oui | Publié par le broker après une déconnexion inattendue |
| Status/error | 1 | non | Mis en file dans une petite file d'état RAM |

La file de tirs conserve au plus 20 messages en RAM. Elle n'est pas persistante
et une remise à zéro de l'ESP32 perd les entrées. À la reconnexion ou après un
PUBACK expiré (15 secondes), les entrées non acquittées sont réessayées avec le
même JSON et le même `event_id`. QoS 1 est une livraison « au moins une fois » :
un doublon reste possible, notamment si l'ACK est perdu après réception broker.
Le backend doit dédupliquer les tirs avec `event_id`; le firmware ne promet pas
une livraison exactement une fois.

La boucle réseau surveille les notifications MQTT, tente une nouvelle
connexion après 1 seconde, reconstruit le client si un ACK expire ou si un
événement MQTT est supprimé, et limite l'enqueue des messages non-shot selon la
taille de l'outbox. Une file de tirs pleine rejette le nouvel envoi; le firmware
émet alors un JSON d'état d'erreur sur le port série. La file de status a une
capacité de quatre éléments et peut également déborder. La file de tirs est
testée en C++ natif; la connexion, les ACK et le Last Will ne sont pas simulés
par ces tests.

## Procédure d'intégration Mosquitto (staging)

Cette procédure est à exécuter quand un broker, son CA et un compte appareil de
staging auront été fournis. Elle n'a pas encore été exécutée contre le broker
de l'équipe.

1. Vérifier avec l'équipe sécurité que le listener TLS est disponible sur le
   port 8883, que le certificat correspond à `SF_MQTT_HOST`, que son CA est
   disponible et que le compte ne peut publier que sur les topics de son
   appareil.
2. Installer les outils Mosquitto. Éviter de passer un vrai mot de passe en
   argument de commande ou dans l'historique shell. Créer un fichier d'options
   local hors du dépôt, avec des ACL restreintes. Exemple de contenu à compléter
   localement (`--pw` représente ici le secret provisionné, jamais à committer) :

   ```text
   --host mqtt.example.org
   --port 8883
   --cafile /chemin/hors-depot/broker-ca.pem
   --username device-placeholder
   --pw MOT_DE_PASSE_LOCAL
   ```

   Le client charge son fichier d'options par défaut. Les versions Mosquitto
   récentes proposent aussi `-o <fichier-options>`; vérifier `mosquitto_sub
   --help` pour la version installée. Un fichier d'options peut contenir le
   mot de passe : ne pas le placer dans Git, le protéger par ACL et le supprimer
   après l'essai. Cette méthode évite le mot de passe dans la ligne de commande.
3. D?marrer un abonnement de staging en lecture seule pour le but :

   ```sh
   mosquitto_sub -t 'smartfootball/goal-esp32-01/#' -v
   ```

4. Provisionner localement `firmware/include/secrets.h`, compiler et flasher
   `esp32net`, puis observer `status=online` et le heartbeat toutes les
   30 secondes.
5. Déclencher un tir accepté et vérifier le topic events, le schéma et le
   timestamp UTC; demander au backend de confirmer l'idempotence sur
   `event_id`.
6. Tester avec l'équipe les coupures Wi-Fi/broker, la file de 20 tirs, la
   reconnexion, les PUBACK, la répétition éventuelle d'un même ID, le Last Will
   retenu et le retour à `online`. Répéter avec un CA invalide et un nom d'hôte
   incorrect pour confirmer que TLS échoue fermé.

La passerelle Wokwi peut limiter l'accès réseau sortant. Une simulation
interactive sans broker ne valide pas ce parcours; utiliser le matériel ou une
passerelle Wokwi autorisée par l'organisation.

## Extension gardien MPU6050

`firmware-keeper/` est un projet ESP32 séparé qui lit le MPU6050 et affiche les
six axes sur le port série. Il n'a actuellement ni topic MQTT, ni schéma
d'événement backend, ni persistance. Le parseur backend de cette branche
consomme des tirs seulement; les équipes doivent convenir du contrat gardien
avant d'ajouter une publication. Voir [`../firmware-keeper/README.md`](../firmware-keeper/README.md).

## Dépendances et preuves

- Profil de production : PlatformIO, `espressif32@6.9.0`, framework Arduino
  ESP32 2.0.17 et ESP-MQTT fourni par l'ESP-IDF correspondant.
- Les profils `esp32dev`, `esp32net` et `esp32watchdog` sont des compilations;
  ils ne prouvent pas l'authentification, la validation d'un CA réel, la
  disponibilité SNTP, la livraison au broker, les PUBACK ou le Last Will.
- Les tests natifs couvrent logique de tir, JSON, file et calcul de temps. Les
  tests Wokwi VS Code couvrent les scénarios manuels du banc, pas MQTT/TLS.
- Wokwi CI n'est pas validé dans cette contribution; ne pas confondre cette
  limite d'authentification CI avec la validation interactive VS Code.
