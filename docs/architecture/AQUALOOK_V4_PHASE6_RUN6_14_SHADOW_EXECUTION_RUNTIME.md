# AquaLook V4 — Phase 6 — Run 6.14

## Objet

Activer le moteur d’exécution passif dans le firmware nominal en mode shadow.

Le moteur reçoit les mêmes plans que le dry-run, progresse depuis la boucle principale et journalise les transitions Activity/Execution, mais ne commande aucun équipement.

## Base

- branche : `feature/aqualook-v4-domain`
- base : `976942eda40f16fdd8bddd7960507a74e280dd6e`
- Run 6.12 : compilation V4 et legacy validée
- Run 6.13 : banc logiciel validé avec `35/35`, aucun échec

## Architecture

Un moteur passif est alloué par zone :

```text
Schedule callback
    ├─ construit le plan shadow
    ├─ soumet le plan au moteur de la zone
    └─ conserve le chemin Equipment existant

loop()
    ├─ executionShadowRuntime.update(millis())
    └─ runtime et relais existants inchangés
```

Ce choix préserve le parallélisme des zones. Un moteur global unique aurait empêché l’observation simultanée de plusieurs activités.

## Identifiants

Chaque soumission reçoit :

- un `WorkflowId` transitoire dérivé de la zone ;
- un `ActivityId` monotone ;
- un `ExecutionId` monotone.

Format de journal :

```text
[Activity#1][Exec#1] Shadow: ...
```

Les compteurs évitent les valeurs invalides `0` et `0xFFFF`.

## Invariant de sécurité

Le shadow runtime :

- ne possède aucun `RelaisManager` ;
- ne possède aucun `EquipmentOutputRuntimeAdapter` ;
- ne possède aucun backend physique ;
- ne modifie pas le résultat de `startZone()` ou `stopZone()` ;
- ne remplace pas le dry-run du Run 6.11 ;
- ne commande ni vanne ni pompe ;
- ne bloque pas la boucle principale.

Le chemin matériel existant reste exécuté par `EquipmentManager` après la soumission shadow.

## Gestion du parallélisme

`EquipmentExecutionShadowRuntime` contient un `ZoneSlot` par zone jusqu’à `MAX_ZONES`.

Chaque zone peut donc progresser indépendamment dans les états :

```text
READY → RUNNING → WAITING → RUNNING → SUCCEEDED
```

Si un nouveau plan arrive pour une zone dont le plan précédent est encore actif, l’ancien plan shadow est annulé puis remplacé. Cette opération n’a aucun effet sur le runtime matériel.

## Journaux attendus sans pompe

Au démarrage :

```text
Shadow engine: actif pour N zone(s), passive=yes
```

Pour un démarrage de zone :

```text
[Activity#1][Exec#1] Shadow: zone 1 START accepted=yes steps=1 pump=no passive=yes
[Activity#1][Exec#1] Shadow: zone 1 state=RUNNING passive=yes
[Activity#1][Exec#1] Shadow: zone 1 step=1/1 action=VALVE_ON consumed passive=yes
[Activity#1][Exec#1] Shadow: zone 1 state=SUCCEEDED duration=... passive=yes
```

Les journaux dry-run et Equipment existants doivent rester présents.

## Fichiers modifiés

- `src/EquipmentExecutionShadowRuntime.h`
- `src/EquipmentExecutionShadowRuntime.cpp`
- `src/main.cpp`
- `docs/architecture/AQUALOOK_V4_PHASE6_RUN6_14_SHADOW_EXECUTION_RUNTIME.md`

## Éléments volontairement non modifiés

- `EquipmentManager` ;
- `EquipmentExecutionEngine` ;
- `ScheduleManager` ;
- `RelaisManager` ;
- `EquipmentOutputRuntimeAdapter` ;
- `V4PilotRuntime` ;
- NVS ;
- Web ;
- LCD ;
- modèle transitoire des pompes.

## Validation attendue

1. compiler `ProgrammeArrosage_v4` ;
2. compiler `ProgrammeArrosage_legacy` ;
3. téléverser la V4 ;
4. vérifier le journal d’initialisation shadow ;
5. lancer une zone manuellement ;
6. vérifier `START`, `RUNNING`, consommation passive et `SUCCEEDED` ;
7. attendre l’arrêt et vérifier la séquence `STOP` ;
8. confirmer que les journaux Equipment et relais restent inchangés ;
9. confirmer l’absence de crash et de commande pompe.

## Critère de réussite

Le Run 6.14 est validé si le moteur shadow observe un cycle START/STOP complet, tandis que le chemin matériel historique conserve exactement son comportement précédent.
