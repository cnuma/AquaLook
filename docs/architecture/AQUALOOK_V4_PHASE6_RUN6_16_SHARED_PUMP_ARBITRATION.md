# AquaLook V4 — Phase 6 — Run 6.16

## Objet

Ajouter un arbitrage passif pour une pompe partagée entre plusieurs zones.

## Base

- branche : `feature/aqualook-v4-domain`
- base : `bd8793d32fd94391f464cee158f140627e9f092d`
- Run 6.15 : plans pompe shadow validés avec `VALVE`, `WAIT`, `PUMP`

## Problème traité

Sans arbitrage, chaque plan STOP de zone contient `PUMP_OFF`. Une zone peut donc demander conceptuellement l’arrêt de la pompe alors qu’une autre zone l’utilise encore.

## Principe

Le shadow runtime conserve :

- un indicateur `pumpRequested` par zone ;
- un compteur global `_sharedPumpUsers` ;
- le plan source produit par `EquipmentManager` ;
- un plan effectif filtré selon l’état du compteur.

### Première acquisition

```text
users 0 -> 1
VALVE_ON
WAIT
PUMP_ON
```

### Acquisition supplémentaire

```text
users 1 -> 2
VALVE_ON
```

La pompe est considérée comme déjà active. `WAIT` et `PUMP_ON` sont retirés du plan effectif.

### Libération non finale

```text
users 2 -> 1
VALVE_OFF
```

`PUMP_OFF` et son `WAIT` sont retirés.

### Dernière libération

```text
users 1 -> 0
PUMP_OFF
WAIT
VALVE_OFF
```

## Invariant de sécurité

L’arbitrage reste entièrement shadow :

- aucune commande de pompe réelle ;
- aucun accès à `RelaisManager` ;
- aucun accès au backend physique ;
- aucune modification NVS ;
- le chemin fonctionnel de vanne reste inchangé ;
- la carte 2 relais ne doit recevoir aucune nouvelle commande pompe.

## Fichiers modifiés

- `src/EquipmentExecutionShadowRuntime.h`
- `src/EquipmentExecutionShadowRuntime.cpp`
- `docs/architecture/AQUALOOK_V4_PHASE6_RUN6_16_SHARED_PUMP_ARBITRATION.md`

## Validation attendue

1. démarrer la zone 1 ;
2. démarrer la zone 2 avant l’arrêt de la zone 1 ;
3. arrêter la zone 2 ;
4. vérifier qu’aucun `PUMP_OFF` shadow n’est consommé ;
5. arrêter la zone 1 ;
6. vérifier que le dernier STOP consomme `PUMP_OFF`, `WAIT`, puis `VALVE_OFF`.

Les journaux d’arbitrage attendus sont :

```text
Shadow pump arbiter: zone 1 ACQUIRE users=0->1 transition=PUMP_ON passive=yes
Shadow pump arbiter: zone 2 ACQUIRE users=1->2 transition=KEEP_ON passive=yes
Shadow pump arbiter: zone 2 RELEASE users=2->1 transition=KEEP_ON passive=yes
Shadow pump arbiter: zone 1 RELEASE users=1->0 transition=PUMP_OFF passive=yes
```
