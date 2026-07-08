# ADR-0021 — Bootstrap non-runtime du registre de drivers binaires

- **Statut :** Acceptée
- **Date :** 8 juillet 2026
- **Phase :** V4 Phase 3 — Run 3.6

## Contexte

Les drivers binaires existent désormais séparément : simulé, GPIO et XL9535. Il faut consolider leur stratégie d’enregistrement sans encore modifier le firmware actif.

## Décision

Un bootstrap explicite est ajouté :

```text
BinaryActuatorDriverBootstrapPlan
bootstrapBinaryActuatorDrivers()
```

Il reçoit :

- un registre existant ;
- un plan contenant les contextes disponibles ;
- un masque de drivers demandés.

Il ne crée aucun objet global et ne s’exécute pas seul.

## Drivers couverts

```text
BOOTSTRAP_DRIVER_SIMULATED
BOOTSTRAP_DRIVER_GPIO
BOOTSTRAP_DRIVER_XL9535
```

GPIO est disponible uniquement lorsque :

```text
AQUALOOK_V4_ENABLE_GPIO=1
```

XL9535 est disponible uniquement lorsque :

```text
AQUALOOK_V4_ENABLE_I2C=1
```

## Règle d’instanciation

L’appelant doit fournir les contextes.

Le bootstrap refuse :

- un contexte manquant ;
- un driver incomplet ;
- un doublon de type de contrôleur ;
- un registre trop petit.

## Non-runtime

Le bootstrap n’est appelé par aucun fichier runtime dans ce run.

Aucun changement dans :

```text
main.cpp
RelaisManager
RelayTopology
ConfigManager
ScheduleManager
NVS
Web
LCD
```

## Options rejetées

### Registre global statique

Rejeté : risque d’initialisation implicite et de couplage prématuré au firmware.

### Enregistrement automatique au démarrage

Rejeté : trop tôt avant la stratégie runtime, NVS et migration effective.

### Allocation dynamique du registre

Rejetée : capacité non déterministe et fragmentation inutile.

## Validation

Validation hôte du comportement de bootstrap :

```text
Compilation hôte OK
registered=3 requested=3 failures-ok
```

## Invariants

1. Le registre reste borné.
2. Aucun driver n’est instancié par défaut.
3. Les profils compilés limitent les drivers disponibles.
4. Le runtime historique ne change pas.
5. L’intégration réelle reste une phase ultérieure.
