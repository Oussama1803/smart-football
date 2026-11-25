# Banc de simulation Wokwi VS Code — tirs et impacts

Ce dossier est un projet Wokwi séparé. Il référence le binaire ESP32 compilé par
le projet de production, mais utilise un unique custom chip de stimulation à la
place des deux barrières IR et des quatre capteurs piézo. Le circuit de
`firmware/diagram.json`, les capteurs de production, le firmware et sa fenêtre
d'impact de 60 ms ne sont pas modifiés par ce banc.

## Séquence

Le contrôle `scenario` sélectionne un cas. Un front `run` de 0 vers 1 produit :

1. IR1 actif bas;
2. IR2 actif bas exactement 10 000 µs plus tard, soit 72 km/h sur 20 cm;
3. l'impact sélectionné 12 000 µs après IR2;
4. une enveloppe analogique décroissante, sur le même modèle simplifié que le
   capteur piézo existant.

| `scenario` | Cas | Sorties analogiques à l'impact |
|---:|---|---|
| 0 | `upper_left` | PZ1 = 3,3 V |
| 1 | `upper_right` | PZ2 = 3,3 V |
| 2 | `lower_left` | PZ3 = 3,3 V |
| 3 | `lower_right` | PZ4 = 3,3 V |
| 4 | aucun impact | aucun piézo |
| 5 | simultané | PZ1 = 1,0 V et PZ4 = 3,3 V; le plus intense doit gagner |

## Préparation locale

Depuis la racine du dépôt :

```powershell
Set-Location firmware
pio run -e esp32dev
```

Compiler le custom chip de test avec clang/wasm-ld :

```powershell
powershell -ExecutionPolicy Bypass -File firmware/wokwi-test/build-chip.ps1
```

Le WASM de test est écrit uniquement dans `firmware/wokwi-test/chips/`.

## Lancement dans VS Code Community

1. Compiler `esp32dev` et le custom chip, comme ci-dessus.
2. Dans VS Code, ouvrir directement le dossier `firmware/wokwi-test` comme
   projet, ou ouvrir `firmware/wokwi-test/diagram.json` dans le workspace.
3. Lancer Wokwi avec **Wokwi: Start Simulator**. Vérifier `[BOOT]` dans le
   moniteur série. Le simulateur doit utiliser le `wokwi.toml` de ce dossier.
4. Sélectionner `shotTest` dans le circuit. Dans ses contrôles, régler
   `Scenario (0-5)` sur le numéro du cas, puis faire passer `Run shot (0 -> 1)`
   de 0 à 1. Un tir est émis par lancement; pour un autre cas, redémarrer la
   simulation, sélectionner le numéro voulu avant de déclencher `run`.
5. Dans le moniteur série, vérifier exactement un JSON `event_type: "shot"`,
   `ball_speed_kmh` proche de 72, `target_zone` correspondant au tableau et
   `impact_intensity` positif pour les quatre zones et le cas simultané. Pour
   le cas 4, vérifier `target_zone: "none"` et `impact_intensity: 0`.

Lancer une nouvelle simulation par scénario garantit un état ESP32 propre. La
séquence complète dure moins que la fenêtre d'acquisition firmware de 60 ms;
l'impact arrive 12 ms après le front IR2.

## Tests locaux et limites

Depuis la racine, `node --test chips/tests/ir-break-beam.test.cjs chips/tests/piezo-sensor.test.cjs chips/tests/shot-sequencer.test.cjs firmware/scripts/check-shot.test.cjs`
exécute les tests WASM existants et ceux du séquenceur (après sa compilation).
`pio test -e native` valide la logique indépendante du matériel.

Ce test valide l'algorithme ESP32, les interruptions GPIO simulées, la capture
ADC Wokwi et le JSON, sans authentification CI. Il ne vérifie pas la latence
matérielle d'un vrai ballon, la réponse mécanique d'un piezo, ni les interactions
dynamiques entre les six chips de production, qui restent couvertes par leurs
tests WASM. Wokwi VS Code exécute la simulation de manière interactive; les
assertions automatisées de scénarios YAML demeurent une fonction du CLI. Les
résultats de la simulation VS Code doivent donc être observés dans le moniteur
série, scénario par scénario.
