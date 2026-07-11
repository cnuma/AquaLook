# AquaLook V4 — Phase 6 — Run 6.11

## Objet

Rendre les plans d’exécution zone/pompe observables en dry-run, sans activer de relais pompe et sans modifier le comportement fonctionnel des vannes.

## Base

- branche : `feature/aqualook-v4-domain`
- base : `dac345e48046e6c5518a5fd9de44cdabac0db863`
- statut de la base : non compilée au moment du démarrage du Run 6.11

Les Runs 6.10 et 6.11 devront être compilés ensemble avant tout téléversement.

## Périmètre

Le Run 6.11 ajoute :

- un nom lisible pour chaque action de plan ;
- un journal de synthèse du plan calculé ;
- un journal par étape ;
- un journal explicite en cas d’échec de résolution ;
- un appel automatique au dry-run avant `startZone()` et `stopZone()`.

## Invariant de sécurité

Le dry-run ne commande aucun équipement.

En particulier :

- `PUMP_ON` n’est jamais exécuté ;
- `PUMP_OFF` n’est jamais exécuté ;
- `WAIT` n’introduit aucun délai réel ;
- le résultat du dry-run ne bloque pas la commande de vanne existante ;
- `executeZone()` reste le seul chemin d’exécution fonctionnel de ce run.

## Journal attendu sans pompe

Le modèle transitoire actuel ne déclare aucune pompe. Une activation de zone doit donc produire :

```text
Equipment plan: zone 1 START steps=1 pump=no dry_run=yes
Equipment plan: zone 1 step=1 action=VALVE_ON equipment=0 dry_run=yes
Equipment: zone 1 ON path=physical_backend exec=1 totals=1/0/0
```

À l’arrêt :

```text
Equipment plan: zone 1 STOP steps=1 pump=no dry_run=yes
Equipment plan: zone 1 step=1 action=VALVE_OFF equipment=0 dry_run=yes
Equipment: zone 1 OFF path=physical_backend exec=2 totals=2/0/0
```

## Journal attendu avec pompe future

Lorsqu’un modèle non persistant déclarera une dépendance pompe, le plan de démarrage pourra être :

```text
Equipment plan: zone 1 START steps=3 pump=yes dry_run=yes
Equipment plan: zone 1 step=1 action=VALVE_ON equipment=0 dry_run=yes
Equipment plan: zone 1 step=2 action=WAIT delay=500 dry_run=yes
Equipment plan: zone 1 step=3 action=PUMP_ON equipment=4 dry_run=yes
```

Aucune de ces étapes pompe ne sera exécutée dans le Run 6.11.

## Fichiers modifiés

- `src/EquipmentManager.h`
- `src/EquipmentManager.cpp`
- `docs/architecture/AQUALOOK_V4_PHASE6_RUN6_11_PUMP_PLAN_DRY_RUN.md`

## Éléments non modifiés

- NVS ;
- `ConfigManager` ;
- `ScheduleManager` ;
- `RelayTopology` ;
- Web ;
- LCD ;
- backend V4 ;
- masque de migration ;
- commande matérielle de pompe.

## Validation différée

La prochaine validation devra obligatoirement inclure :

1. vérification du HEAD local ;
2. compilation `ProgrammeArrosage_legacy` ;
3. compilation `ProgrammeArrosage_v4` ;
4. absence d’erreur de linkage ;
5. téléversement seulement après réussite des deux compilations ;
6. vérification des logs dry-run sur au moins les zones 1 et 2 ;
7. confirmation que les chemins `physical_backend` et `relay_manager_fallback` restent inchangés.

## Retour arrière

Le retour au commit `dac345e48046e6c5518a5fd9de44cdabac0db863` supprime uniquement l’observabilité dry-run du Run 6.11 et conserve la construction passive des plans du Run 6.10.
