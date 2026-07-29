# AquaLook Engineering Reference — Runtime et profiling

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/main.cpp`, `src/RuntimeProfiler.*`, `src/SystemDiagnostics.*`
- Composants : `setup()`, `loop()`, Runtime V4, profiler, EventLog
- Maturité : D4

## Mission

Le Runtime est câblé dans `src/main.cpp`. Il initialise les managers dans un ordre déterministe puis exécute une boucle coopérative instrumentée par `RuntimeProfiler`.

## Ordre d’initialisation confirmé

1. série, `FaultManager`, `EventLog`, `SystemDiagnostics` ;
2. bus I²C et scan ;
3. `ConfigManager` et configuration Runtime des équipements ;
4. TFT puis `StorageManager` ;
5. `RelaisManager`, backend legacy ou pilote V4, `EquipmentManager` ;
6. `ScheduleManager`, callback relais et application de la configuration ;
7. Wi-Fi, NTP, Web et météo ;
8. `DisplayManager`.

Les sorties sont initialisées avant l’activation du Scheduler.

## Boucle principale réelle

L’ordre instrumenté dans `loop()` est :

```text
SystemDiagnostics::loopEnter
FaultManager::update
StorageManager::update
WiFiManager::update
NTPManager::update si connecté
WeatherManager::update(true) si connecté
ScheduleManager::update si NTP synchronisé
EquipmentExecutionShadowRuntime::update
RelaisManager::update
WebManager::update
DisplayManager::update
DisplayPlanningDecor
FaultManager::update
yield
SystemDiagnostics::loopExit
```

Chaque segment utilise :

```cpp
uint32_t startedUs = RuntimeProfiler::start();
RuntimeProfiler::stop(RuntimeProfiler::Component::<COMPONENT>, startedUs);
```

## Chaîne de commande des zones

`ScheduleManager` appelle `onRelayRequest(zone, state)`. Cette fonction :

1. construit et soumet un plan au runtime shadow ;
2. appelle `equipmentMgr.startZone()` ou `stopZone()` ;
3. demande un rafraîchissement dynamique si l’action réussit ;
4. journalise l’échec ;
5. utilise `outputAdapter.setZoneValve()` comme repli.

Le mode physique de la pompe reste bloqué ; la configuration est forcée en exécution shadow passive.

## Profiling

Les mesures basées sur `micros()` sont du temps mural. Elles peuvent inclure interruptions, préemptions FreeRTOS et attente système. `yield()` est mesuré mais exclu des alertes de lenteur métier.

## Invariants

- `INV-RUN-001` : l’ordre des managers dans `loop()` reste explicite et non bloquant.
- `INV-RUN-002` : le Scheduler n’est évalué qu’après `ntpMgr.isSynced()`.
- `INV-RUN-003` : le fallback legacy est conservé tant que la migration V4 n’est pas validée.
- `INV-RUN-004` : les mesures du profiler ne sont pas interprétées comme du temps CPU strict.
- `INV-RUN-005` : `FaultManager::update()` encadre le cycle avant et après les services.
- `INV-RUN-006` : le Runtime shadow ne pilote pas physiquement la pompe.

## Validation

- compilation legacy ;
- compilation et upload V4 ;
- démarrage matériel ;
- diagnostics Runtime ;
- fonctionnement hors réseau ;
- synchronisation NTP sans redraw complet ;
- tests relais sur une zone ;
- observation du profiler sous charge Web et météo.

## Références

- `src/main.cpp` ;
- `src/RuntimeProfiler.h` et `.cpp` ;
- `src/SystemDiagnostics.h` et `.cpp` ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_STEP6_RUN6-26.md` ;
- `docs/engineering/35_CODE_TRACEABILITY_REGISTER.md`.

## Historique

### 1.1

Consolidation D4 avec ordre exact de `setup()`, ordre instrumenté de `loop()` et chaîne réelle de commande.
