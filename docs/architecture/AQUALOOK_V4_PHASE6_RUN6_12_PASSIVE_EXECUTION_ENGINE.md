# AquaLook V4 — Phase 6 — Run 6.12

## Objet

Créer un moteur d’exécution passif capable de consommer les plans produits par `EquipmentManager`, avec une machine d’états non bloquante et une gestion sûre des attentes par horodatage.

## Base

- branche : `feature/aqualook-v4-domain`
- base validée : `158a8f030bf094f80866fcf6c673f54a6310e586`
- Runs 6.10 et 6.11 : compilés, téléversés et validés en dry-run le 11 juillet 2026

## Périmètre

Le Run 6.12 ajoute :

- `WorkflowId` ;
- `ActivityId` ;
- `EquipmentExecutionEngine` ;
- `ExecutionContext` ;
- `PumpContext` ;
- une machine d’états passive ;
- la progression non bloquante des étapes `WAIT` ;
- la gestion d’annulation ;
- la détection des plans et contextes invalides.

## Machine d’états

```text
IDLE
  ↓ load(plan)
READY
  ↓ tick(now)
RUNNING
  ├─ action passive → RUNNING
  ├─ WAIT → WAITING
  ├─ fin du plan → SUCCEEDED
  ├─ erreur → FAILED
  └─ cancel(now) → CANCELLED

WAITING
  ├─ délai non écoulé → WAITING
  ├─ délai écoulé → RUNNING
  └─ cancel(now) → CANCELLED
```

## Invariant de sécurité

Le moteur est totalement passif.

Il ne contient et ne référence aucun :

- `RelaisManager` ;
- `EquipmentOutputRuntimeAdapter` ;
- driver GPIO ;
- driver I2C ;
- backend physique ;
- appel à `millis()` ;
- appel bloquant à `delay()`.

Le temps courant est injecté par `tick(nowMs)`.

Les actions `VALVE_ON`, `VALVE_OFF`, `PUMP_ON` et `PUMP_OFF` sont uniquement consommées dans le contexte interne. Elles ne sont jamais transmises au runtime.

## Gestion de `WAIT`

Une étape `WAIT` mémorise son instant d’entrée dans `waitStartedAtMs`.

La condition de fin utilise :

```cpp
static_cast<uint32_t>(nowMs - waitStartedAtMs) >= delayMs
```

Cette forme reste correcte lors du rebouclage de `millis()`.

## PumpContext

`PumpContext` conserve uniquement l’état planifié :

- dépendance pompe requise ou non ;
- index de l’équipement pompe ;
- dernier état pompe planifié ;
- nombre de transitions pompe consommées.

Il ne représente pas l’état physique réel d’une pompe.

## Intégration

Aucune intégration runtime n’est réalisée dans ce run.

En particulier, les fichiers suivants restent inchangés :

- `src/main.cpp` ;
- `src/EquipmentManager.cpp` ;
- `src/EquipmentManager.h` ;
- `src/ScheduleManager.cpp` ;
- `src/V4PilotRuntime.cpp` ;
- `src/EquipmentOutputRuntimeAdapter.cpp` ;
- `src/RelaisManager.cpp`.

Le moteur n’est donc ni instancié ni appelé par le firmware.

## Fichiers modifiés

- `src/domain/DomainIdentifiers.h`
- `src/EquipmentExecutionEngine.h`
- `src/EquipmentExecutionEngine.cpp`
- `docs/architecture/AQUALOOK_V4_PHASE6_RUN6_12_PASSIVE_EXECUTION_ENGINE.md`

## Validation attendue

1. mettre à jour la branche locale avec `git pull --ff-only` ;
2. vérifier le nouveau HEAD ;
3. compiler `ProgrammeArrosage_v4` ;
4. compiler `ProgrammeArrosage_legacy` ;
5. vérifier l’absence d’erreur de linkage ;
6. téléverser uniquement après réussite des compilations ;
7. confirmer que les logs fonctionnels restent identiques au Run 6.11 ;
8. confirmer qu’aucun nouveau log du moteur n’apparaît, puisqu’il n’est pas connecté.

## Critère de réussite

Le Run 6.12 est validé si le nouveau moteur compile avec le firmware sans modifier le comportement matériel ou fonctionnel observé au Run 6.11.

## Étape suivante

Le run suivant pourra introduire un banc de validation logiciel ou une activation contrôlée du moteur, sans encore lui confier de commande matérielle.
