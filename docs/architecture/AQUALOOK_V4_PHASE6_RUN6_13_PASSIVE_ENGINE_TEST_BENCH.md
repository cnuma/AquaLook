# AquaLook V4 — Phase 6 — Run 6.13

## Objet

Ajouter un banc de validation logiciel dédié au moteur d’exécution passif du Run 6.12.

Le banc s’exécute sur l’ESP32 mais ne charge ni le firmware nominal, ni le runtime AquaLook, ni les drivers de relais.

## Base

- branche : `feature/aqualook-v4-domain`
- base : `60c65a2522d2fd370ea0b1954e42fc98afba672b`
- Run 6.12 : compilation V4 et legacy validée
- comportement nominal après téléversement : inchangé et validé

## Périmètre

Le Run 6.13 ajoute :

- un environnement PlatformIO `test_execution_engine` ;
- un programme autonome `src/test_execution_engine.cpp` ;
- des scénarios déterministes ;
- un bilan série `passed`, `failed` et `status`.

## Scénarios couverts

### 1. Plan vanne seul

Validation de la séquence :

```text
READY -> RUNNING -> SUCCEEDED
```

Le plan contient une étape `VALVE_ON`, consommée passivement.

Le test vérifie qu’aucune transition pompe n’est enregistrée.

### 2. Plan pompe avec attente

Le plan utilisé est :

```text
VALVE_ON
WAIT 500 ms
PUMP_ON
```

Le test vérifie :

- l’entrée dans `WAITING` ;
- l’absence de progression avant 500 ms ;
- la progression exacte à 500 ms ;
- l’enregistrement passif de la pompe ;
- la terminaison en `SUCCEEDED`.

### 3. Rebouclage de `millis()`

Une attente de 32 ms est démarrée avant `0xFFFFFFFF` et terminée après le rebouclage.

Le test vérifie que le calcul non signé :

```cpp
static_cast<uint32_t>(nowMs - waitStartedAtMs)
```

reste correct.

### 4. Annulation

Une exécution placée en `WAITING` est annulée.

Le test vérifie :

- la transition vers `CANCELLED` ;
- l’horodatage de fin ;
- l’impossibilité de continuer à avancer après annulation.

### 5. Contexte invalide

Un `WorkflowId` invalide est fourni au chargement.

Le test vérifie :

- le refus du chargement ;
- l’état `FAILED` ;
- l’erreur `INVALID_CONTEXT` ;
- le caractère terminal de l’exécution.

## Invariant de sécurité

L’environnement `test_execution_engine` compile uniquement :

```text
src/EquipmentExecutionEngine.cpp
src/test_execution_engine.cpp
```

Il ne compile pas :

- `main.cpp` ;
- `ScheduleManager` ;
- `EquipmentManager.cpp` ;
- `RelaisManager` ;
- `EquipmentOutputRuntimeAdapter` ;
- `V4PilotRuntime` ;
- les drivers GPIO et I2C ;
- les backends physiques ;
- le Web ;
- le LCD.

Aucune sortie matérielle n’est donc commandée par ce banc.

## Fichiers modifiés

- `platformio.ini`
- `src/test_execution_engine.cpp`
- `docs/architecture/AQUALOOK_V4_PHASE6_RUN6_13_PASSIVE_ENGINE_TEST_BENCH.md`

## Position des modifications

### `platformio.ini`

Dans `env:ProgrammeArrosage.build_src_filter` :

```ini
-<test_execution_engine.cpp>
```

Cette exclusion empêche le programme de test d’être intégré aux firmwares nominal, legacy et V4.

Un nouvel environnement est ajouté après `env:ProgrammeArrosage_v4` :

```ini
[env:test_execution_engine]
```

L’exclusion est également ajoutée au filtre de `env:debug_boot`.

### `src/test_execution_engine.cpp`

Nouveau programme autonome Arduino contenant les cinq scénarios et leur synthèse série.

## Compilation attendue

```powershell
pio run -e test_execution_engine
```

## Téléversement et lecture sur le nouveau poste

```powershell
pio run -e test_execution_engine -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

## Résultat série attendu

Chaque assertion doit produire :

```text
[PASS] ...
```

La synthèse finale doit être :

```text
RESULT: passed=... failed=0 status=SUCCESS
```

## Validation des non-régressions

Après le banc autonome, recompiler :

```powershell
pio run -e ProgrammeArrosage_v4
pio run -e ProgrammeArrosage_legacy
```

Le Run 6.13 est validé uniquement si :

1. le banc compile ;
2. le banc affiche `failed=0 status=SUCCESS` ;
3. V4 compile ;
4. legacy compile ;
5. aucune commande relais n’est produite par le banc.

## Étape suivante

Après validation, le prochain run pourra introduire une observation passive du moteur depuis le firmware nominal, sans lui déléguer encore l’exécution des équipements.
