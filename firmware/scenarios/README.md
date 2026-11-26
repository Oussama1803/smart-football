# Validation des impacts piézo

Les scénarios `impact-*.test.yaml` complètent les scénarios existants sans les
remplacer. Chaque scénario doit démarrer une simulation neuve.

| Scénario | Stimulation | Zone attendue |
|---|---|---|
| `impact-upper-left` | piezo1, 3,3 V | upper_left |
| `impact-upper-right` | piezo2, 3,3 V | upper_right |
| `impact-lower-left` | piezo3, 3,3 V | lower_left |
| `impact-lower-right` | piezo4, 3,3 V | lower_right |
| `impact-no-impact` | aucun impact | none, intensité 0 |
| `impact-simultaneous` | piezo1 à 1 V et piezo4 à 3,3 V | lower_right |

Après `[BOOT]`, les déclencheurs sont remis à zéro puis échantillonnés pendant
2 ms. Shoot passe à 1 à 72 km/h. Les impacts sont commandés 12 ms plus tard :
IR2 doit arriver à 10–11 ms, et le polling du piézo produit l'impulsion à
12–13 ms. Elle est donc dans la fenêtre de collecte de 60 ms après IR2.
Le cas simultané commande les deux impacts au même instant de scénario ; le
polling peut introduire jusqu'à 1 ms de décalage. Les amplitudes différentes
évitent de dépendre d'une égalité ADC ou de la saturation. La règle d'égalité
(premier indice gagnant) reste couverte par les tests natifs existants.

## Exécution automatisée

Prérequis : firmware compilé, WASM reconstruits via `bash chips/build.sh`, CLI
Wokwi installé dans le PATH et authentification CI configurée hors du dépôt.
Depuis `firmware/` :

```sh
node scripts/run-impact-scenarios.cjs
```

Le lanceur exécute les six simulations et valide les JSON : exactement un tir,
vitesse à 72 ± 0,5 km/h, bonne zone, intensité entière strictement positive
(zéro pour none), aucune erreur ni alerte du firmware. Il échoue aussi si le
CLI manque, si Wokwi échoue ou si le scénario expire.
La validation lit le journal série dédié (`--serial-log-file`) dans un dossier
temporaire hors du dépôt, supprimé après chaque scénario. La sortie console
mélange les données série et les messages de progression du CLI, qui peuvent
interrompre une ligne JSON.

Pour un seul scénario :

```sh
wokwi-cli . --scenario scenarios/impact-upper-left.test.yaml --timeout 3000 --serial-log-file upper-left.log
node scripts/check-shot.cjs upper-left.log upper_left
```

Vérifier également le code de sortie du CLI. Le YAML attend la zone dans le
JSON ; le vérificateur séparé impose l'intensité positive et l'unicité. Il ne
faut pas présenter le seul succès de `wait-serial` comme une validation complète.

## Si seul Wokwi dans VS Code est disponible

Redémarrer complètement la simulation pour recharger les WASM. Reproduire les
contrôles et délais avec l'automatisation Wokwi lorsqu'elle est accessible.
Deux clics manuels ne garantissent pas un impact dans les 60 ms : ne pas
conclure à un défaut de capteur sur cette seule manipulation. Pour un contrôle
manuel du chemin ADC, déclencher l'impact avant Shoot peut laisser une queue
de décroissance mesurable, mais cela ne valide pas le scénario synchronisé.

Capturer le moniteur série brut puis utiliser `scripts/check-shot.cjs` sur le
fichier. Si le résultat reste none, capturer temporairement, depuis `loop()`
après la collecte, les quatre maxima ADC et les timestamps d'ouverture/fin de
fenêtre. Ne pas imprimer dans les ISR ni à chaque lecture ADC.

Les tests locaux `node --test chips/tests/*.test.cjs` (racine),
`node --test scripts/check-shot.test.cjs` et `pio test -e native` (firmware)
ne remplacent pas une exécution du simulateur ESP32 : l'échelle ADC et la
latence de lecture restent à valider dans Wokwi.
