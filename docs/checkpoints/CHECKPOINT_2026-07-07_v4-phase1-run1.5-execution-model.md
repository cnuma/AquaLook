# AquaLook V4 — Checkpoint Phase 1 Run 1.5 — Modèle Execution

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/relay-board-mapping`  
**Base de départ :** `5896529014368d2db82ae8d8397bdbee597ca9bd`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Introduire un modèle `EquipmentExecution` compact et isolé pour suivre une opération issue d’une intention acceptée.

## 2. Fichiers source créés

```text
src/domain/ExecutionModel.h
src/domain/ExecutionModel.cpp
```

## 3. Structure Execution

`EquipmentExecution` contient :

```text
état demandé
création
démarrage
échéance
fin
ExecutionId
IntentId source
EquipmentId cible
CorrelationId
erreur
statut
étape
flags
révision
```

Taille verrouillée :

```text
sizeof(EquipmentExecution) = 40 octets
```

## 4. Statuts

```text
CREATED
RUNNING
SUCCEEDED
FAILED
CANCELLED
TIMED_OUT
COMPENSATING
COMPENSATED
```

## 5. Étapes

```text
PREPARE
AUTHORIZE
APPLY
OBSERVE
FINALIZE
COMPENSATE
```

Les étapes ordinaires progressent vers l’avant uniquement pendant `RUNNING`.

## 6. Création depuis Intent

`createExecutionFromIntent()` copie :

- `IntentId` ;
- cible ;
- corrélation ;
- état demandé ;
- exigence d’observation ;
- timeout fourni.

L’intention source reste indépendante.

## 7. Annulation

Deux opérations distinctes :

```text
requestCancellation()
markExecutionCancelled()
```

La demande seule ne déclare pas l’opération annulée.

## 8. Timeout

- temps monotone ;
- compatible rebouclage de `millis()` ;
- `deadlineAtMs == 0` signifie aucun timeout automatique ;
- `markExecutionTimedOut()` produit `OperationError::TIMEOUT`.

## 9. Compensation

```text
FAILED / TIMED_OUT
-> COMPENSATING
-> COMPENSATED
```

Une compensation réussie ne transforme pas l’opération initiale en succès.

## 10. Résultat final

`makeOperationResult()` convertit les états terminaux vers le modèle du Run 1.3 :

```text
SUCCEEDED   -> APPLIED
FAILED      -> FAILED
CANCELLED   -> CANCELLED
TIMED_OUT   -> TIMED_OUT
COMPENSATED -> FAILED + détail compensation
```

## 11. Validation hôte

Compilation de contrôle :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Cas vérifiés :

- taille exacte de 40 octets ;
- passage CREATED vers RUNNING ;
- expiration à l’échéance ;
- timeout ;
- compensation ;
- clôture compensée ;
- demande et confirmation d’annulation.

Résultat :

```text
Compilation hôte OK
EquipmentExecution = 40 octets
```

## 12. Compilation PlatformIO

Non exécutée, poste local indisponible.

Commande restant à réaliser :

```text
pio run -e ProgrammeArrosage
```

## 13. Documentation créée ou modifiée

```text
docs/architecture/adr/ADR-0009-execution-lifecycle.md
docs/architecture/AQUALOOK_V4_EXECUTION_MODEL.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.5-execution-model.md
```

## 14. Fichiers volontairement non modifiés

- `src/main.cpp` ;
- `src/ConfigManager.*` ;
- `src/ScheduleManager.*` ;
- `src/RelaisManager.*` ;
- `src/RelayTopology.*` ;
- `src/WebManager.*` ;
- `src/DisplayManager.*` ;
- `src/domain/EquipmentModel.*` ;
- `src/domain/EquipmentRuntimeState.*` ;
- `src/domain/IntentModel.*` ;
- `platformio.ini` ;
- format NVS ;
- ressources Web et LCD.

## 15. Comportement runtime

Aucun changement.

Le modèle Execution n’est relié à aucun chemin exécuté du firmware actuel.

## 16. Risques et limites

- pas de registre borné d’exécutions ;
- pas d’orchestrateur ;
- pas de retry ;
- pas de politique de compensation par type ;
- pas de dépendances ;
- pas d’intégration actionneur ;
- compilation ESP32 complète à confirmer.

## 17. Invariants préservés

1. Une exécution reste distincte de l’intention.
2. Elle cible un `EquipmentId`.
3. Une demande d’annulation n’est pas une annulation confirmée.
4. Timeout et durée utilisent un temps monotone.
5. Compensation et succès restent distincts.
6. Aucun `String`, pointeur ou allocation dynamique.
7. NVS, planning, relais, Web et LCD inchangés.
8. Aucun effet matériel.

## 18. Prochaine action unique

Démarrer **Phase 1 — Run 1.6 — Dépendances et détection des cycles**.

Le run devra définir :

- relation entre équipements ;
- types de dépendances ;
- conditions d’activation et d’arrêt ;
- exclusivité ;
- ordre ;
- validation des références ;
- détection des cycles ;
- aucune intégration au runtime historique.
