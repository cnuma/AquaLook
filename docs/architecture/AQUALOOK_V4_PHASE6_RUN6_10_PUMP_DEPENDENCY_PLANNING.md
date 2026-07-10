# AquaLook V4 — Phase 6 — Run 6.10

## Objet

Préparer l’orchestration future d’une pompe partagée en formalisant les dépendances et l’ordre logique des actions, sans exécuter de commande pompe et sans modifier la persistance.

## Base

- branche : `feature/aqualook-v4-domain`
- base matérielle validée : `785d205fc28625b4020f8800e305b4d37c3fe85f`
- profil matériel validé : `ProgrammeArrosage_v4`

## Périmètre

Le Run 6.10 ajoute dans `EquipmentManager` :

- un type `PlanAction` ;
- un type `PlanStep` ;
- un plan borné `ZoneExecutionPlan` ;
- `buildZoneStartPlan(zone)` ;
- `buildZoneStopPlan(zone)`.

Ces méthodes ne commandent aucun matériel. Elles décrivent uniquement une séquence future à partir du modèle déjà résolu.

## Séquence sans pompe

Démarrage :

```text
VALVE_ON
```

Arrêt :

```text
VALVE_OFF
```

## Séquence avec pompe

Démarrage :

```text
VALVE_ON
WAIT startupDelayMs, si non nul
PUMP_ON
```

Arrêt :

```text
PUMP_OFF
WAIT shutdownDelayMs, si non nul
VALVE_OFF
```

Cet ordre prépare un comportement sûr :

- la vanne est ouverte avant la mise en route de la pompe ;
- la pompe est arrêtée avant la fermeture de la vanne.

## Bornes

- maximum de 4 étapes par plan ;
- aucune allocation dynamique ;
- index d’équipement sur 8 bits ;
- délais sur 32 bits dans le plan ;
- les délais restent issus des champs existants `startupDelayMs` et `shutdownDelayMs`.

## Invariants préservés

Aucune modification de :

- `main.cpp` ;
- `ScheduleManager` ;
- `EquipmentOutputRuntimeAdapter` ;
- `RelaisManager` ;
- `RelayTopology` ;
- NVS ;
- `ConfigManager` ;
- Web ;
- LCD ;
- modèle transitoire construit au boot.

Les méthodes existantes `startZone()` et `stopZone()` continuent à commander uniquement la vanne de zone.

## Critères de validation

1. compilation `ProgrammeArrosage_legacy` réussie ;
2. compilation `ProgrammeArrosage_v4` réussie ;
3. aucune différence de comportement matériel ;
4. les journaux du Run 6.9 restent identiques ;
5. aucune commande de rôle `PUMP` n’apparaît.

## Étape suivante

Le prochain run pourra ajouter un exécuteur non bloquant de plan, initialement désactivé par défaut. Il devra gérer :

- l’état courant du plan ;
- les délais par `millis()` ;
- les commandes concurrentes ;
- le partage d’une pompe entre plusieurs zones ;
- le rollback en cas d’échec ;
- l’arrêt sûr au reboot ou en défaut.

## Retour arrière

Le retour au commit `785d205fc28625b4020f8800e305b4d37c3fe85f` retire uniquement la préparation des plans pompe et conserve tous les runs validés jusqu’au Run 6.9.
