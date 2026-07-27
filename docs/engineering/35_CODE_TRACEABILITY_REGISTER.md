# AquaLook Engineering Reference — Registre de traçabilité vers le code

- Version documentaire : 1.4
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
| Affichage | `13_DISPLAY_AND_TOUCH.md` | `src/DisplayManager.h`, `.cpp`, `src/ScreenManager.*`, `src/main.cpp`, `platformio.ini` | TFT/splash au boot ; bus VSPI tactile ; `displayDirty` → configuration puis redraw ; refresh dynamique depuis Runtime | P3/P5 |
| Réseau | `18_NETWORK_AND_WIFI.md` | `src/WiFiManager.h`, `.cpp`, `src/WebManager.*`, `src/main.cpp` | machine d’états STA/AP, actions différées, retries, DNS captif et scan asynchrone | P3 |
| Stockage | `14_SD_AND_STATIC_RESOURCES.md` | `src/StorageManager.h`, `.cpp`, `src/SdStaticHandler.h`, `.cpp`, `src/WebManager.*`, `src/main.cpp` | montage SdFat, sentinelle `/www/index.html`, handler SD avant LittleFS, erreur → retrait de la chaîne | P3/P5 |
| Modèle V4 | `16_V4_EQUIPMENT_MODEL_AND_WEATHER.md` | `src/EquipmentManager.*`, `src/EquipmentModel.*`, `src/EquipmentOutputRuntimeAdapter.*`, `src/main.cpp`, `platformio.ini` | résolution zone/dépendances, plans max 4 étapes, électrovannes exécutables, pompe dry-run | P3/P5 |
| Météo | `16_V4_EQUIPMENT_MODEL_AND_WEATHER.md` | `src/WeatherManager.h`, `.cpp`, `src/ConfigManager.*`, `src/EventBus.*` | `update()` copie la config puis lance `weather-fetch`; HTTP/JSON hors boucle; résultat appliqué ensuite | P3/P4 |

## Éléments D4 extraits

### Stockage et ressources

- états `NOT_INITIALIZED`, `READY`, `SD_UNAVAILABLE`, `WEB_ASSETS_MISSING`, `READ_ERROR` ;
- SPI logiciel SdFat et sentinelle `/www/index.html` contrôlée toutes les 2 s ;
- priorité du handler SD avant `serveStatic()` LittleFS ;
- pages hybrides `/setup` et `/logs` ;
- logo SD → LittleFS → SVG firmware ;
- route `/api/storage`, types MIME, cache et en-tête `X-AquaLook-Storage` ;
- protection contre traversée de chemin et exclusion `/api/`.

### Modèle V4 et météo

- API de résolution, planification, dry-run et exécution d’`EquipmentManager` ;
- plans de quatre étapes maximum avec vanne, pompe et attente ;
- seules les électrovannes sont actuellement exécutables ; les actions pompe restent passives ;
- tâche météo FreeRTOS de 12288 octets, priorité 1 ;
- timeout HTTP 8 s et délai de retry 60 s ;
- endpoint OWM forecast, 40 créneaux, cinq jours et filtre ArduinoJson streaming.

## Incohérences et risques détectés

- `handleSetWifi()` imprime actuellement le mot de passe Wi-Fi en clair sur la sortie série ;
- le point d’accès `Arrosage-Setup` est créé sans mot de passe ;
- OpenWeatherMap est appelé en HTTP et la clé API est placée dans l’URL ;
- la décision `rainExpected` utilise actuellement le seuil de pluie de la zone 0 ;
- les étapes pompe sont présentes dans les plans mais non exécutées ;
- `usedBytes` et `totalBytes` ne sont pas exposés par `/api/storage`, et `usedBytes` reste à zéro ;
- les anciens documents indiquaient un passage automatique de l’EventLog à l’heure NTP absolue, absent du code actuel ;
- `EventLog::log()` ne protège pas le buffer par section critique ;
- `GRID4` est déclaré mais non sélectionnable dans le firmware courant ;
- après cinq échecs Wi-Fi, aucune nouvelle tentative automatique n’est lancée sans événement externe ou redémarrage.

## Niveaux de preuve

- P0 : assertion non reliée ;
- P1 : fichier identifié ;
- P2 : symbole identifié ;
- P3 : chaîne d’appel et effet identifiés ;
- P4 : test reproductible associé ;
- P5 : validation matérielle ou cible et checkpoint référencé.

## Écarts ouverts

- manifeste versionné de la carte SD de référence et mise à jour atomique de `/www` ;
- schéma exhaustif d’`EquipmentModel` et stabilité des identifiants ;
- décision et implémentation concernant les actions pompe ;
- HTTPS pour OWM et protection de la clé API ;
- politique multi-zone du seuil météo ;
- correspondance exhaustive invariants → tests automatisés ;
- correction du secret Wi-Fi journalisé et protection du portail captif ;
- tests de concurrence EventLog ;
- décision sur le code dormant `GRID4`.

## Références

- `src/StorageManager.h` et `.cpp` ;
- `src/SdStaticHandler.h` et `.cpp` ;
- `src/EquipmentManager.h` et `.cpp` ;
- `src/EquipmentModel.h` et `.cpp` ;
- `src/EquipmentOutputRuntimeAdapter.h` et `.cpp` ;
- `src/WeatherManager.h` et `.cpp` ;
- `src/main.cpp` ;
- `platformio.ini` ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_STEP6_RUN6-26.md`.