# AquaLook V4 — Phase 6 — Run 6.15 correctif capacité

## Objet

Permettre au scénario pompe synthétique du Run 6.15 de s’activer même lorsque toutes les zones occupent déjà les emplacements historiques du modèle.

## Base

- branche : `feature/aqualook-v4-domain`
- base : `468a67fd42cc963cecd87b5fa8e6f1e7e0fccb07`
- Run 6.14 : shadow runtime validé sur START et STOP
- premier essai Run 6.15 : non activé, faute de capacité disponible

## Cause

La capacité était définie ainsi :

```cpp
MAX_RELAY_ASSIGNMENTS = MAX_ZONES;
MAX_EQUIPMENTS = MAX_RELAY_ASSIGNMENTS;
```

Avec quatre zones configurées, les quatre emplacements étaient occupés. Aucun équipement pompe ni aucune affectation pompe ne pouvait être ajouté à la copie shadow.

## Correction

Quatre emplacements auxiliaires sont maintenant réservés :

```cpp
RESERVED_AUXILIARY_ASSIGNMENTS = 4;
MAX_RELAY_ASSIGNMENTS = MAX_ZONES + RESERVED_AUXILIARY_ASSIGNMENTS;
```

`EquipmentModel::MAX_EQUIPMENTS` suit automatiquement cette capacité.

Ces emplacements sont destinés aux équipements non-zone :

- pompe ;
- auxiliaire ;
- éclairage ;
- équipement futur.

## Topologie shadow indépendante

Le scénario cherche d’abord une voie libre dans la copie de la topologie réelle.

Si aucune voie libre n’existe, il crée une carte synthétique d’une voie uniquement dans `shadowRelayTopology`.

Cette carte :

- n’est pas enregistrée en NVS ;
- n’est jamais transmise à `RelaisManager` ;
- n’est jamais transmise au backend V4 ;
- n’accède pas au bus I2C ;
- ne commande aucun relais physique.

Le journal indique la provenance :

```text
source=free_channel
```

ou :

```text
source=synthetic_board
```

## Invariant de sécurité

Le modèle fonctionnel `transientEquipmentModel` et la topologie réelle de `RelaisManager` restent sans pompe.

Le chemin réel doit continuer à produire :

```text
Equipment plan: zone 1 START steps=1 pump=no dry_run=yes
```

Seul le shadow doit produire :

```text
Shadow: zone 1 START accepted=yes steps=3 pump=yes passive=yes
```

La carte 2 relais actuellement connectée ne doit pas voir son second relais commandé par ce run.

## Fichiers modifiés

- `src/RelayTopology.h`
- `src/main.cpp`
- `docs/architecture/AQUALOOK_V4_PHASE6_RUN6_15_CAPACITY_FIX.md`

## Éléments non modifiés

- NVS ;
- `ConfigManager` ;
- `RelaisManager` ;
- backend V4 ;
- adaptateur de sortie ;
- logique du programmateur ;
- commande réelle de pompe.

## Validation attendue

1. compilation `ProgrammeArrosage_v4` ;
2. compilation `ProgrammeArrosage_legacy` ;
3. téléversement V4 sur COM3 ;
4. présence du journal `Shadow pump: scenario pret` ;
5. présence de `steps=3 pump=yes` dans le shadow ;
6. présence des étapes `VALVE_ON`, `WAIT`, `PUMP_ON` au START ;
7. présence des étapes `PUMP_OFF`, `WAIT`, `VALVE_OFF` au STOP ;
8. maintien de `steps=1 pump=no` dans le chemin Equipment réel ;
9. absence de commande physique du second relais.
