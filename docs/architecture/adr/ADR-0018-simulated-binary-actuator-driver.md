# ADR-0018 — Driver binaire simulé

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 3 — Run 3.2

## Contexte

Le contrat des actionneurs binaires doit être éprouvé avant tout accès GPIO ou I²C réel. Un driver mémoire permet de tester les règles de configuration, commande, lecture, état sûr, santé et erreurs sans dépendance matérielle.

## Décision

Un driver simulé est introduit :

```text
SimulatedBinaryActuatorContext
simulatedBinaryActuatorDriverOps()
```

Le contexte conserve :

```text
state
health
configureCount
writeCount
readCount
safeStateCount
faultMask
configured
```

Sa taille est verrouillée à 24 octets.

## Pannes injectables

```text
SIMULATED_FAULT_CONFIGURE
SIMULATED_FAULT_WRITE
SIMULATED_FAULT_READ
SIMULATED_FAULT_SAFE_STATE
SIMULATED_FAULT_UNAVAILABLE
SIMULATED_FAULT_READBACK_MISMATCH
```

Les pannes peuvent être combinées dans un masque.

## Comportements

### Configuration

- incrémente `configureCount` ;
- refuse un contexte absent ;
- refuse un port non binaire ;
- peut simuler une indisponibilité ou une panne interne ;
- marque le contexte configuré en cas de succès.

### Écriture

- exige une configuration préalable ;
- incrémente `writeCount` ;
- peut simuler une indisponibilité ou une erreur de communication ;
- met à jour l’état uniquement en cas de succès.

### Lecture

- exige une configuration préalable ;
- incrémente `readCount` ;
- peut simuler une erreur de readback ;
- peut retourner volontairement un état opposé et passer la santé à `DEGRADED`.

### État sûr

- exige une configuration préalable ;
- incrémente `safeStateCount` ;
- applique `INACTIVE` ou `ACTIVE` selon le port ;
- peut simuler une erreur de communication.

### Santé

La santé reflète :

```text
HEALTHY
DEGRADED
UNAVAILABLE
FAULTED
```

## Options rejetées

### Tester directement avec GPIO

Rejeté : dépendance matérielle trop tôt et diagnostic plus difficile.

### Simuler seulement le chemin nominal

Rejeté : les erreurs et états dégradés font partie du contrat.

### Modifier directement la session depuis le driver

Rejeté : la session reste gérée par la couche générique.

## Conséquences

- le contrat binaire est testable de bout en bout sur hôte ;
- les futurs drivers concrets disposent d’un comportement de référence ;
- les politiques d’erreur peuvent être testées sans carte ;
- aucune dépendance Arduino n’est ajoutée.

## Invariants

1. Le driver simulé n’effectue aucune entrée-sortie.
2. Une panne d’écriture ne modifie pas l’état appliqué de la session.
3. Un readback incohérent dégrade la santé.
4. Les compteurs permettent de vérifier l’idempotence.
5. Le runtime historique reste inchangé.
