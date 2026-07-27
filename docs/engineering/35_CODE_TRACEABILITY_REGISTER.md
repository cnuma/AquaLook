# AquaLook Engineering Reference — Registre de traçabilité vers le code

- Version documentaire : 1.1
- Statut : actif
- Dernière consolidation : 2026-07-27
- Source : branche stable `main` et code du commit ciblé
- Maturité : D4

## Objet

Ce registre relie les affirmations d’architecture aux fichiers, symboles, chaînes d’appel et validations réelles.

## Registre consolidé

| Composant | Document | Ancrages code confirmés | Chaîne d’appel / propriété | Preuve |
|---|---|---|---|---|
| Scheduler | `06_SCHEDULER.md` | `src/ScheduleManager.h`, `src/ScheduleManager.cpp`, `src/main.cpp` | `setup()` câble `setRelayCallback(onRelayRequest)` ; `loop()` appelle `update()` après synchro NTP | P3, matériel checkpoint étape 6 |
| Configuration | `07_CONFIGURATION_AND_PERSISTENCE.md` | `src/ConfigManager.h`, `src/ConfigManager.cpp`, `platformio.ini`, `src/main.cpp` | namespace NVS `aqualook`, clés `config` et `intAnchors`, schéma 1 ; LittleFS monté par `ConfigManager` | P3 |
| Runtime | `15_RUNTIME_AND_PROFILING.md` | `src/main.cpp`, `src/RuntimeProfiler.*`, `src/SystemDiagnostics.*` | ordre exact de `setup()` et `loop()` documenté ; chaque segment est instrumenté | P4, matériel checkpoint étape 6 |
| Relais | `08_RELAY_AND_EQUIPMENT_CONTROL.md` | `src/main.cpp`, `src/RelaisManager.*`, `src/RelaisManagerBackend.*` | `onRelayRequest` → `EquipmentManager` → `EquipmentOutputRuntimeAdapter` → backend physique ou fallback | P3/P5 |
| Web | `09_WEB_AND_HTTP_INTERFACES.md` | `src/WebManager.*`, ressources JS, `src/main.cpp` | `webMgr.begin(...)`, handler SD et routes défaut enregistrés au boot | P2 |
| EventLog | `10_TIME_AND_EVENTLOG.md` | `src/EventLog.*`, `src/main.cpp` | événements de boot, I2C, relais, équipement et Runtime centralisés | P3 |
| Affichage | `13_DISPLAY_AND_TOUCH.md` | `src/DisplayManager.*`, `src/main.cpp` | initialisation TFT avant stockage ; `update()` après Web ; refresh dynamique depuis commande de zone | P3/P5 |
| Stockage | `14_SD_AND_STATIC_RESOURCES.md` | `src/StorageManager.*`, `src/SdStaticHandler.*`, `platformio.ini` | SD enregistrée auprès du WebManager ; LittleFS de secours via `data_dir = littlefs` | P3/P5 |

## API Scheduler extraite

Les signatures publiques de `ScheduleManager` sont désormais documentées dans `06_SCHEDULER.md`, notamment `begin()`, `update(...)`, les setters de créneaux, les commandes manuelles et `setRelayCallback()`.

## Persistance extraite

Les éléments suivants sont confirmés dans `src/ConfigManager.h` :

```text
CFG_PATH=/config.json
CFG_VERSION=2
CFG_NVS_NAMESPACE=aqualook
CFG_NVS_KEY=config
CFG_NVS_INTERVAL_ANCHORS_KEY=intAnchors
CFG_NVS_SCHEMA=1
```

## Niveaux de preuve

- P0 : assertion non reliée ;
- P1 : fichier identifié ;
- P2 : symbole identifié ;
- P3 : chaîne d’appel et effet identifiés ;
- P4 : test reproductible associé ;
- P5 : validation matérielle ou cible et checkpoint référencé.

## Écarts ouverts

- inventaire automatique complet des routes et méthodes HTTP ;
- fonctions exactes et format JSON d’`EventLog` ;
- cartographie détaillée des contrôleurs relais et logique directe/inverse ;
- API précise de `RuntimeProfiler` et seuils ;
- correspondance exhaustive invariants → tests automatisés.

## Références

- `src/main.cpp` ;
- `src/ScheduleManager.h` ;
- `src/ConfigManager.h` ;
- `platformio.ini` ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_STEP6_RUN6-26.md`.
