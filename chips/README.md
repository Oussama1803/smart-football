# Custom chips Wokwi

Trois capteurs du cahier des charges que Wokwi ne propose pas : `ir-break-beam`, `piezo-sensor`, `fsr-sensor`. Chaque dossier contient `main.c` et le fichier `<nom>.chip.json` (broches et contrôles).

Compiler : `./build.sh` (clang cible wasm32 et wasm-ld). Le script copie les `.wasm` et `.json` dans `../firmware/chips/`, d'où les lit `firmware/wokwi.toml`.

`wokwi-api.h` est l'en-tête officiel de l'API des custom chips, copié sans modification depuis https://github.com/wokwi/inverter-chip (licence MIT, Uri Shaked / Wokwi).

| Chip | Sortie | Contrôles Wokwi |
|---|---|---|
| `ir-break-beam` | numérique : HIGH faisceau intact, LOW faisceau coupé | `ballSpeedKmh`, `shoot` (maître uniquement) |
| `piezo-sensor` | analogique : pic de tension qui décroît (tau 5 ms) | `peakVolts`, `trigger` |
| `fsr-sensor` | analogique : pont diviseur avec 10 kohm vers GND | `pressure` (0 à 100 %) |

Les contrôles Wokwi n'existent qu'en curseur (`range`), pas en bouton : `shoot` et `trigger` sont des curseurs 0 / 1 qu'on passe de 0 à 1 pour déclencher.

Les deux barrières sont deux instances de `ir-break-beam`. `beam1` est le maître (rôle 1), `beam2` l'esclave (attribut `role = 2` dans `diagram.json`). Leurs broches `SYNC` sont reliées : l'impulsion de `SYNC` dure `D / v`, et l'esclave coupe son faisceau sur le front descendant.
