# AquaLook V4 — Modèle Execution

**Statut :** référence de Phase 1  
**Date :** 7 juillet 2026  
**Run :** Phase 1 — Run 1.5

## 1. Position dans l’architecture

```text
EquipmentIntent accepté
        ↓
EquipmentExecution
        ↓
Orchestrateur futur
        ↓
Actionneur futur
        ↓
EquipmentRuntimeState
        ↓
OperationResult
```

L’exécution représente l’opération suivie. Elle reste indépendante du driver matériel.

## 2. Fichiers source

```text
src/domain/ExecutionModel.h
src/domain/ExecutionModel.cpp
```

## 3. Structure

`EquipmentExecution` occupe exactement 40 octets :

```text
requestedState      8 octets
createdAtMs         4 octets
startedAtMs         4 octets
deadlineAtMs        4 octets
completedAtMs       4 octets
ExecutionId         2 octets
IntentId            2 octets
EquipmentId         2 octets
CorrelationId       2 octets
OperationError      2 octets
status              1 octet
step                1 octet
flags               1 octet
revision            1 octet
```

## 4. Création depuis une intention

`createExecutionFromIntent()` reprend :

- l’intention source ;
- la cible ;
- la corrélation ;
- l’état demandé ;
- l’exigence d’observation ;
- une durée de timeout fournie par l’orchestrateur.

L’intention n’est pas modifiée.

## 5. Cycle de vie

```text
CREATED
  ├── RUNNING
  └── CANCELLED

RUNNING
  ├── SUCCEEDED
  ├── FAILED
  ├── CANCELLED
  ├── TIMED_OUT
  └── COMPENSATING

FAILED / TIMED_OUT
  └── COMPENSATING

COMPENSATING
  ├── COMPENSATED
  └── FAILED
```

États terminaux :

```text
SUCCEEDED
FAILED
CANCELLED
TIMED_OUT
COMPENSATED
```

Une exécution `FAILED` ou `TIMED_OUT` peut néanmoins être reprise pour compensation avant archivage définitif.

## 6. Étapes ordinaires

```text
PREPARE
AUTHORIZE
APPLY
OBSERVE
FINALIZE
```

`advanceExecutionStep()` interdit :

- le retour arrière ;
- l’avance pendant un statut autre que `RUNNING` ;
- l’entrée directe dans `COMPENSATE`.

La compensation utilise `beginCompensation()`.

## 7. Annulation

```text
requestCancellation()
```

positionne `CANCELLATION_REQUESTED`.

```text
markExecutionCancelled()
```

confirme l’arrêt et produit l’état terminal.

Une demande seule ne suffit pas à annoncer l’annulation.

## 8. Timeout

`hasExecutionTimedOut()` compare `nowMs` et `deadlineAtMs` en arithmétique monotone compatible avec le rebouclage de `millis()`.

`markExecutionTimedOut()` :

- passe le statut à `TIMED_OUT` ;
- positionne `OperationError::TIMEOUT` ;
- mémorise la date de fin ;
- rend un résultat disponible.

## 9. Compensation

`beginCompensation()` :

- passe à `COMPENSATING` ;
- positionne l’étape `COMPENSATE` ;
- retire temporairement le flag de résultat prêt.

`markExecutionCompensated()` :

- passe à `COMPENSATED` ;
- mémorise la fin ;
- restaure le résultat final.

Le résultat final reste `FAILED`, avec `detail = 1` pour signaler une compensation réussie.

## 10. Résultat final

`makeOperationResult()` produit un objet de 16 octets déjà défini au Run 1.3.

Correspondances principales :

```text
SUCCEEDED   -> APPLIED
FAILED      -> FAILED
CANCELLED   -> CANCELLED
TIMED_OUT   -> TIMED_OUT
COMPENSATED -> FAILED + détail compensation
```

## 11. Validation

`validateExecution()` contrôle :

- `ExecutionId` ;
- `IntentId` ;
- cible ;
- valeur demandée ;
- statut ;
- étape ;
- échéance.

Elle ne contrôle pas encore :

- que l’intention source soit réellement `ACCEPTED` ;
- l’existence de la cible dans la configuration active ;
- les capacités ;
- la politique de timeout par type ;
- la stratégie concrète de compensation.

Ces contrôles appartiendront à l’orchestrateur.

## 12. Hors périmètre

- registre ou file bornée d’exécutions ;
- ordonnanceur ;
- actionneur ;
- politique de retry ;
- compensation spécifique aux équipements ;
- dépendances ;
- parallélisme ;
- intégration au moteur actuel ;
- persistance.

## 13. Risque identifié

`FAILED` et `TIMED_OUT` sont considérés comme terminaux pour les consommateurs ordinaires, mais peuvent encore entrer en compensation.

Le futur registre devra donc distinguer :

```text
résultat initial disponible
≠
cycle de compensation définitivement clos
```

## 14. Prochaine étape

Le Run 1.6 devra définir les dépendances entre équipements et les règles de détection des cycles avant de construire un orchestrateur multi-équipements.
