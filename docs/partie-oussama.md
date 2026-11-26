# Partie d'Oussama : supervision (étape 10) et sécurité du serveur et des données (étape 8b)

Oussama est chef de projet. Sa partie technique couvre la supervision de la plateforme et la sécurité côté serveur et données, en lien avec Mohamed (sécurité des communications, étape 8a). Tout a été testé en local, sur la pile Docker du projet (décision du 26 novembre : pas de VPS, voir `docs/demo.md`).

## 1. Supervision (étape 10)

Prometheus interroge toutes les 15 secondes des collecteurs dans le réseau Docker, et Grafana affiche le tableau « Smart Football : supervision ».

| Indicateur du cahier | Source | Testé |
|---|---|---|
| CPU, RAM | node-exporter | oui |
| Espace disque | node-exporter | oui (corrigé pour Docker Desktop) |
| Disponibilité du serveur et de l'API | blackbox : `/health` de l'API (contenu vérifié), Nginx en HTTPS | oui, API coupée : « EN PANNE » et alerte `ApiDown` |
| État du broker | blackbox (port TCP), mosquitto-exporter (`$SYS`, compte `monitor`) | oui |
| Messages reçus | mosquitto-exporter | oui |
| Erreurs de traitement | `/metrics` du backend (Alae) | oui, messages non JSON comptés |
| Déconnexions des appareils | tableau Appareils (PostgreSQL), `/metrics` (Last Will) | en ligne et hors ligne : oui. Last Will : attend le vrai ESP32 |

Alertes : `ApiDown`, `BrokerDown`, `NoDeviceTraffic`, `DiskAlmostFull`, visibles dans Prometheus et dans le tableau Alertes de Grafana.

Critère de fin de l'étape 10 (l'arrêt de la simulation se voit dans Grafana) : un appareil publie son heartbeat en TLS avec son compte. Il passe « en ligne » et « Messages MQTT reçus » passe au vert. Après l'arrêt, les deux passent au rouge et `NoDeviceTraffic` se déclenche.

Grafana et Prometheus n'écoutent que sur `127.0.0.1`. Docker contourne le pare-feu sur les ports qu'il publie : un port publié sur toutes les interfaces serait ouvert, même si UFW le bloque.

Fichiers : `infra/compose.monitoring.yaml`, `infra/monitoring/`, `docs/monitoring.md`.

## 2. Sécurité du serveur et des données (étape 8b)

| Mesure | Ce qui a été fait | Preuve |
|---|---|---|
| Moindre privilège en base | `infra/security/db-roles.sql` : `sf_owner` (propriétaire, sans connexion), `sf_app` (lecture et ajout), `sf_backup` (lecture seule) | Dans le conteneur : `sf_backup` lit, et `DELETE FROM shots` donne `permission denied` |
| Sauvegardes | `infra/security/backup-postgres.sh` : dump compressé, 14 jours de rétention | Sauvegarde sur le vrai conteneur. Restauration dans une base vide testée. Le test a trouvé un bug, corrigé : une sauvegarde ratée laissait un fichier vide qui pouvait écraser la bonne. |
| Droits MQTT | Compte `monitor` ajouté à l'ACL de Mohamed : lecture de `$SYS/#` seulement | `monitor` voit les statistiques. Pendant qu'un message de l'ESP32 passe (le backend le reçoit), `monitor` ne reçoit rien. |
| Ports ouverts (T4, local) | Vérifiés avant et après le 8a | Avant : 1883 ouvert sans mot de passe. Après : seulement 80, 443 et 8883. Base, API, Grafana et Prometheus sur `127.0.0.1`. |
| Aucun secret dans Git (T5) | Recherche dans les 31 commits de toutes les branches | Aucun mot de passe ni clé. `.env`, certificats, `passwords` et sauvegardes ignorés. |
| Mots de passe jamais affichés | Lus depuis `.env` dans des variables PowerShell, générés au hasard (lettres et chiffres) | `.env` ignoré par Git |
| Rôles de l'application | Conçus dans `docs/rbac.md` : admin, coach, joueur, tableau des droits, exemple FastAPI testé | **Pas codés dans l'API** (Alae). T3 et T7 ne passent pas. |
| Pare-feu UFW | `infra/security/ufw-setup.sh` : 22, 80, 443 et 8883 seulement | **Pas appliqué** : il n'y a pas de VPS |

État des tests de sécurité : voir `docs/security-tests.md`. T1, T2 et T6 ont été faits par Mohamed. T4, T5 et T8 ont été faits par Oussama, en local. T3 et T7 attendent la connexion à l'API.

## 3. Ce qui n'est pas fait, et pourquoi

- **Connexion et rôles dans l'API** : c'est la faille principale restante. `POST /api/shots` est ouvert : on peut injecter un tir par HTTP sans passer par MQTT. C'est le code d'Alae, et le modèle est prêt.
- **Validation des statuts** : le backend accepte n'importe quel texte sur `.../status`. C'est signalé à Alae, avec une proposition (JSON, liste fixe de statuts).
- **VPS et UFW** : abandonnés pour la démo. La pile est déployable telle quelle (`docs/deployment-vps.md`, `infra/security/ufw-setup.sh`).
- **Risque M5** (firmware modifié par OTA) : non traité.
- **Compteurs du backend** : un compteur n'existe qu'après sa première valeur, donc Prometheus ne voit pas la toute première erreur. Signalé à Alae, la correction tient en une ligne par compteur.

## 4. Problèmes rencontrés

- `rslave` refusé par Docker Desktop : le montage du node-exporter a été simplifié.
- Case Disque vide sous Docker Desktop : le disque principal est d'un type que node-exporter ignore. La requête prend maintenant le vrai disque le plus rempli.
- Après le 8a : le site passe en HTTPS (la sonde a été adaptée), et le collecteur Mosquitto est refusé en anonyme (d'où le compte `monitor`).
- Un mauvais nom de compte dans `.env` donnait « not authorised ». Le collecteur affichait une erreur trompeuse (« identifier rejected »), parce qu'il réessayait en MQTT 3.1. Trouvé en lisant les logs de Mosquitto.
