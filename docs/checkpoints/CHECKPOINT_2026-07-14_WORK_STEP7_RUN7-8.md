# CHECKPOINT — Phase 7 / RUN7.8 — Audit passif orchestrateur ↔ plan shadow

Date : 14 juillet 2026  
Branche : `work/step7-run7-8`

## Objectif

Ajouter un contrôle de cohérence passif entre :

- l'aperçu calculé par `EquipmentOrchestrator` ;
- le `ZoneExecutionPlan` construit par le gestionnaire shadow réellement utilisé par le runtime.

RUN7.8 ne change pas l'autorité de commande : `authority=no` reste obligatoire.

## Base validée

RUN7.8 part de RUN7.7, validé par compilation et test matériel :

- orchestrateur shadow initialisé ;
- zones 1 et 2 START/STOP validées ;
- zone 1 via `physical_backend` ;
- zone 2 via `relay_manager_fallback` ;
- moteur shadow en `SUCCEEDED` ;
- horodatage NTP série et Web validé.

## Modification

Le script `tools/run7-8/apply_run7_8.ps1` modifie uniquement `src/main.cpp`, dans `onRelayRequest()`, après le log d'observation RUN7.7.

L'audit compare :

- validité du plan ;
- `ActionResult` ;
- nombre d'étapes ;
- présence ou absence d'une pompe ;
- intention START/STOP par le chemin commun `state`.

Le détail des actions et leur ordre ne sont pas encore exposés par `EquipmentOrchestrator::Preview`; ils ne sont donc pas comparés dans ce run.

## Logs attendus

Cas cohérent :

```text
Orchestrator audit: zone=1 intent=START match=yes preview=0/1/no_pump shadow=0/1/no_pump authority=no
```

Cas divergent :

```text
Orchestrator audit: zone=1 intent=START match=no preview=0/1/no_pump shadow=0/3/pump authority=no
```

Une divergence produit un avertissement mais ne bloque aucune commande.

## Invariants

- aucune exécution via `EquipmentOrchestrator::executeStartZone()` ou `executeStopZone()` ;
- aucun changement de commande physique ;
- aucun changement NVS ;
- fallback relais conservé ;
- moteur shadow existant conservé ;
- `authority=no` dans tous les logs RUN7.8.

## Validation requise

```powershell
git fetch --prune
git switch work/step7-run7-8
git pull --ff-only
powershell -ExecutionPolicy Bypass -File tools/run7-8/apply_run7_8.ps1
git diff --check
git diff -- src/main.cpp

pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Tester START/STOP sur les zones 1 et 2. Tous les audits attendus doivent être `match=yes` et le comportement matériel doit rester identique à RUN7.7.

## État de validation

- préparation Git : réalisée ;
- compilation : à réaliser localement ;
- upload : à réaliser localement ;
- monitoring : à réaliser localement ;
- test matériel : à réaliser localement.
