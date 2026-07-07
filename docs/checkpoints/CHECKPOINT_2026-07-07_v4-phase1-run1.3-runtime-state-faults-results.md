# AquaLook V4 — Checkpoint Phase 1 Run 1.3 — États, défauts et résultats

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/relay-board-mapping`  
**Base de départ :** `da0fea4ba919f6b7c19a8620713b41790ae90753`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Introduire des structures runtime compactes distinctes de la configuration `Equipment` pour représenter :

- état demandé ;
- état autorisé ;
- état appliqué ;
- état observé ;
- santé ;
- défaut structuré ;
- résultat d’opération.

Aucune intégration au runtime historique n’est réalisée.

## 2. Fichiers source créés

```text
src/domain/EquipmentRuntimeState.h
src/domain/EquipmentRuntimeState.cpp
```

## 3. Modèle runtime

`EquipmentRuntimeState` référence un équipement par `EquipmentId` et contient quatre valeurs indépendantes :

```text
requested
authorized
applied
observed
```

Chaque valeur contient :

```text
int32_t value
StateValueKind kind
StateValidity validity
```

Chaque étape possède un timestamp monotone `uint32_t`.

## 4. Valeurs et validités

Types de valeurs :

```text
UNKNOWN
BINARY
PERCENT
POSITION
ENUMERATED
RAW_SIGNED
```

Validités :

```text
UNKNOWN
VALID
STALE
INVALID
NOT_SUPPORTED
```

`NOT_SUPPORTED` permet de représenter explicitement un équipement sans retour d’état.

## 5. Santé et flags

Santé :

```text
UNKNOWN
HEALTHY
DEGRADED
FAULTED
UNAVAILABLE
```

Flags runtime :

```text
AUTHORIZED
COMMAND_PENDING
OBSERVATION_EXPECTED
INTERLOCKED
```

Le calcul automatique de santé reste hors périmètre.

## 6. Défauts

`EquipmentFault` contient :

```text
EquipmentId
code
FaultDomain
FaultSeverity
flags
occurrenceCount
firstSeenAtMs
lastSeenAtMs
```

Fonctions disponibles :

```text
activateFault()
clearFault()
isFaultActive()
hasBlockingFault()
```

Un acquittement futur ne devra pas effacer automatiquement le caractère actif.

## 7. Résultats

`OperationResult` contient :

```text
ExecutionId
EquipmentId
OperationStatus
OperationStage
OperationError
completedAtMs
detail
```

Défaut durable et résultat ponctuel restent séparés.

## 8. Convergence

`isConverged()` applique actuellement :

```text
feedback valide : applied == observed
feedback non supporté : authorized == applied
sinon : non convergé
```

La comparaison analogique avec tolérance reste à définir.

## 9. Tailles mesurées

Compilation hôte C++11 :

```text
sizeof(EquipmentRuntimeState) = 56 octets
sizeof(EquipmentFault)        = 16 octets
sizeof(OperationResult)       = 16 octets
sizeof(EquipmentStateValue)   = 8 octets
```

Des assertions de compilation verrouillent ces limites.

## 10. Tests réalisés

Commande équivalente :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Scénario :

- création d’un état runtime ;
- enregistrement requested, authorized, applied et observed ;
- convergence sans feedback via `NOT_SUPPORTED` ;
- activation répétée d’un défaut bloquant ;
- vérification du compteur d’occurrences ;
- effacement du défaut actif ;
- résultat d’opération appliqué.

Résultat :

```text
Compilation hôte OK
56 16 16
```

## 11. Compilation PlatformIO

Non exécutée : le dépôt local de l’utilisateur n’est pas synchronisable actuellement dans cette session.

La compilation complète suivante reste obligatoire ultérieurement :

```text
pio run -e ProgrammeArrosage
```

## 12. Documentation créée ou modifiée

```text
docs/architecture/adr/ADR-0006-equipment-runtime-state-separation.md
docs/architecture/adr/ADR-0007-equipment-faults-and-operation-results.md
docs/architecture/AQUALOOK_V4_RUNTIME_STATE_MODEL.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.3-runtime-state-faults-results.md
```

## 13. Fichiers volontairement non modifiés

- `src/main.cpp` ;
- `src/ConfigManager.*` ;
- `src/ScheduleManager.*` ;
- `src/RelaisManager.*` ;
- `src/RelayTopology.*` ;
- `src/WebManager.*` ;
- `src/DisplayManager.*` ;
- `src/domain/EquipmentModel.*` ;
- `include/config.h` ;
- `platformio.ini` ;
- format NVS ;
- ressources Web et LCD.

## 14. Comportement runtime

Aucun changement.

Les nouveaux fichiers ne sont reliés à aucun chemin exécuté du firmware actuel.

## 15. Invariants préservés

1. `Equipment` reste immuable et à 28 octets.
2. Aucun état mutable n’est stocké dans la configuration active.
3. `requested`, `authorized`, `applied` et `observed` sont distincts.
4. `applied` n’est pas présenté comme une observation physique.
5. Aucun `String`, pointeur ou allocation dynamique n’est ajouté.
6. Les timestamps sont monotones.
7. NVS, planning, relais, Web et LCD restent inchangés.
8. Aucun effet matériel.

## 16. Risques et limites

- comparaison stricte seulement, sans tolérance analogique ;
- absence de registre borné des états ;
- absence de table de défauts ;
- absence de file de résultats ;
- santé non calculée automatiquement ;
- politique latched/acquittement encore incomplète ;
- intégration ESP32 à confirmer par PlatformIO.

## 17. Prochaine action unique

Démarrer **Phase 1 — Run 1.4 — Modèle Intent**.

Le run devra définir :

- identité d’intention ;
- origine ;
- cible ;
- action ou état demandé ;
- priorité ;
- durée de validité ;
- corrélation ;
- statut de traitement ;
- refus structuré ;
- aucun branchement au moteur historique.

## 18. Message de reprise recommandé

```text
Projet AquaLook V4 — Phase 1 — Run 1.4 — Modèle Intent

Base :
- dépôt : cnuma/AquaLook
- branche : feature/relay-board-mapping
- HEAD distant : utiliser le commit exact du checkpoint Run 1.3

Références :
- docs/architecture/AQUALOOK_V4_EQUIPMENT_MODEL.md
- docs/architecture/AQUALOOK_V4_RUNTIME_STATE_MODEL.md
- docs/architecture/adr/ADR-0006-equipment-runtime-state-separation.md
- docs/architecture/adr/ADR-0007-equipment-faults-and-operation-results.md
- docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.3-runtime-state-faults-results.md

État à préserver :
- Equipment immuable ;
- EquipmentRuntimeState = 56 octets ;
- EquipmentFault = 16 octets ;
- OperationResult = 16 octets ;
- requested/authorized/applied/observed distincts ;
- runtime historique inchangé.

Objectif unique :
Créer le modèle Intent isolé avec priorité, origine, validité et résultat d’arbitrage.
```
