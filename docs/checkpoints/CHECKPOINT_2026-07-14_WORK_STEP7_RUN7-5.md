# CHECKPOINT — AquaLook — RUN7.5 — orchestrateur branché en shadow

Date : 14 juillet 2026

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Base : `work/step7-run7-4`
- Branche : `work/step7-run7-5`

## Objectif

Brancher `EquipmentOrchestrator` dans le runtime nominal en observation uniquement, sans lui donner l'autorité sur les relais.

## Modifications prévues dans `src/main.cpp`

1. Ajouter `EquipmentOrchestrator.h` dans les includes équipements.
2. Ajouter l'instance globale `equipmentOrchestrator` avec les autres composants runtime.
3. Ajouter le drapeau `equipmentOrchestratorShadowReady` à côté des états équipements.
4. Initialiser l'orchestrateur après construction du scénario pompe shadow :
   - source `shadowEquipmentMgr` lorsque le scénario pompe est disponible ;
   - sinon source `equipmentMgr` ;
   - journaliser la source, l'état et `authority=no`.
5. Dans `onRelayRequest()`, demander un aperçu START/STOP à l'orchestrateur et journaliser :
   - zone ;
   - intention ;
   - statut ;
   - résultat du plan ;
   - nombre d'étapes ;
   - dépendance pompe ;
   - `authority=no`.

## Autorité physique inchangée

Le chemin qui commande réellement les sorties reste strictement celui de RUN7.4 :

```text
ScheduleManager
  -> onRelayRequest
      -> EquipmentManager::startZone/stopZone
          -> EquipmentOutputRuntimeAdapter
              -> backend physique / fallback
```

L'orchestrateur ne doit appeler ni `executeStartZone()` ni `executeStopZone()` dans RUN7.5.

## Application locale contrôlée

Le dépôt contient :

```text
tools/run7-5/apply_run7_5.ps1
```

Le script :

- refuse de fonctionner hors de `work/step7-run7-5` ;
- refuse un working tree non propre ;
- vérifie que chaque point d'insertion existe une seule fois ;
- modifie uniquement `src/main.cpp` ;
- exécute `git diff --check` ;
- affiche le diff ;
- ne crée aucun commit automatiquement.

Commandes :

```powershell
git fetch --prune
git switch work/step7-run7-5
git pull --ff-only
powershell -ExecutionPolicy Bypass -File tools/run7-5/apply_run7_5.ps1
```

## Validation obligatoire RUN7.5

RUN7.5 modifie `src/main.cpp`. La validation complète AquaLook est donc nécessaire :

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

## Logs attendus

Au démarrage :

```text
Orchestrator shadow: status=ready source=runtime_model authority=no zones=N
```

ou, lorsque le scénario pompe shadow est actif :

```text
Orchestrator shadow: status=ready source=pump_shadow authority=no zones=N
```

Lors d'une demande de zone :

```text
Orchestrator shadow: zone=1 intent=START status=0 plan=0 steps=... pump=... authority=no
```

Puis les logs existants d'exécution réelle doivent rester présents et les relais doivent fonctionner comme avant.

## Tests matériels à confirmer

- démarrage normal ;
- écran et tactile opérationnels ;
- serveur Web opérationnel ;
- zone 1 ON/OFF fonctionnelle ;
- autre zone ON/OFF fonctionnelle si disponible ;
- présence des logs `Orchestrator shadow` ;
- absence de double commande physique ;
- absence de régression du fallback.

## Statut

Le script de modification et le checkpoint sont versionnés. Le changement de `src/main.cpp`, la compilation, l'upload et la validation matérielle restent à exécuter localement.
