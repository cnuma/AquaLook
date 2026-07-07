# AquaLook V4 — Modèle runtime des états, défauts et résultats

**Statut :** référence de Phase 1  
**Date :** 7 juillet 2026  
**Run :** Phase 1 — Run 1.3

## 1. Objectif

Définir des structures runtime compactes et indépendantes de la configuration `Equipment` afin de distinguer clairement :

```text
ce qui est demandé
ce qui est autorisé
ce qui est appliqué
ce qui est observé
ce qui est en défaut
le résultat d’une opération
```

Aucun de ces éléments ne modifie l’objet `Equipment` de la configuration active.

## 2. Fichiers source

```text
src/domain/EquipmentRuntimeState.h
src/domain/EquipmentRuntimeState.cpp
```

## 3. Séparation générale

```text
ActiveConfigurationArena
└── Equipment immuable

RuntimeExecutionArena future
├── EquipmentRuntimeState
├── EquipmentFault
└── OperationResult
```

L’état runtime référence l’équipement uniquement par `EquipmentId`.

## 4. Les quatre états

### Requested

État demandé par une intention, une commande manuelle ou un automatisme.

Il ne signifie pas que la demande est autorisée.

### Authorized

État retenu après arbitrage, contrôle du mode, des dépendances, des interlocks et des défauts bloquants.

Un refus peut conduire à un état autorisé différent ou à l’absence d’autorisation.

### Applied

État que l’adaptateur d’actionneur confirme avoir envoyé ou appliqué.

Il ne prouve pas que l’équipement physique a réellement atteint cet état.

### Observed

État fourni par un capteur, un retour de position, un contact auxiliaire ou une autre observation indépendante.

Un équipement dépourvu de feedback utilise explicitement :

```text
StateValidity::NOT_SUPPORTED
```

## 5. Valeur générique

`EquipmentStateValue` occupe 8 octets et contient :

```text
int32_t value
StateValueKind kind
StateValidity validity
uint16_t reserved
```

Le modèle initial supporte :

```text
BINARY
PERCENT
POSITION
ENUMERATED
RAW_SIGNED
```

Exemples :

```text
vanne fermée : kind=BINARY, value=0
vanne ouverte : kind=BINARY, value=1
ouvrant à 35 % : kind=PERCENT, value=35
position codée : kind=ENUMERATED, value=<code>
```

Le type d’équipement et ses capacités déterminent quelles représentations sont valides.

## 6. Validité

```text
UNKNOWN
VALID
STALE
INVALID
NOT_SUPPORTED
```

`STALE` signifie qu’une valeur était connue mais n’est plus suffisamment fraîche.

`INVALID` signifie qu’une observation existe mais n’est pas exploitable.

`NOT_SUPPORTED` signifie que la couche concernée ne fournit jamais cette information.

## 7. Structure runtime

`EquipmentRuntimeState` contient :

```text
EquipmentId
revision
requested
authorized
applied
observed
horodatage monotone de chaque étape
EquipmentHealth
flags runtime
nombre de défauts actifs
```

La taille est limitée par assertion à 56 octets maximum.

Les flags initiaux représentent :

```text
AUTHORIZED
COMMAND_PENDING
OBSERVATION_EXPECTED
INTERLOCKED
```

## 8. Révision

Toute modification via les fonctions `record*State()` incrémente une révision 16 bits.

Cette révision permet :

- de détecter un changement pour une interface ;
- d’éviter certains rafraîchissements inutiles ;
- de comparer deux lectures rapprochées.

Elle n’est pas persistée et ne remplace pas un horodatage.

## 9. Convergence

`isConverged()` applique la règle :

```text
si observed est supporté et valide : applied == observed
si observed est NOT_SUPPORTED : authorized == applied
sinon : non convergé
```

Cette fonction constitue une primitive, pas encore une politique complète de timeout ou de tolérance analogique.

Pour les valeurs analogiques, une future comparaison spécifique au type pourra remplacer l’égalité stricte.

## 10. Santé

`EquipmentHealth` fournit une synthèse :

```text
UNKNOWN
HEALTHY
DEGRADED
FAULTED
UNAVAILABLE
```

Le calcul automatique de la santé n’est pas implémenté dans ce run.

Il devra tenir compte des défauts actifs, de la fraîcheur des observations et de la disponibilité des actionneurs.

## 11. Défauts

`EquipmentFault` occupe 16 octets et contient :

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

Les fonctions disponibles sont :

```text
activateFault()
clearFault()
isFaultActive()
hasBlockingFault()
```

`activateFault()` :

- initialise la première apparition lors du premier passage actif ;
- incrémente le compteur jusqu’à 255 lors des répétitions ;
- actualise la dernière apparition ;
- retire l’acquittement lors d’une nouvelle occurrence.

`clearFault()` retire uniquement le flag actif et conserve l’historique compact.

## 12. Résultats d’opération

`OperationResult` occupe 16 octets et contient :

```text
ExecutionId
EquipmentId
OperationStatus
OperationStage
OperationError
completedAtMs
detail
```

Le champ `detail` peut contenir une information numérique contextuelle : code driver, durée mesurée, index de dépendance ou autre valeur définie par le contrat de l’erreur.

Il ne contient jamais un pointeur ni un texte.

## 13. Horodatages

Tous les timestamps du modèle sont des valeurs monotones `uint32_t` en millisecondes.

Les soustractions devront utiliser l’arithmétique non signée compatible avec le rebouclage de `millis()`.

L’heure civile sera ajoutée uniquement lors de la journalisation ou de la présentation.

## 14. Fonctions sans effet extérieur

Les fonctions du modèle :

- ne journalisent rien ;
- ne publient aucun événement ;
- n’accèdent à aucun matériel ;
- n’allouent aucune mémoire ;
- ne dépendent pas d’Arduino.

Elles modifient uniquement les structures explicitement fournies.

## 15. Hors périmètre

Le Run 1.3 n’ajoute pas :

- de registre dynamique d’états ;
- de file de résultats ;
- de table de défauts ;
- de calcul automatique de santé ;
- de timeout de convergence ;
- de tolérance analogique ;
- d’acquittement complet ;
- de mapping vers `FaultManager` ;
- d’intégration avec `RelaisManager` ;
- de modification du runtime historique.

## 16. Intégration future

```text
Intent
  -> arbitrage
  -> recordRequestedState
  -> recordAuthorizedState
  -> actionneur
  -> recordAppliedState
  -> observation
  -> recordObservedState
  -> convergence / défaut / résultat
```

Le prochain modèle d’intention utilisera ces primitives sans les modifier.

## 17. Critères de réussite

Le modèle est acceptable si :

1. une demande ne peut pas être confondue avec une commande appliquée ;
2. une commande appliquée ne peut pas être confondue avec une observation ;
3. les équipements sans feedback sont explicitement identifiés ;
4. défaut et résultat restent séparés ;
5. aucune chaîne dynamique n’est utilisée ;
6. les structures restent bornées et mesurables ;
7. aucune dépendance au runtime actuel n’est ajoutée.
