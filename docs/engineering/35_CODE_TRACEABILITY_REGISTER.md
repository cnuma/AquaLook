# AquaLook Engineering Reference — Registre de traçabilité vers le code

- Version documentaire : 1.3
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
| Stockage | `14_SD_AND_STATIC_RESOURCES.md` | `src/StorageManager.*`, `src/SdStaticHandler.*`, `platformio.ini` | SD enregistrée auprès du WebManager ; LittleFS via `data_dir = littlefs` | P3/P5 |

## Éléments D4 extraits

### Affichage et tactile

- API publique `initTft`, `showSplash`, `begin`, `update`, `requestDynamicRefresh` et `setOutputAdapter` ;
- tactile XPT2046 sur bus VSPI séparé ;
- polling tactile 80 ms et debounce 500 ms ;
- refresh nominal 5 s et actif 1 s, configurables ;
- état OutputAdapter prioritaire sur le fallback RelaisManager ;
- modes actifs LIST et GRID2 ; `GRID4` reste dormant car l’interface borne à 8 zones.

### Réseau et Wi-Fi

- états `IDLE`, `CONNECTING`, `CONNECTED`, `DISCONNECTED`, `CAPTIVE_STARTING`, `CAPTIVE_PORTAL` ;
- timeout STA 15 s, intervalle 30 s, maximum 5 tentatives ;
- stabilisations 100/50/200/200 ms ;
- DNS captif port 53 et AP `Arrosage-Setup` ;
- scan Wi-Fi asynchrone ;
- auto-reconnexion native désactivée.

## Incohérences et risques détectés

- `handleSetWifi()` imprime actuellement le mot de passe Wi-Fi en clair sur la sortie série ;
- le point d’accès `Arrosage-Setup` est créé sans mot de passe ;
- les anciens documents indiquaient un passage automatique de l’EventLog à l’heure NTP absolue, absent du code actuel ;
- `EventLog::log()` ne protège pas le buffer par section critique ;
- `GRID4` est déclaré mais non sélectionnable dans le firmware courant ;
- `DisplayManager::initTft()` ne monte pas LittleFS malgré un ancien commentaire d’interface ; le propriétaire réel reste `ConfigManager` ;
- après cinq échecs Wi-Fi, aucune nouvelle tentative automatique n’est lancée sans événement externe ou redémarrage ;
- les contrats HTTP, affichage et réseau ne sont pas encore couverts par des tests automatisés.

## Niveaux de preuve

- P0 : assertion non reliée ;
- P1 : fichier identifié ;
- P2 : symbole identifié ;
- P3 : chaîne d’appel et effet identifiés ;
- P4 : test reproductible associé ;
- P5 : validation matérielle ou cible et checkpoint référencé.

## Écarts ouverts

- manifeste complet SD/LittleFS et contrats de fallback ;
- frontières stabilisées du modèle V4 et de la météo ;
- correspondance exhaustive invariants → tests automatisés ;
- correction du secret Wi-Fi journalisé ;
- protection du portail captif ;
- tests de concurrence EventLog ;
- décision sur le code dormant `GRID4` ;
- tests automatiques des machines d’états et cadences.

## Références

- `src/main.cpp` ;
- `src/DisplayManager.h` et `.cpp` ;
- `src/ScreenManager.h` et `.cpp` ;
- `src/WiFiManager.h` et `.cpp` ;
- `src/WebManager.h` et `.cpp` ;
- `platformio.ini` ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_STEP6_RUN6-26.md`.