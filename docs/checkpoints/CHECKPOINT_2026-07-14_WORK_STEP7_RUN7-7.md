# AquaLook V4 — CHECKPOINT RUN7.7

## Objectif
Exposer dans le runtime nominal les compteurs d'observation de `EquipmentOrchestrator`, sans lui donner l'autorité sur les relais.

## Périmètre
- source : branche `work/step7-run7-6` validée par banc, 31 tests réussis ;
- nouvelle branche cible : `work/step7-run7-7` ;
- modification runtime limitée à `src/main.cpp` ;
- aucune modification NVS, Web, planning, drivers ou backend ;
- `authority=no` conservé.

## Comportement attendu
À chaque intention shadow, le log inclut les compteurs cumulés :
- demandes totales ;
- START / STOP ;
- plans prêts / rejetés ;
- plans avec pompe ;
- étapes cumulées.

Exemple :

```text
Orchestrator shadow: zone=1 intent=START status=0 plan=0 steps=1 pump=no authority=no stats=1/1/0 ready=1 rejected=0 pumpPlans=0 plannedSteps=1
```

## Validation requise
Comme `src/main.cpp` est modifié :

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Vérifier une commande START puis STOP sur une zone et confirmer que les compteurs progressent sans changement du chemin physique existant.
