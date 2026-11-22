# Brief Oussama : chef de projet (étapes 1, 8b, 10, 12b)

## Coordination

- Inviter Saad, Alae et Mohamed comme collaborateurs du dépôt privé (Settings, Collaborators), puis leur donner leur brief.
- Fixer les dates des jalons M1 à M5 (les jalons ne sont pas créés sur GitHub, il n'y a que les tâches).
- Assigner chaque tâche à son propriétaire : les titres portent le prénom, mais l'assignation GitHub n'est pas faite.
- Faire relire à Saad les étapes 2 et 3 (écrites avec l'aide de Claude, jamais lancées dans Wokwi).
- Le VPS est indispensable (la passerelle privée de Wokwi est payante) : lancer l'étape 7 tôt, parce qu'elle débloque Saad.

## Décisions à prendre ou à valider

1. Un tir sans impact : `target_zone = "none"` et `impact_intensity = 0` (pour le taux de précision). Ce n'est pas dans le format du cahier des charges.
2. Les LED : zone touchée (fait) ou zone à viser (fiches de TD, pas fait).
3. Le `timestamp` : provisoire jusqu'au NTP de l'étape 4.
4. Tableau de bord : React ou HTML/CSS/JavaScript.
5. Les `.wasm` des chips versionnés dans le dépôt, oui ou non.
6. Le fichier `CLAUDE.md` à la racine (notes pour Claude Code) : le garder visible ou l'ignorer.

## Tes étapes

- **Étape 1** (faite, poussée sur GitHub) : squelette, README, `.gitignore`, `.env.example`.
- **Étape 8b** : pare-feu UFW (seuls les ports nécessaires ouverts), authentification Web, rôles (administrateur, entraîneur, joueur), sauvegardes PostgreSQL, vérifier qu'aucun secret n'est dans Git. Tests documentés avec captures. Fini quand les tests de sécurité du cahier passent. Avec Mohamed.
- **Étape 10** (extension) : Prometheus, Grafana, exporter système. Indicateurs du cahier : CPU et RAM, espace disque, disponibilité du serveur et de l'API, état du broker, messages reçus, erreurs de traitement, déconnexions des appareils. Fini quand l'arrêt de la simulation se voit dans Grafana.
- **Étape 12b** : répéter la démonstration et assembler le rapport.

## La démonstration finale (10 points du cahier)

1. Un tir. 2. La pression FSR. 3. La vitesse par les barrières IR. 4. La zone par les piézos. 5. L'envoi MQTT sécurisé. 6. La validation par le backend. 7. L'enregistrement dans PostgreSQL. 8. L'affichage dans le tableau de bord. 9. Les métriques dans Grafana. 10. Le refus d'un accès MQTT non autorisé.

## Pour le rapport

- Chaque exigence (EF1, EF2, ENF1, ENF2, SEC1, SEC2, SUR1, SUR2) a un résultat noté dans `docs/`, avec une capture quand c'est possible.
- Dire ce qui n'est pas traité : le risque M5 (firmware modifié par OTA).
- Dire ce qui est une valeur brute, une estimation ou une donnée de démonstration.
- Noter les écarts avec le cahier des charges (voir les décisions ci-dessus), même petits.

## Où on en est (au 22 novembre)

Étapes 1 à 3 sur GitHub. Logique du firmware testée (28 tests). Jamais lancés : compilation ESP32, simulation Wokwi, scénarios CI. Étapes 4 à 12 pas commencées.
