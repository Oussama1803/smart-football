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

## Résultats réels dans Wokwi VS Code Community

Les résultats ci-dessous ont été observés par Saad dans Wokwi VS Code. Ils sont
distincts des tests WASM locaux et des résultats attendus du séquenceur.

| Scénario | `target_zone` observée | Vitesse observée | ADC piézo / intensité observée |
|---:|---|---:|---:|
| 0 | `upper_left` | 72 km/h | 2303 |
| 1 | `upper_right` | 72 km/h | 2169 |
| 2 | `lower_left` | 72 km/h | 2084 |
| 3 | `lower_right` | 72 km/h | 2002 |
| 4 | `none` | 72 km/h | 0 |
| 5 | `lower_right` | 72 km/h | 2002 |

Sept JSON de tir ont été observés au total pendant les sessions manuelles, dont
deux avec `target_zone: "none"`. Le tableau donne le résultat de chaque valeur
de scénario; le cas 5 a été explicitement relancé dans une simulation fraîche.
Les traces brutes des sept événements n'ont pas toutes été conservées, donc le
total ne permet pas à lui seul de compter les JSON par déclenchement. Les zones,
vitesses et valeurs rapportées sont cohérentes avec le firmware : le seuil est
300 ADC, une zone sélectionnée le dépasse, et la zone la plus intense gagne en
cas d'impacts simultanés. Le cas sans impact conserve le tir et sa vitesse, avec
la zone `none` et l'intensité 0.

Capture série réelle fournie pour la confirmation du scénario 5 :

```text
[SHOT] 72.0 km/h, zone lower_right, pressure raw 3, impact raw 2002
```

```json
{"event_id":"c6896deb-69e3-4f10-b85f-aa4e167971d7","device_id":"goal-esp32-01","session_id":"session-001","event_type":"shot","timestamp":"1970-01-01T00:00:08Z","ball_speed_kmh":72.0,"pressure_reading":3,"target_zone":"lower_right","impact_intensity":2002,"firmware_version":"0.1.0"}
```

Le timestamp de 1970 est attendu dans le profil hors ligne sans synchronisation
SNTP. `pressure_reading` et `impact_intensity` sont des valeurs ADC brutes, pas
des mesures physiques calibrées.

## Préparation locale

Depuis la racine du dépôt :

```powershell
Set-Location firmware
pio run -e esp32dev
```

Compiler les deux custom chips avec clang/wasm-ld :

```powershell
powershell -ExecutionPolicy Bypass -File firmware/wokwi-test/build-chips.ps1
```

Les deux WASM et leurs fichiers `.chip.json` sont écrits uniquement dans
`firmware/wokwi-test/chips/`. Le banc est autonome pour ses custom chips; il
réutilise uniquement le binaire ESP32 produit par PlatformIO dans `firmware/.pio`.

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

Depuis la racine, `node --test chips/tests/ir-break-beam.test.cjs chips/tests/piezo-sensor.test.cjs chips/tests/shot-sequencer.test.cjs firmware/wokwi-test/project.test.cjs firmware/scripts/check-shot.test.cjs`
exécute les tests WASM existants, les contrôles statiques du projet autonome,
le FSR local et le séquenceur (après leur compilation).
`pio test -e native` valide la logique indépendante du matériel.

La validation manuelle ci-dessus couvre le circuit Wokwi VS Code et les valeurs
ADC observées. Les tests WASM locaux valident les modèles de puces et le
séquenceur, pas l'intégration réelle dans Wokwi. Aucun résultat MQTT/TLS n'est
couvert par cette validation. Le banc ne vérifie pas la latence
matérielle d'un vrai ballon, la réponse mécanique d'un piezo, ni les interactions
dynamiques entre les six chips de production, qui restent couvertes par leurs
tests WASM. Wokwi VS Code exécute la simulation de manière interactive; les
assertions automatisées de scénarios YAML demeurent une fonction du CLI. Les
résultats de la simulation VS Code doivent donc être observés dans le moniteur
série, scénario par scénario.
