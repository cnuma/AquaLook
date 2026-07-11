# AquaLook V4 — Phase 6 — Run 6.15

## Objet

Exercer dans le firmware nominal les plans complets avec dépendance pompe, attente de démarrage et attente d’arrêt, tout en conservant une séparation stricte avec le modèle fonctionnel et le matériel.

## Base

- branche : `feature/aqualook-v4-domain`
- base : `2014e99ff6b3f0bcd90b9f6edd891fa4ecc2fcfc`
- Run 6.14 : shadow runtime validé sur START et STOP
- carte relais absente lors de la validation, fallback correctement observé

## Principe

Le modèle fonctionnel `transientEquipmentModel` reste inchangé et continue de ne déclarer aucune pompe.

Le Run 6.15 crée une copie dédiée au shadow :

```text
shadowEquipmentModel
shadowRelayTopology
shadowEquipmentMgr
```

Une pompe synthétique est ajoutée uniquement si :

- un emplacement équipement est libre ;
- une affectation relais est libre ;
- une voie libre existe sur une carte déclarée valide.

La pompe synthétique utilise :

```text
startupDelayMs  = 500
shutdownDelayMs = 500
```

Toutes les zones du modèle shadow dépendent de cette pompe.

## Invariant de sécurité

La topologie shadow est une copie en mémoire.

Elle n’est jamais transmise à :

- `RelaisManager` ;
- `EquipmentOutputRuntimeAdapter` ;
- `V4PilotRuntime` ;
- un driver GPIO ou I2C ;
- un backend physique.

`shadowEquipmentMgr` ne reçoit aucun exécuteur.

Le chemin fonctionnel continue d’utiliser `equipmentMgr`, qui conserve son modèle sans pompe.

## Plans attendus

### Démarrage

```text
VALVE_ON
WAIT 500 ms
PUMP_ON
```

### Arrêt

```text
PUMP_OFF
WAIT 500 ms
VALVE_OFF
```

Ces actions sont consommées uniquement par `EquipmentExecutionShadowRuntime`.

## Fichiers modifiés

- `src/main.cpp`
- `docs/architecture/AQUALOOK_V4_PHASE6_RUN6_15_SYNTHETIC_PUMP_SHADOW_SCENARIO.md`

## Éléments non modifiés

- NVS ;
- `ConfigManager` ;
- `EquipmentManager` ;
- `EquipmentExecutionEngine` ;
- `EquipmentExecutionShadowRuntime` ;
- `ScheduleManager` ;
- `RelaisManager` ;
- backend V4 ;
- Web ;
- LCD.

## Validation attendue

1. compiler `ProgrammeArrosage_v4` ;
2. compiler `ProgrammeArrosage_legacy` ;
3. téléverser la V4 ;
4. vérifier au démarrage que le scénario pompe shadow est prêt ;
5. lancer une zone manuellement ;
6. vérifier START : `VALVE_ON`, `WAIT`, `PUMP_ON` ;
7. vérifier STOP : `PUMP_OFF`, `WAIT`, `VALVE_OFF` ;
8. confirmer que les logs `Equipment plan` fonctionnels indiquent toujours `pump=no` ;
9. confirmer qu’aucune commande relais pompe n’apparaît.

## Critère de réussite

Le Run 6.15 est validé si les plans pompe complets s’exécutent en shadow avec les délais attendus, tandis que le comportement réel des vannes reste strictement celui du Run 6.14.
