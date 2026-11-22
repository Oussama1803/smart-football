# Brief Alae : cloud et données (étapes 5, 6, 7)

Tu portes tout ce qui reçoit, stocke et affiche les tirs : Mosquitto, FastAPI, PostgreSQL, le tableau de bord et le déploiement.

## Ce que tu reçois

Le format du message MQTT et les topics sont fixés (voir le README à la racine). Tu n'as pas besoin d'attendre le firmware pour commencer : tu peux publier de faux tirs avec `mosquitto_pub` en respectant le format.

## Étape 5 : Mosquitto, FastAPI, PostgreSQL en Docker Compose

- Un fichier `infra/compose.yaml`, base PostgreSQL sur un volume persistant.
- FastAPI s'abonne à `smartfootball/+/events` et à `smartfootball/+/status`.
- Valide chaque message avant d'écrire en base : champs obligatoires, types, vitesse dans la plage 5 à 150 km/h, `target_zone` parmi `upper_left`, `upper_right`, `lower_left`, `lower_right`, `none`.
- Anti-doublon : `event_id` unique en base. Un même tir renvoyé deux fois (après une coupure réseau) ne doit donner qu'une ligne.
- Une API qui ressort les tirs, avec des tests `pytest` dans `backend/tests/`.
- Mots de passe en variables d'environnement (`.env.example` existe déjà), jamais dans le dépôt.
- Au départ Mosquitto peut tourner sans TLS en local. Mohamed ajoute le TLS, les comptes et les ACL à l'étape 8a : garde la configuration dans `infra/` pour qu'il puisse la modifier.

Fini quand : un faux tir publié arrive en base et ressort par l'API.

## Étape 6 : tableau de bord

Fonctionnalités du cahier des charges : nombre total de tirs, vitesse moyenne et maximale, répartition par zone, taux de précision, historique des séances, pression relative, intensité relative des impacts, données du gardien si le module existe, comparaison entre séances.

Points importants :
- `pressure_reading` et `impact_intensity` sont des valeurs ADC brutes. Affiche-les comme une pression et une intensité relatives, jamais en Newton ni comme une « puissance de tir ».
- Le tableau de bord doit distinguer les mesures réelles, les estimations et les données de démonstration.
- Un tir sans impact arrive avec `target_zone = "none"`. Pour le taux de précision, ce sont les tirs qui ne touchent aucune zone. Ce choix est à confirmer avec Oussama.
- Choix React ou HTML/CSS/JavaScript : à décider avec l'équipe avant de commencer.
- L'extension à 9 zones doit rester possible sans changer le format du message.

Fini quand : les tirs en base s'affichent.

## Étape 7 : déploiement sur le VPS

- VPS Ubuntu, Docker Compose, Nginx pour HTTPS, avec Oussama et Mohamed (pare-feu UFW et certificats).
- Le broker doit être joignable depuis Internet : l'ESP32 simulé dans Wokwi sort par la passerelle publique, qui n'atteint pas une machine locale. La passerelle privée est payante, donc le VPS est indispensable.
- À vérifier tôt : l'ESP32 simulé peut-il joindre le broker sur le port choisi (1883 puis 8883 avec TLS) ? La documentation Wokwi ne le confirme pas. Dis-le à Saad dès que le broker répond.

Fini quand : un tir simulé apparaît en ligne.

## Exigences concernées

ENF1 : écart entre l'heure du tir et l'heure de réception inférieur à 2 s. SUR1 : broker arrêté, 10 tirs, broker relancé, 10 tirs en base sans doublon.

## Avec qui

Saad pour le format et le réseau, Mohamed pour la sécurité du broker et de l'API, Oussama pour le pare-feu, l'authentification Web, les rôles et les sauvegardes.
