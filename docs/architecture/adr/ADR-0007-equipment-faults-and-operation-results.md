# ADR-0007 — Défauts et résultats d’opération des équipements

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.3

## Contexte

AquaLook dispose déjà de `FaultManager` et `EventLog`, mais ces mécanismes techniques ne représentent pas encore un défaut métier complet lié à un équipement ni le résultat structuré d’une opération.

La V4 doit pouvoir répondre précisément :

- quelle cible est concernée ;
- à quelle étape l’opération s’est arrêtée ;
- quelle erreur a été rencontrée ;
- si le défaut est actif, bloquant, acquitté ou mémorisé ;
- quand le défaut est apparu et a été vu pour la dernière fois.

## Décision

Deux structures distinctes sont introduites :

```text
EquipmentFault
OperationResult
```

### EquipmentFault

Un défaut représente une condition durable ou répétée associée à un équipement.

Il contient :

```text
EquipmentId
code de défaut
FaultDomain
FaultSeverity
flags
compteur d’occurrences
première apparition monotone
dernière apparition monotone
```

Domaines initiaux :

```text
CONFIGURATION
AUTHORIZATION
ACTUATOR
SENSOR
COMMUNICATION
SAFETY
INTERNAL
```

Sévérités :

```text
INFO
WARNING
ERROR
CRITICAL
```

Flags :

```text
ACTIVE
ACKNOWLEDGED
BLOCKING
LATCHED
```

La structure ne contient pas de texte. Le code et le domaine sont traduits ultérieurement par les couches de présentation ou de journalisation.

### OperationResult

Un résultat décrit l’issue d’une opération ponctuelle.

Il contient :

```text
ExecutionId
EquipmentId
OperationStatus
OperationStage
OperationError
horodatage monotone de fin
champ detail numérique
```

Étapes initiales :

```text
REQUEST
AUTHORIZATION
APPLICATION
OBSERVATION
COMPENSATION
```

Statuts initiaux :

```text
ACCEPTED
REJECTED
APPLIED
FAILED
CANCELLED
TIMED_OUT
```

Erreurs initiales :

```text
INVALID_TARGET
INVALID_STATE
DISABLED
INTERLOCKED
DEPENDENCY_UNAVAILABLE
CAPABILITY_NOT_SUPPORTED
ACTUATOR_UNAVAILABLE
COMMUNICATION_ERROR
OBSERVATION_MISMATCH
TIMEOUT
INTERNAL_ERROR
```

## Séparation défaut / résultat

Un résultat est ponctuel et lié à une exécution.

Un défaut est une condition suivie dans le temps et peut survivre à plusieurs opérations.

Exemple :

```text
résultat : commande pompe échouée à l’étape APPLICATION
cause : COMMUNICATION_ERROR

défaut : communication pompe active, bloquante, occurrence 3
```

## Temps

Les timestamps de ce modèle sont monotones en millisecondes.

L’heure civile peut être ajoutée lors de la journalisation, mais ne doit pas piloter les durées, répétitions ou expirations.

## Acquittement et disparition

- acquitter ne supprime pas le caractère actif ;
- effacer le défaut retire `ACTIVE` mais conserve le dernier instant observé ;
- un défaut `LATCHED` peut exiger une politique spécifique avant disparition ;
- le Run 1.3 fournit uniquement les primitives, pas la politique complète.

## Compatibilité avec l’existant

`FaultManager` et `EventLog` restent inchangés.

Dans une phase future, ils pourront devenir des adaptateurs ou consommateurs de `EquipmentFault` et `OperationResult`.

## Options rejetées

### Un texte libre comme erreur

Rejeté : coût mémoire, absence de contrat et difficulté d’exploitation par l’API.

### Un booléen `error`

Rejeté : ne précise ni étape, ni domaine, ni caractère bloquant.

### Fusionner défaut et résultat

Rejeté : cycles de vie différents.

## Conséquences

- défauts et résultats sont compacts et sérialisables ;
- les interfaces pourront afficher une cause structurée ;
- les politiques d’acquittement et de latched restent à définir ;
- un registre borné de défauts et une file bornée de résultats seront nécessaires plus tard ;
- les textes restent hors des structures runtime.

## Invariants

1. Un résultat n’est jamais utilisé comme état durable.
2. Un défaut actif n’est pas effacé par un simple acquittement.
3. Un défaut bloquant doit être pris en compte par l’autorisation future.
4. Les codes d’erreur sont stables et versionnés.
5. Aucun `String` ou pointeur n’est contenu dans ces structures.
