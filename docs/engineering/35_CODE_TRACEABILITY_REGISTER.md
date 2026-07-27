# AquaLook Engineering Reference — Registre de traçabilité vers le code

- Version documentaire : 1.5
- Statut : actif
- Dernière consolidation : 2026-07-27
- Source : branche stable `main` et code du commit ciblé
- Maturité : D4

## Objet

Ce registre relie les affirmations d’architecture aux fichiers, symboles, chaînes d’appel et validations réelles.

## Registre consolidé

| Composant | Document | Ancrages code confirmés | Chaîne d’appel / propriété | Preuve |
|---|---|---|---|---|
| Scheduler | `06_SCHEDULER.md` | `src/ScheduleManager.*`, `src/main.cpp` | callback puis mise à jour Runtime | P3/P5 |
| Configuration | `07_CONFIGURATION_AND_PERSISTENCE.md` | `src/ConfigManager.*`, `platformio.ini` | NVS `aqualook`, clés `config`/`intAnchors`, LittleFS | P3 |
| Runtime | `15_RUNTIME_AND_PROFILING.md` | `src/main.cpp`, `src/RuntimeProfiler.*` | ordre exact `setup()`/`loop()` | P4/P5 |
| Relais | `08_RELAY_AND_EQUIPMENT_CONTROL.md` | `src/RelaisManager.*`, `src/RelayTopology.*` | Scheduler → modèle → adaptateur → backend → I²C | P3/P5 |
| Web | `09_WEB_AND_HTTP_INTERFACES.md` | `src/WebManager.*`, `src/SdStaticHandler.*` | routes et handlers réels | P3 |
| EventLog/diagnostics | `10`, `24` | `src/EventLog.*`, `src/SystemDiagnostics.*` | buffer, JSON et profiler | P3/P4 |
| Affichage | `13_DISPLAY_AND_TOUCH.md` | `src/DisplayManager.*`, `src/ScreenManager.*` | splash, tactile, redraw et refresh | P3/P5 |
| Réseau | `18_NETWORK_AND_WIFI.md` | `src/WiFiManager.*` | machine d’états STA/AP et DNS | P3 |
| Stockage | `14_SD_AND_STATIC_RESOURCES.md` | `src/StorageManager.*`, `src/SdStaticHandler.*` | SD avant LittleFS, sentinelle et fallbacks | P3/P5 |
| Modèle V4 | `16`, `36` | `src/EquipmentModel.*`, `src/EquipmentManager.*` | validation, dépendances, plans et exécution valve | P3/P5 |
| Météo | `16` | `src/WeatherManager.*` | tâche FreeRTOS, HTTP et JSON filtré | P3/P4 |
| Build/tests | `17`, `30` | `platformio.ini`, `src/test_execution_engine.cpp`, `src/test_relais.cpp`, `src/calibration_touch.cpp` | profils complets et bancs ciblés | P3/P5 |

## Éléments D4 du lot courant

- profils `ProgrammeArrosage`, legacy, V4, `test_execution_engine`, `calibration`, `test_relais` et `debug_boot` ;
- distinction `littlefs/` pour `buildfs` et `data/` pour la carte SD ;
- matrice modification → build → test → preuve ;
- structures `EquipmentConfig`, `ZoneEquipmentLink` et `EquipmentConfigSet` ;
- huit types d’équipements et rôles relais attendus ;
- validation des index, types, noms, liens de zone et pompe optionnelle ;
- limite confirmée : électrovannes exécutables, pompe en dry-run.

## Incohérences et risques ouverts

- mot de passe Wi-Fi journalisé en clair ;
- portail captif sans mot de passe ;
- OWM en HTTP avec clé dans l’URL ;
- seuil météo issu de la zone 0 ;
- actions pompe non exécutées ;
- `EventLog` sans protection de concurrence ;
- `GRID4` dormant ;
- absence de CI et de manifeste SD versionné ;
- champs `minOnSec`/`minOffSec` présents mais application Runtime à confirmer.

## Niveaux de preuve

- P0 : assertion non reliée ;
- P1 : fichier identifié ;
- P2 : symbole identifié ;
- P3 : chaîne d’appel et effet identifiés ;
- P4 : test reproductible associé ;
- P5 : validation matérielle ou cible et checkpoint référencé.

## Références

- `platformio.ini` ;
- `src/EquipmentModel.h` et `.cpp` ;
- `src/EquipmentManager.h` et `.cpp` ;
- `src/test_execution_engine.cpp` ;
- `src/test_relais.cpp` ;
- `src/calibration_touch.cpp` ;
- `17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md` ;
- `30_TEST_AND_ANTI_REGRESSION_MATRIX.md` ;
- `36_DETAILED_EQUIPMENT_MODEL_SCHEMA.md`.