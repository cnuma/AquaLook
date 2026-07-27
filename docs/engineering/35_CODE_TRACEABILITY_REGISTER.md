# AquaLook Engineering Reference — Registre de traçabilité vers le code

- Version documentaire : 1.2
- Statut : actif
- Dernière consolidation : 2026-07-27
- Source : branche stable `main` et code du commit ciblé
- Maturité : D4

## Objet

Ce registre relie les affirmations d’architecture aux fichiers, symboles, chaînes d’appel et validations réelles.

## Registre consolidé

| Composant | Document | Ancrages code confirmés | Chaîne d’appel / propriété | Preuve |
|---|---|---|---|---|
| Scheduler | `06_SCHEDULER.md` | `src/ScheduleManager.h`, `.cpp`, `src/main.cpp` | `setup()` câble `setRelayCallback(onRelayRequest)` ; `loop()` appelle `update()` après synchro NTP | P3/P5 |
| Configuration | `07_CONFIGURATION_AND_PERSISTENCE.md` | `src/ConfigManager.h`, `.cpp`, `platformio.ini`, `src/main.cpp` | NVS `aqualook`, clés `config` et `intAnchors`, schéma 1 ; LittleFS monté par `ConfigManager` | P3 |
| Runtime | `15_RUNTIME_AND_PROFILING.md` | `src/main.cpp`, `src/RuntimeProfiler.*`, `src/SystemDiagnostics.*` | ordre exact de `setup()` et `loop()` ; segments instrumentés | P4/P5 |
| Relais | `08_RELAY_AND_EQUIPMENT_CONTROL.md` | `src/RelaisManager.h`, `.cpp`, `src/RelayTopology.*`, `src/main.cpp`, `platformio.ini` | Scheduler → `onRelayRequest` → EquipmentManager → OutputAdapter → backend → I²C | P3/P5 |
| Web | `09_WEB_AND_HTTP_INTERFACES.md` | `src/WebManager.h`, `.cpp`, `src/SdStaticHandler.*`, `src/main.cpp` | handlers SD/faults enregistrés avant `begin()` ; routes GET/POST inventoriées | P3 |
| EventLog | `10_TIME_AND_EVENTLOG.md` | `src/EventLog.h`, `.cpp`, `src/FaultManager.*`, `src/WebManager.*` | buffer circulaire de 60 entrées, timestamp `millis()`, erreurs vers FaultManager | P3 |
| Diagnostics | `24_DIAGNOSTICS_AND_OBSERVABILITY.md` | `src/SystemDiagnostics.h`, `.cpp`, `src/RuntimeProfiler.*`, `src/WebManager.*` | `loopEnter/loopExit`, réponses Web, `fillJson`, route `/api/diagnostics` | P3/P4 |
| Affichage | `13_DISPLAY_AND_TOUCH.md` | `src/DisplayManager.*`, `src/main.cpp` | TFT initialisé avant stockage ; refresh dynamique depuis commande de zone | P3/P5 |
| Stockage | `14_SD_AND_STATIC_RESOURCES.md` | `src/StorageManager.*`, `src/SdStaticHandler.*`, `platformio.ini` | SD enregistrée auprès du WebManager ; LittleFS via `data_dir = littlefs` | P3/P5 |

## Éléments D4 extraits

### Relais

- API `begin`, `update`, `setRelay`, `setAssignment`, états et topologie ;
- initialisation sûre directe/inverse ;
- contrôleurs XL9535 et MCP23017 ;
- refus en cas de mapping invalide ou carte absente ;
- sécurité de durée maximale dans `RelaisManager::update()`.

### Web

- serveur HTTP port 80 ;
- routes captives, GET, POST JSON et routes de diagnostic ;
- ordre SD puis LittleFS ;
- sauvegardes système différées hors callback AsyncTCP ;
- réponse avant redémarrage.

### EventLog et diagnostics

- `LOG_CAPACITY=60`, `LOG_MSG_LEN=72` ;
- chronologie relative `millis()` uniquement ;
- acquittement distinct de l’effacement ;
- objets JSON `system`, `build`, `memory`, `loop`, `web`, `wifi` ;
- seuil d’overrun 100 ms et limitation des logs à 5 secondes.

## Incohérences et risques détectés

- `handleSetWifi()` imprime actuellement le mot de passe Wi-Fi en clair sur la sortie série ; correction de sécurité requise ;
- les anciens documents indiquaient un passage automatique de l’EventLog à l’heure NTP absolue, absent du code actuel ; correction documentaire appliquée ;
- `EventLog::log()` ne protège pas le buffer par section critique ; vérifier la concurrence avant ajout de nouvelles tâches productrices ;
- le contrat HTTP n’est pas encore couvert par des tests automatisés ;
- la validation relais multi-topologies reste à archiver sous forme de preuves P5.

## Niveaux de preuve

- P0 : assertion non reliée ;
- P1 : fichier identifié ;
- P2 : symbole identifié ;
- P3 : chaîne d’appel et effet identifiés ;
- P4 : test reproductible associé ;
- P5 : validation matérielle ou cible et checkpoint référencé.

## Écarts ouverts

- API exacte de l’affichage et du tactile ;
- manifeste complet SD/LittleFS et contrats de fallback ;
- états, délais et stratégies de reconnexion réseau ;
- frontières stabilisées du modèle V4 et de la météo ;
- correspondance exhaustive invariants → tests automatisés ;
- correction du secret Wi-Fi journalisé ;
- tests de concurrence EventLog.

## Références

- `src/main.cpp` ;
- `src/RelaisManager.h` et `.cpp` ;
- `src/WebManager.h` et `.cpp` ;
- `src/EventLog.h` et `.cpp` ;
- `src/SystemDiagnostics.h` et `.cpp` ;
- `platformio.ini` ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_STEP6_RUN6-26.md`.