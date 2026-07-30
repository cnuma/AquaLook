# AquaLook V4 — Phase 7 — RUN7.9

## Objectif

Supprimer la double construction du plan d'équipements dans le runtime shadow.

Le plan validé par `EquipmentOrchestrator` est désormais conservé dans `EquipmentOrchestrator::Preview` puis transmis directement à `EquipmentExecutionShadowRuntime`.

## Branche

`work/step7-run7-9`

## Base validée

RUN7.8 matériellement validé :

- audits START/STOP zones 1 et 2 en `match=yes` ;
- zone 1 via `physical_backend` ;
- zone 2 via `relay_manager_fallback` ;
- moteur shadow en `SUCCEEDED` ;
- autorité physique inchangée.

## Modifications

### `src/EquipmentOrchestrator.h`

`Preview` transporte maintenant un `EquipmentManager::ZoneExecutionPlan plan` complet.

### `src/EquipmentOrchestrator.cpp`

Le plan construit par `EquipmentManager` est stocké directement dans `Preview::plan`. Les champs de synthèse (`planResult`, `requiresPump`, `stepCount`) sont dérivés de ce plan unique.

### `src/main.cpp`

Le patch `tools/run7-9/apply_run7_9.ps1` remplace `onRelayRequest()` afin que :

- le moteur shadow consomme `orchestratorPreview.plan` ;
- aucune seconde construction du plan ne soit réalisée lorsque l'orchestrateur est prêt ;
- un fallback de construction historique subsiste uniquement si l'orchestrateur est indisponible ;
- l'autorité reste `no` ;
- le chemin physique `equipmentMgr.startZone/stopZone` reste inchangé.

## Log attendu

```text
Orchestrator handoff: zone=1 intent=START source=orchestrator result=0 steps=1 pump=no authority=no
```

Le fallback éventuel serait explicitement visible :

```text
source=legacy_shadow_builder
```

## Invariants

- aucune prise d'autorité physique par l'orchestrateur ;
- aucune modification de `ScheduleManager`, `RelaisManager` ou des backends ;
- zone 1 conserve le backend physique V4 ;
- zone 2 conserve le fallback `RelaisManager` ;
- retour arrière possible par restauration de `src/main.cpp`.

## Validation requise

```powershell
pio run -c platformio.run7-2.ini -e test_equipment_orchestrator -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Puis validation nominale complète :

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Tester START/STOP sur les zones 1 et 2 et vérifier `source=orchestrator`.
