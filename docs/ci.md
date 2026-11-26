# Intégration continue (étape 9)

Responsable : Mohamed. Fichier : `.github/workflows/ci.yml`. Issue : #10.

## Ce qui est lancé

À chaque push (toutes les branches) et à chaque Pull Request vers `main`, GitHub Actions lance 3 jobs :

| Job | Commande | Ce qu'il vérifie |
|---|---|---|
| Backend (pytest) | `pytest -q` dans `backend/` | 55 tests de l'API, de la validation, de la déduplication et du MQTT (SQLite en mémoire) |
| Firmware (pio test -e native) | `pio test -e native` dans `firmware/` | 34 tests de la logique : calcul de vitesse, zones, anti-rebond, JSON, file d'envoi |
| Wokwi (shot at 72 km/h) | `pio run -e esp32dev` puis `wokwi/wokwi-ci-action` | compile le vrai firmware ESP32 et simule un tir à 72 km/h (`firmware/scenarios/shot-72kmh.test.yaml`) |

Le job Wokwi ne démarre que si les tests du firmware passent : inutile de simuler un firmware déjà faux.

Le workflow n'a que le droit de lire le code (`permissions: contents: read`).

## Jeton Wokwi

Le job Wokwi a besoin du secret `WOKWI_CLI_TOKEN` (GitHub : Settings > Secrets and variables > Actions). Le jeton se crée sur https://wokwi.com/dashboard/ci. Il ne doit jamais apparaître dans le code, une capture ou un message ; GitHub le masque dans les logs.

Si le secret manque, le job Wokwi échoue avec un message explicite au lieu de passer en vert sans rien tester.

Le plan gratuit de Wokwi a un quota de minutes de CI : ne pas relancer le workflow sans raison.

## Preuve (critère de l'issue #10)

| Run | Commit | Résultat | Capture |
|---|---|---|---|
| Sans jeton Wokwi | `8f413b7` | Backend ✅, Firmware ✅, Wokwi ❌ (jeton manquant) | `captures/ci-1-sans-jeton-wokwi.png` |
| Avec jeton | `8f413b7` | Backend ✅, Firmware ✅, Wokwi ✅ | `captures/ci-2-tout-vert.png` |
| Calcul de vitesse cassé (`3.6f` → `3.0f`) | `39c142b` | Firmware ❌, Wokwi sauté, Backend ✅ | `captures/ci-3-calcul-casse-rouge.png` |

Un push qui casse le calcul de vitesse fait bien échouer le workflow. L'erreur volontaire a été annulée par `git revert` (commit `aedbbfe`).

Ces runs sont aussi la première exécution de `pio test -e native` et du scénario Wokwi `shot-72kmh` par l'équipe.

## Lancer les mêmes tests en local (Docker)

```
docker run --rm -v "${PWD}:/repo" -w /repo/backend python:3.11-slim sh -c "pip install -q -r requirements.txt && pytest -q"
docker run --rm -v "${PWD}:/repo" -w /repo/firmware python:3.11-slim sh -c "apt-get update -qq && apt-get install -y -qq g++ && pip install -q platformio && pio test -e native"
```

Monter tout le dépôt, pas seulement `backend/` : un test vérifie que le dossier `frontend/` est servi.

## Limites

- Seul le scénario `shot-72kmh` est en CI. Les autres scénarios de `firmware/scenarios/` (rejets, zones, timeout) peuvent être ajoutés en suivant le même modèle, en tenant compte du quota Wokwi.
- Les tests des custom chips (`chips/tests/*.cjs`) et le firmware du gardien (`firmware-keeper/`) ne sont pas encore en CI.
- La CI ne teste pas la liaison MQTT/TLS vers le broker : elle se teste sur le VPS.
