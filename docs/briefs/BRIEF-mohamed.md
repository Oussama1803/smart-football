# Brief Mohamed : sécurité, qualité, DevOps (étapes 8a et 9)

Tu travailles en binôme avec Oussama. Tu portes la sécurité du transport MQTT et la chaîne d'intégration continue.

## Étape 8a : TLS, comptes MQTT, ACL

- TLS pour MQTT (Mosquitto) et HTTPS (Nginx).
- Un compte MQTT par appareil, avec une ACL par topic : un appareil ne publie que sur `smartfootball/<son_device_id>/events` et `.../status`. Le backend a son propre compte, en lecture.
- Secrets dans des variables d'environnement et dans les GitHub Secrets. Rien dans le dépôt : vérifie `.gitignore` (`.env`, `secrets.h`, clés et certificats).
- Travaille sur la configuration de Mosquitto qu'Alae met dans `infra/`. Mets-toi d'accord avec lui sur qui modifie quoi.

Tests de sécurité du cahier des charges, à faire et à documenter avec captures dans `docs/` :
- publication MQTT sans identifiants : refusée ;
- publication sur un topic non autorisé : bloquée par l'ACL ;
- tableau de bord sans authentification : accès refusé (avec Oussama) ;
- seuls les ports nécessaires sont ouverts (avec Oussama) ;
- aucun mot de passe ni clé dans le dépôt Git ;
- Web et MQTT chiffrés.

SEC1 : `mosquitto_pub` sans identifiants ou sur un topic interdit est refusé et rien n'arrive en base.

Fini quand : publication sans identifiants refusée, topic interdit bloqué.

## Étape 9 (extension) : GitHub Actions avec Wokwi CI

- Un workflow qui lance les tests unitaires du firmware (`pio test -e native`), les tests `pytest` du backend et le scénario Wokwi `firmware/scenarios/shot-72kmh.test.yaml`.
- Wokwi CI demande un jeton (`WOKWI_CLI_TOKEN`) à mettre dans les GitHub Secrets.
- Les scénarios existent déjà, mais personne ne les a encore lancés. Lance-les à la main avec Saad avant de les mettre en CI.

Fini quand : un push qui casse le calcul de vitesse fait échouer le workflow.

## Les risques retenus dans nos fiches

| Repère | Risque | Réponse |
|---|---|---|
| M1 | Fausser la vitesse en masquant une barrière | SEC2 (rejet hors de 5 à 150 km/h) |
| M2 | Lire les identifiants en mémoire par USB | Aucun secret dans le dépôt |
| M3 | Publier de faux tirs sur le broker | SEC1 |
| M4 | Saturer le broker ou brouiller le Wi-Fi | SUR1 |
| M5 | Pousser un firmware modifié par OTA | Non traité, à signaler dans le rapport |
| D1 à D5 | Désalignement, vibration, bogue, coupure Wi-Fi, coupure d'alimentation | SEC2, seuil, SUR2, SUR1, heartbeat et Last Will |

Le risque M5 n'est pas traité : on ne le cache pas, on le dit dans le rapport.

## Avec qui

Alae pour la configuration du broker et le déploiement, Saad pour les identifiants côté ESP32 et les scénarios Wokwi, Oussama pour le pare-feu, l'authentification Web, les rôles et les sauvegardes.
