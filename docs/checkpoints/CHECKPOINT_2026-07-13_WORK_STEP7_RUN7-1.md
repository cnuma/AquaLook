# CHECKPOINT — AquaLook — work/step7-run7-1 — RUN7.1

Date : 13 juillet 2026

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche de départ : `main`
- Commit de départ : `d28f318331fc6b66704a6afeff0d3c37beedb60a`
- Branche de travail : `work/step7-run7-1`

## Objectif

Poser le contrat initial de la Phase 7 — orchestrateur d'équipements — sans modifier le runtime matériel validé à l'étape 6.

## Réalisation

Ajout de `EquipmentOrchestrator`, couche applicative passive placée au-dessus de `EquipmentManager`.

L'orchestrateur :

- reçoit une intention de démarrage ou d'arrêt de zone ;
- contrôle son initialisation et l'indice de zone ;
- demande à `EquipmentManager` de construire le plan correspondant ;
- expose un aperçu compact : statut, intention, zone, besoin pompe, nombre d'étapes et résultat du plan ;
- n'exécute aucune action matérielle.

## Fichiers ajoutés

- `src/EquipmentOrchestrator.h`
- `src/EquipmentOrchestrator.cpp`

## Invariants préservés

1. Aucun changement dans `main.cpp`.
2. Aucun changement dans `ScheduleManager`.
3. Aucun changement NVS ou `ConfigManager`.
4. Aucun changement dans `RelaisManager`, les drivers ou la topologie.
5. Aucun démarrage réel de pompe.
6. Aucun retrait du fallback legacy.
7. Le backend V4 validé à l'étape 6 reste inchangé.

## Validation

Validation structurelle effectuée par inspection :

- interface séparée de la couche matérielle ;
- aucune méthode d'exécution appelée ;
- dépendance limitée au contrat public de `EquipmentManager` ;
- aucun état global ajouté ;
- aucun impact sur le chemin runtime actuel tant que l'orchestrateur n'est pas branché.

La compilation PlatformIO, l'upload et les tests matériels ne sont pas déclarés réussis dans ce checkpoint : ils doivent être exécutés sur le poste disposant de la chaîne AquaLook et de la carte.

## Commandes complètes de validation locale

La procédure standard AquaLook doit toujours inclure la compilation legacy, la compilation et l'upload V4, puis le monitor série :

```powershell
git fetch --prune
git switch work/step7-run7-1
git pull --ff-only

pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Ne pas lancer séparément `pio run -e ProgrammeArrosage_v4` avant l'upload : la commande d'upload compile déjà cet environnement.

Même lorsqu'une évolution n'est pas encore reliée au runtime, l'upload et le monitor restent nécessaires pour valider l'absence de régression sur la cible réelle.

## Étape suivante proposée — RUN7.2

Ajouter des tests hôte du contrat passif :

- orchestrateur non initialisé ;
- zone hors limites ;
- plan vanne seule ;
- plan avec dépendance pompe ;
- propagation d'un rejet produit par `EquipmentManager`.

RUN7.2 devra rester sans branchement dans `main.cpp`.
