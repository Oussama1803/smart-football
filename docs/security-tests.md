# Tests de sécurité (cahier des charges, section 8)

Pour chaque test : lancer la commande, faire une capture d'écran du résultat, la ranger dans `docs/captures/` sous le nom indiqué, noter la date et le résultat dans le tableau.

Remplacer `HOTE` par le nom ou l'IP du VPS, `ca.crt` par le certificat de l'autorité utilisée pour Mosquitto.

| N° | Test | Résultat attendu | Capture | Fait le | Résultat |
|---|---|---|---|---|---|
| T1 | Publication MQTT sans identifiants | Refusée | `t1-mqtt-sans-identifiants.png` | | |
| T2 | Publication sur un topic non autorisé | Bloquée par l'ACL, rien en base | `t2-mqtt-topic-interdit.png` | | |
| T3 | Tableau de bord sans authentification | Accès refusé | `t3-dashboard-sans-auth.png` | | |
| T4 | Seuls les ports nécessaires sont ouverts | 22, 80, 443, 8883 seulement | `t4-ports-ouverts.png` | | |
| T5 | Aucun mot de passe ni clé dans Git | Aucun résultat | `t5-secrets-git.png` | | |
| T6 | Web et MQTT chiffrés | TLS sur 443 et 8883, rien en clair | `t6-chiffrement.png` | | |
| T7 | Rôles (RBAC) | Un joueur ne fait pas ce que fait un administrateur | `t7-rbac.png` | | |
| T8 | Sauvegarde PostgreSQL | Dump non vide, restauration possible | `t8-sauvegarde.png` | | |

## T1 : sans identifiants (SEC1)

```
mosquitto_pub -h HOTE -p 8883 --cafile ca.crt -t smartfootball/goal-esp32-01/events -m '{"test":1}'
```

Attendu : « Connection Refused: not authorised ». Vérifier aussi que rien n'est arrivé en base.

## T2 : topic interdit

Avec les identifiants valides de `goal-esp32-01`, publier sur le topic d'un autre appareil :

```
mosquitto_pub -h HOTE -p 8883 --cafile ca.crt -u goal-esp32-01 -P '<mot de passe>' -t smartfootball/autre-appareil/events -m '{"test":1}'
```

Avec MQTT 3.1.1, le client peut ne pas voir d'erreur même quand l'ACL bloque la publication. À confirmer sur notre broker : ne pas se fier au message du client, ouvrir un abonné en parallèle (`mosquitto_sub -t 'smartfootball/#' -v` avec le compte du backend) et vérifier que rien n'arrive, et que la base n'a pas de nouvelle ligne.

## T3 : tableau de bord sans authentification

```
curl -i https://HOTE/
curl -i https://HOTE/api/shots
```

Attendu : redirection vers la page de connexion ou code 401 / 403, aucune donnée de tir dans la réponse.

## T4 : ports ouverts

Depuis une machine qui n'est pas le VPS :

```
nmap -Pn -p- HOTE
```

Attendu : 22, 80, 443 et 8883 seulement. Si 5432, 8000, 9090, 3000 ou 1883 apparaissent, un port Docker est publié sans passer par UFW (voir l'avertissement dans `ufw-setup.sh`). Sur le VPS : `sudo ufw status verbose` et `ss -tlnp`.

## T5 : aucun secret dans Git

```
git grep -nIiE "(password|passwd|secret|api[_-]?key|token)\s*[:=]" -- . ':!*.md' ':!.env.example'
git log -p --all -S"BEGIN PRIVATE KEY" --oneline
git ls-files | findstr /i ".env secrets.h .pem .key"
```

Attendu : pas de vraie valeur. `.env.example` et `secrets.h.example` ne contiennent que des `change-me`. Si une vraie valeur est déjà passée dans l'historique, la changer, pas seulement la supprimer.

## T6 : chiffrement

```
openssl s_client -connect HOTE:8883 -CAfile ca.crt </dev/null
curl -vI http://HOTE/
```

Attendu : certificat valide sur 8883, et le HTTP clair sur le port 80 redirige vers HTTPS. Aucun accès MQTT sur 1883 depuis l'extérieur (voir T4).

## T7 : rôles

Se connecter avec un compte administrateur, un compte entraîneur et un compte joueur. Noter pour chaque rôle ce qui est possible et refusé (voir le tableau des droits ci-dessous), avec une capture de chaque refus.

Droits proposés (à valider par l'équipe) :

| Action | Administrateur | Entraîneur | Joueur |
|---|---|---|---|
| Voir ses propres tirs et séances | oui | oui | oui |
| Voir les tirs de tous les joueurs | oui | oui | non |
| Comparer les séances | oui | oui | ses propres séances |
| Gérer les comptes utilisateurs | oui | non | non |
| Configurer les appareils et le broker | oui | non | non |

## T8 : sauvegarde

```
./backup-postgres.sh
ls -lh backups/
gunzip -c backups/smartfootball_*.sql.gz | head -20
```

Attendu : un fichier non vide qui contient du SQL. Tester une restauration dans une base vide au moins une fois.
