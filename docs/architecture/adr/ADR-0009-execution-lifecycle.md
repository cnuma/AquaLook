# ADR-0009 — Cycle de vie d’une exécution

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.5

## Contexte

Une intention acceptée ne doit pas être confondue avec l’opération qui tente réellement d’appliquer la demande.

L’exécution doit suivre les étapes, délais, annulations, erreurs et compensations sans modifier l’intention source ni la configuration `Equipment`.

## Décision

Le modèle introduit `EquipmentExecution` comme objet runtime distinct.

Il contient :

```text
ExecutionId
IntentId source
EquipmentId cible
CorrelationId
état demandé
dates de création, démarrage, échéance et fin
statut
étape courante
flags
erreur
révision
```

Sa taille est verrouillée à 40 octets.

## Statuts

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

## Étapes

```text
PREPARE
AUTHORIZE
APPLY
OBSERVE
FINALIZE
COMPENSATE
```

Les étapes ordinaires progressent uniquement vers l’avant lorsque l’exécution est `RUNNING`.

## Création

`createExecutionFromIntent()` copie uniquement les données nécessaires :

- identifiants ;
- cible ;
- corrélation ;
- état demandé ;
- exigence d’observation ;
- échéance calculée.

L’intention reste un objet séparé.

## Délais

`deadlineAtMs == 0` signifie qu’aucun timeout automatique n’est défini.

Sinon, l’échéance est calculée sur le temps monotone. La durée maximale sûre reste inférieure à la demi-plage d’un compteur 32 bits.

## Annulation

L’annulation comporte deux temps :

```text
requestCancellation()
markExecutionCancelled()
```

La demande positionne un flag. La confirmation fait passer l’exécution à l’état terminal `CANCELLED`.

Cette séparation évite de déclarer une exécution annulée avant que l’actionneur ou l’orchestrateur ait réellement arrêté l’opération.

## Compensation

Après un échec ou un timeout, une compensation peut être lancée :

```text
FAILED ou TIMED_OUT
-> COMPENSATING
-> COMPENSATED
```

La compensation ne transforme pas l’opération initiale en succès. Le résultat final reste un échec, accompagné d’un indicateur précisant que la compensation a abouti.

## Résultat

`makeOperationResult()` transforme un état terminal en `OperationResult` compact.

Le résultat contient :

- l’exécution ;
- la cible ;
- le statut final ;
- l’étape ;
- l’erreur ;
- la date de fin ;
- un détail numérique.

## Options rejetées

### Utiliser directement l’intention comme exécution

Rejeté : cycles de vie, mutations et responsabilités différents.

### Marquer une annulation dès sa demande

Rejeté : absence de confirmation d’arrêt réel.

### Considérer une compensation comme un succès

Rejeté : l’action initiale a échoué même si le système a retrouvé un état sûr.

### Piloter le matériel depuis la machine d’états

Rejeté : l’exécution reste indépendante des drivers et actionneurs.

## Conséquences

- l’orchestrateur futur pourra suivre chaque opération ;
- les timeouts et annulations sont explicites ;
- les résultats peuvent être publiés sans conserver toute l’exécution ;
- les politiques de compensation seront définies par type d’opération ;
- une file bornée d’exécutions sera nécessaire avant intégration runtime.

## Invariants

1. Une exécution provient d’une intention acceptée.
2. Une exécution cible un `EquipmentId`, jamais un port.
3. Une demande d’annulation n’est pas une annulation confirmée.
4. Un timeout utilise un temps monotone.
5. Une compensation réussie ne change pas l’échec initial en succès.
6. Aucun pointeur, `String` ou allocation dynamique n’est contenu dans le modèle.
7. La machine d’états ne commande aucun matériel directement.
