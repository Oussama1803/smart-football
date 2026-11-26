# Tests de sécurité (cahier des charges, section 8)

Pour chaque test : lancer la commande, faire une capture d'écran du résultat, la ranger dans `docs/captures/` sous le nom indiqué, noter la date et le résultat dans le tableau.

Remplacer `HOTE` par le nom ou l'IP du VPS, `ca.crt` par le certificat de l'autorité utilisée pour Mosquitto.

| N° | Test | Résultat attendu | Capture | Fait le | Résultat |
|---|---|---|---|---|---|
| T1 | Publication MQTT sans identifiants | Refusée | `t1-mqtt-sans-identifiants.png`, `t1-port-1883-ferme.png` | 2026-11-26 (local) | ✅ `Connection Refused: not authorised` sur 8883 (TLS) ; port 1883 fermé depuis l'hôte. À refaire sur le VPS. |
| T2 | Publication sur un topic non autorisé | Bloquée par l'ACL, rien en base | `t2-acl-topic-interdit.png`, `t2-acl-abonne-backend.png` | 2026-11-26 (local) | ✅ `goal-esp32-01` sur `smartfootball/autre-appareil/events` : `PUBACK RC:135 Not authorized` (MQTT v5), rien reçu par l'abonné `backend`. Sur son propre topic : `RC:0`. À refaire sur le VPS. |
| T3 | Tableau de bord sans authentification | Accès refusé | `t3-dashboard-sans-auth.png` | | |
| T4 | Seuls les ports nécessaires sont ouverts | 22, 80, 443, 8883 seulement | `t4-ports-ouverts.png` | | |
| T5 | Aucun mot de passe ni clé dans Git | Aucun résultat | `t5-secrets-git.png` | 2026-11-26 | ✅ Aucune clé privée PEM dans tout l'historique, aucun fichier sensible suivi par Git, `.env`, `passwords` et clés ignorés. |
| T6 | Web et MQTT chiffrés | TLS sur 443 et 8883, rien en clair | `t6-chiffrement-mqtt.png`, `t6-chiffrement-web.png` | 2026-11-26 (local) | ✅ MQTT 8883 : TLS 1.3, certificat vérifié. Web : HTTP 80 redirige (301) vers HTTPS 443, certificat vérifié. Sans le CA : connexion refusée. À refaire sur le VPS avec le vrai certificat. |
| T7 | Rôles (RBAC) | Un joueur ne fait pas ce que fait un administrateur | `t7-rbac.png` | | |
| T8 | Sauvegarde PostgreSQL | Dump non vide, restauration possible | `t8-sauvegarde.png` | | |

### Test complémentaire : tir légitime de bout en bout (2026-11-26, local)

Un tir au format du cahier des charges, publié en TLS sur 8883 avec le compte `goal-esp32-01` sur son propre topic, est accepté (`PUBACK RC:0`), reçu par l'abonné `backend` et enregistré en base (même `event_id`). La sécurité bloque ce qu'il faut sans bloquer les vrais tirs. Captures : `tir-legitime-1-abonne.png`, `tir-legitime-2-publication-tls.png`, `tir-legitime-3-en-base.png`.

Le `latency_ms` affiché dans ce test n'est pas une mesure de l'exigence ENF1 : le `timestamp` du message a été fixé à la préparation du fichier, avant l'envoi. ENF1 se mesure avec le firmware de l'ESP32.

Captures « avant » la sécurisation (publication anonyme acceptée sur 1883) : `avant-8a-publication-anonyme-pub.png`, `avant-8a-publication-anonyme-sub.png`.

### Comment les tests locaux ont été lancés

Les tests MQTT locaux utilisent un client `mosquitto_pub` / `mosquitto_sub` dans un conteneur sur le réseau Docker du projet (`infra_smartfootball_net`), connecté à `mosquitto:8883` en TLS avec `ca.crt`. Les mots de passe sont lus depuis `.env` dans des variables PowerShell, jamais tapés ni affichés :

```
$de = (Select-String '^MQTT_DEVICE_PASSWORD=' .env).Line.Split('=',2)[1].Trim()
docker run --rm --network infra_smartfootball_net -v "${PWD}\infra\mosquitto\certs\ca.crt:/ca.crt:ro" eclipse-mosquitto:2 mosquitto_pub -h mosquitto -p 8883 --cafile /ca.crt -V mqttv5 -q 1 -d -u goal-esp32-01 -P $de -t smartfootball/autre-appareil/events -m "faux tir"
```

## T1 : sans identifiants (SEC1)

```
mosquitto_pub -h HOTE -p 8883 --cafile ca.crt -t smartfootball/goal-esp32-01/events -m '{"test":1}'
```

Attendu : « Connection Refused: not authorised ». Vérifier aussi que rien n'est arrivé en base.

## T2 : topic interdit

Avec les identifiants valides de `goal-esp32-01`, publier sur le topic d'un autre appareil, en MQTT v5 et QoS 1 pour que le client affiche la réponse du broker :

```
mosquitto_pub -h HOTE -p 8883 --cafile ca.crt -V mqttv5 -q 1 -d -u goal-esp32-01 -P '<mot de passe>' -t smartfootball/autre-appareil/events -m '{"test":1}'
```

Attendu : `CONNACK (0)` (le compte est accepté), puis `Publish 1 failed: Not authorized` et `PUBACK (... RC:135)` (l'ACL bloque).

Avec MQTT 3.1.1 (sans `-V mqttv5`), le client ne voit pas d'erreur même quand l'ACL bloque la publication. Dans tous les cas, ouvrir un abonné en parallèle avec le compte du backend (`mosquitto_sub -u backend -t 'smartfootball/+/events' -t 'smartfootball/+/status' -v`) et vérifier que rien n'arrive, et que la base n'a pas de nouvelle ligne.

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
git --no-pager log --all --oneline -G "-----BEGIN [A-Z ]*PRIVATE KEY-----"
git ls-files | Select-String -Pattern "\.env$|secrets\.h$|passwords|\.key$|\.pem$|\.crt$"
git check-ignore -v .env infra/mosquitto/passwords infra/mosquitto/certs/ca.key infra/mosquitto/certs/server.key
```

Attendu : `git grep` ne trouve que des lectures de variables ou des valeurs d'exemple (`change-me`), les deux commandes suivantes ne trouvent rien, et `check-ignore` affiche une règle du `.gitignore` pour chaque fichier secret. `.env.example` et `secrets.h.example` ne contiennent que des valeurs d'exemple. Si une vraie valeur est déjà passée dans l'historique, la changer, pas seulement la supprimer.

Ne pas utiliser `git log -S"BEGIN PRIVATE KEY"` : ce fichier contient ce texte, la commande le trouve et donne une fausse alerte.

## T6 : chiffrement

```
openssl s_client -connect HOTE:8883 -CAfile ca.crt -verify_hostname HOTE -brief </dev/null
curl -vI http://HOTE/
curl -sS --cacert ca.crt https://HOTE/health
```

Attendu : sur 8883, `Verification: OK` et une version TLS 1.2 ou 1.3 ; le HTTP clair sur le port 80 redirige (301) vers HTTPS ; HTTPS répond avec un certificat valide. Sans le bon CA, la vérification échoue. Aucun accès MQTT sur 1883 depuis l'extérieur (voir T4).

Sous Windows, ajouter `--ssl-no-revoke` à `curl.exe` quand le certificat est signé par notre propre CA : le `curl` de Windows tente sinon de vérifier la révocation, impossible avec une CA privée.

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