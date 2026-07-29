# AquaLook Engineering Reference — Compilation, déploiement et validation matérielle

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Sources : `platformio.ini`, `aqualook_partitions.csv`, `tools/version_build.py`, sources de test et checkpoints
- Composants : PlatformIO, profils legacy/V4, bancs ciblés, LittleFS, upload série
- Maturité : D4

## Objet

Ce document définit la chaîne de compilation, de déploiement et de validation applicable au dépôt courant. Les noms d’environnements, filtres de sources et options sont ceux de `platformio.ini`.

## Socle commun

- plateforme : `espressif32 @ 6.13.0` ;
- carte : `esp32dev` ;
- framework : Arduino ;
- moniteur série : `115200` bauds ;
- upload nominal configuré à `921600` bauds ;
- partitions : `aqualook_partitions.csv` ;
- système de fichiers : LittleFS ;
- source LittleFS : dossier `littlefs/`, défini par `data_dir = littlefs`.

Le dossier `data/` reste la source complète destinée à la carte SD ; il n’est pas utilisé directement par `buildfs` dans le profil courant.

## Environnements confirmés

| Environnement | Finalité | Sources particulières | Backend |
|---|---|---|---|
| `ProgrammeArrosage` | firmware nominal | toutes les sources sauf bancs et variantes de main | legacy |
| `ProgrammeArrosage_legacy` | alias explicite nominal | hérite de `ProgrammeArrosage` | legacy |
| `ProgrammeArrosage_v4` | firmware avec sélection V4 | même runtime principal | V4 activé à la compilation |
| `test_execution_engine` | banc déterministe passif | `EquipmentExecutionEngine.cpp`, `test_execution_engine.cpp` uniquement | aucun backend physique |
| `calibration` | calibration tactile | `calibration_touch.cpp` uniquement | sans runtime nominal |
| `test_relais` | diagnostic I²C et relais | `test_relais.cpp` uniquement | banc matériel |
| `debug_boot` | diagnostic boot interactif | runtime avec exclusion de `main_nominal.cpp` | legacy |

Le profil V4 active `AQUALOOK_RELAY_BACKEND_V4=1` et désactive le backend legacy. Les interfaces V4 autorisées sont GPIO, I²C et VIRTUAL ; SPI, UART, OneWire, CAN, RS-485 et REMOTE restent désactivées.

## Commandes de compilation

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4
pio run -e test_execution_engine
pio run -e calibration
pio run -e test_relais
```

Les trois dernières commandes compilent des firmwares de banc. Leur réussite ne valide pas le firmware nominal.

## Upload et moniteur

```powershell
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM9
pio device monitor --port COM9 --baud 115200
```

`COM9` est la valeur locale actuellement inscrite dans `platformio.ini`. Elle doit être remplacée par le port réel de la machine et ne constitue pas un invariant du projet.

## LittleFS

```powershell
pio run -e ProgrammeArrosage -t buildfs
pio run -e ProgrammeArrosage -t uploadfs --upload-port COM9
```

Une modification de `littlefs/` impose `buildfs`. Une modification de `data/` concerne le contenu SD de référence et exige une validation séparée du manifeste SD.

## Chaîne de validation par type de modification

| Modification | Compilations minimales | Validation supplémentaire |
|---|---|---|
| logique Scheduler/configuration | legacy + V4 | scénario fonctionnel et reboot |
| backend/relais/topologie | legacy + V4 + `test_relais` | mesure matérielle P5 |
| modèle V4/moteur | V4 + `test_execution_engine` | scénarios déterministes |
| écran/tactile | firmware concerné + `calibration` si calibration | test sur dalle réelle |
| LittleFS | firmware + `buildfs` | chargement des fallbacks |
| SD/ressources Web | firmware | matrice SD/LittleFS/firmware |
| Web/API | legacy + V4 | tests GET/POST et effets observables |
| sécurité | environnements impactés | tests négatifs et absence de secrets |

## Preuves attendues

Une livraison distingue explicitement :

1. compilation réussie ;
2. upload réussi ;
3. boot observé ;
4. test fonctionnel exécuté ;
5. test dégradé exécuté ;
6. test matériel P5 lorsqu’un comportement électrique change.

Une compilation ou un upload ne vaut pas validation fonctionnelle.

## Critères de blocage

L’intégration est bloquée si :

- un environnement requis ne compile pas ;
- `git diff --check` échoue ;
- le firmware nominal n’est pas distingué d’un banc de test ;
- un changement LittleFS n’a pas été validé par `buildfs` ;
- une modification matérielle n’a pas de preuve P5 ou de mention explicite « non testée » ;
- une route, un schéma NVS ou un contrat JSON change sans mise à jour documentaire ;
- un secret apparaît dans le diff, les logs ou les artefacts ;
- la documentation impactée n’est pas consolidée au checkpoint.

## Invariants

- `INV-BLD-001` : aucun code non compilé n’est déclaré validé.
- `INV-BLD-002` : les profils legacy et V4 restent compilables tant que la coexistence est maintenue.
- `INV-BLD-003` : un banc ciblé ne remplace jamais la compilation du firmware nominal.
- `INV-BLD-004` : une modification matérielle exige une validation sur cible.
- `INV-BLD-005` : le checkpoint référence le commit exact, les environnements et les tests exécutés.
- `INV-BLD-006` : `littlefs/` est la source actuelle de `buildfs`; `data/` est traité comme contenu SD.

## Références

- `platformio.ini` ;
- `aqualook_partitions.csv` ;
- `tools/version_build.py` ;
- `src/test_execution_engine.cpp` ;
- `src/test_relais.cpp` ;
- `src/calibration_touch.cpp` ;
- `30_TEST_AND_ANTI_REGRESSION_MATRIX.md` ;
- `11_CHECKPOINT_CONSOLIDATION.md`.

## Historique

### 1.1

Consolidation D4 des profils, filtres de sources, commandes, preuves et critères de blocage.