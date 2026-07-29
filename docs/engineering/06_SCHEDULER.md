# AquaLook Engineering Reference — Scheduler

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/ScheduleManager.h`, `src/ScheduleManager.cpp`, `src/main.cpp`, `src/ConfigManager.h`
- Composant : `ScheduleManager`
- Maturité : D4

## Mission

`ScheduleManager` interprète les programmes chargés en RAM, détecte les occurrences et demande l’activation ou l’arrêt d’une zone par callback. Il ne pilote jamais directement le matériel.

## Modèle de données réel

Défini dans `src/ScheduleManager.h` :

- `TimeSlot` : `hour`, `minute`, `duration`, `enabled` ;
- `DaySchedule` : `slots[MAX_SLOTS]` ;
- `RainConfig` : seuil de pluie et horizon ;
- `ZoneSchedule` : mode jours/intervalle, ancre, pluie et créneaux ;
- `ActiveSlot` : état d’exécution, `startMs`, `durationMs`, origine manuelle.

Les tableaux internes sont dimensionnés à `MAX_ZONES`; le nombre actif est `_nbZones`.

## API publique confirmée

```cpp
void begin();
void update(int hour, int minute, int weekday, uint32_t epochDay, float rainMm);
void setNbZones(uint8_t nb);
uint8_t getNbZones() const;
ZoneSchedule getZoneSchedule(uint8_t zone) const;
bool isZoneActive(uint8_t zone) const;
String getLastReason(uint8_t zone) const;
uint32_t getElapsedMs(uint8_t zone) const;
uint32_t getRemainingMs(uint8_t zone) const;
void setMode(uint8_t zone, uint8_t mode);
void setIntervalDays(uint8_t zone, uint8_t days);
void setIntervalAnchorDay(uint8_t zone, uint32_t epochDay);
void clearIntervalProgramming(uint8_t zone);
void setDaySlot(...);
void setIntervalSlot(...);
void setRainConfig(uint8_t zone, float threshMm, uint8_t hours);
void setManualDuration(uint16_t minutes);
void startManualWatering(uint8_t zone);
void stopManualWatering(uint8_t zone);
void setRelayCallback(RelayCallback cb);
```

## Chaîne d’appel réelle

Dans `src/main.cpp` :

```cpp
scheduleMgr.begin();
scheduleMgr.setRelayCallback(onRelayRequest);
configMgr.applyToSchedule(scheduleMgr);
```

Dans `loop()`, `scheduleMgr.update(...)` n’est appelé que lorsque `ntpMgr.isSynced()` est vrai. Les valeurs fournies sont l’heure, la minute, le jour de semaine, le jour epoch et la pluie courante.

Le callback `onRelayRequest(uint8_t zone, bool state)` soumet d’abord le plan au runtime shadow, tente `equipmentMgr.startZone()` ou `stopZone()`, puis utilise `outputAdapter.setZoneValve()` en repli. Le Scheduler ne connaît donc ni `RelaisManager` ni le contrôleur physique.

## Persistance

`ConfigManager::applyToSchedule(ScheduleManager&)` est l’unique pont confirmé entre la configuration persistée et le Scheduler. `ConfigManager` fournit également `nbZones()` et les structures de zones.

## Invariants

- `INV-SCH-001` : un créneau désactivé ne déclenche aucune exécution.
- `INV-SCH-002` : aucun accès direct au matériel depuis `ScheduleManager`.
- `INV-SCH-003` : le callback est câblé dans `src/main.cpp`.
- `INV-SCH-004` : les déclenchements calendaires ne sont évalués qu’après synchronisation temporelle.
- `INV-SCH-005` : la limite de durée matérielle reste imposée hors du Scheduler.

## Validation

- compilation `ProgrammeArrosage_legacy` ;
- compilation et upload `ProgrammeArrosage_v4` ;
- tests de créneaux jours et intervalle ;
- test manuel start/stop ;
- vérification d’absence de double déclenchement ;
- essai relais sur une zone, durée courte, sous surveillance.

## Références

- `src/ScheduleManager.h` ;
- `src/ScheduleManager.cpp` ;
- `src/main.cpp` ;
- `src/ConfigManager.h` ;
- `docs/engineering/35_CODE_TRACEABILITY_REGISTER.md`.

## Historique

### 1.1

Consolidation D4 avec API publique, modèle de données et chaîne d’appel extraits du code.
