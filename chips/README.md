# Custom chips Wokwi

Trois capteurs du cahier des charges que Wokwi ne propose pas : `ir-break-beam`, `piezo-sensor`, `fsr-sensor`. Chaque dossier contient `main.c` et le fichier `<nom>.chip.json` (broches et contrôles).

Compiler : `./build.sh` (clang cible wasm32 et wasm-ld). Le script copie les `.wasm` et `.json` dans `../firmware/chips/`, d'où les lit `firmware/wokwi.toml`.

`wokwi-api.h` est l'en-tête officiel de l'API des custom chips, copié sans modification depuis https://github.com/wokwi/inverter-chip (licence MIT, Uri Shaked / Wokwi).

| Chip | Sortie | Contrôles Wokwi |
|---|---|---|
| `ir-break-beam` | numérique : HIGH faisceau intact, LOW faisceau coupé | `ballSpeedKmh`, `shoot` (maître uniquement) |
| `piezo-sensor` | analogique : pic de tension qui décroît (tau 5 ms) | `peakVolts`, `trigger` |
| `fsr-sensor` | analogique : pont diviseur avec 10 kohm vers GND | `pressure` (0 à 100 %) |

Le séquenceur `shot-test-sequencer` n'est pas une puce du circuit de production.
Il est propre au projet [firmware/wokwi-test](../firmware/wokwi-test/README.md)
et synthétise IR1, IR2 et les quatre voies analogiques pour les essais manuels
dans Wokwi VS Code. Ses WASM et le FSR local sont compilés par
`firmware/wokwi-test/build-chips.ps1`; ils restent dans ce sous-projet.

Les contrôles Wokwi n'existent qu'en curseur (`range`), pas en bouton : `shoot` et `trigger` sont des curseurs 0 / 1 qu'on passe de 0 à 1 pour déclencher.

Les deux barrières sont deux instances de `ir-break-beam`. `beam1` est le maître (rôle 1), `beam2` l'esclave (attribut `role = 2` dans `diagram.json`). Leurs broches `SYNC` sont reliées : l'impulsion de `SYNC` dure `D / v`, et l'esclave coupe son faisceau sur le front descendant.

Chaque instance IR possède un `chip_state_t`, transmis aux timers et au watcher par
`user_data`. Le module WASM partage un pool de **deux contextes distincts**, adapté
aux deux IR du diagramme. Cette allocation évite d'ajouter une libc au build
freestanding. Une troisième instance provoque volontairement un trap au démarrage,
plutôt que de réutiliser l'état d'un capteur existant. Pour agrandir le circuit,
adapter `MAX_IR_INSTANCES` et les tests.

## Tests des instances IR

Depuis la racine du dépôt, après `bash chips/build.sh` :

```sh
node --test chips/tests/ir-break-beam.test.cjs
```

Ces tests exécutent le fichier `firmware/chips/ir-break-beam.chip.wasm` réellement
référencé par `firmware/wokwi.toml`, avec un hôte API déterministe en mémoire.
Ils couvrent les deux ordres d'initialisation, l'absence d'impulsion au repos,
IR1 puis IR2 séparés de 10 000 µs à 72 km/h, les retours HIGH après 10 ms,
l'indépendance des contrôles de l'esclave et le déclenchement unique sur Shoot
0 → 1. Ils ne simulent ni l'ESP32 ni les latences d'interruptions Wokwi.

## Validation dans Wokwi après reconstruction

1. Compiler le firmware (`pio run -e esp32dev` depuis `firmware/`) et redémarrer
   complètement la simulation pour recharger les WASM.
2. Attendre `[BOOT]`, sélectionner **beam1**, régler 72 km/h et laisser Shoot à 0
   quelques millisecondes, puis le passer à 1.
3. Attendre une seule ligne JSON de tir avec `ball_speed_kmh` proche de 72.0,
   sans erreur de sens. La ligne `[SHOT]` est un log du même tir, pas un second
   événement. Maintenir Shoot à 1 pendant une seconde : aucun nouveau tir.
4. Remettre Shoot à 0, attendre 500 ms, puis à 1 : un nouveau tir doit apparaître.
5. Actionner les contrôles de **beam2** : ils ne doivent ni déclencher de tir ni
   modifier la vitesse des tirs commandés par beam1.
6. Pour observer les signaux, brancher un analyseur logique dans une copie de test
   du diagramme : GPIO25, GPIO26 et SYNC. Les sorties IR sont HIGH au repos ;
   GPIO25 descend avant GPIO26, avec 10 ms d'écart. Pour mesurer la latence ESP32,
   capturer les timestamps ISR dans un tampon et les afficher depuis `loop()` ;
   ne pas imprimer depuis les ISR.

Les 34 tests `pio test -e native` couvrent notamment IR2 → IR1 rejeté, le timeout
et le réarmement. Pour vérifier aussi le rejet dans le simulateur, utiliser une
copie de test avec un générateur de fronts actifs LOW : IR2/GPIO26 d'abord, puis
IR1/GPIO25 10 ms plus tard, sorties initialement HIGH et chip IR déconnecté des
deux entrées. Attendre une erreur de sens et aucun JSON de tir. Le diagramme
principal et les associations GPIO restent inchangés.

Avec un CLI Wokwi installé et authentifié, exécuter depuis `firmware/` :

```sh
wokwi-cli . --scenario scenarios/shot-72kmh.test.yaml
wokwi-cli . --scenario scenarios/shot-rejected-fast.test.yaml
wokwi-cli . --scenario scenarios/shot-rejected-slow.test.yaml
wokwi-cli . --scenario scenarios/shot-abort-timeout.test.yaml
```

Ne pas confondre le succès des tests WASM locaux avec celui de ces scénarios.
