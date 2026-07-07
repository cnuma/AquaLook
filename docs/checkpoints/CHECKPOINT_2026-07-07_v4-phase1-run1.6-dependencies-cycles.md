# AquaLook V4 — Checkpoint Phase 1 Run 1.6 — Dépendances et cycles

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/relay-board-mapping`  
**Base de départ :** `5516ef2d79c77e88ce7d87d3e93611143dbd633e`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Introduire les dépendances logiques entre équipements et refuser les cycles d’ordre avant activation d’une configuration candidate.

## 2. Fichiers source créés

```text
src/domain/DependencyModel.h
src/domain/DependencyModel.cpp
```

## 3. Structure

`EquipmentDependency` contient :

```text
sourceId
targetId
requiredState
delayMs
type
flags
```

Convention :

```text
sourceId = équipement dépendant
targetId = équipement requis ou contraignant
```

Taille mesurée :

```text
sizeof(EquipmentDependency) = 16 octets
```

## 4. Types de relations

```text
REQUIRES_STATE
START_AFTER
STOP_BEFORE
MUTUALLY_EXCLUSIVE
INHIBITS
```

## 5. Validation

Le modèle refuse :

- identifiant source invalide ;
- identifiant cible invalide ;
- auto-référence ;
- type inconnu ;
- état requis invalide ;
- source orpheline ;
- cible orpheline ;
- relation dupliquée ;
- cycle orienté ;
- workspace insuffisant.

## 6. Détection des cycles

Relations orientées contrôlées :

```text
REQUIRES_STATE
START_AFTER
STOP_BEFORE
```

Relations exclues du graphe d’ordre :

```text
MUTUALLY_EXCLUSIVE
INHIBITS
```

L’algorithme utilise un tri topologique de Kahn avec un workspace fourni par l’appelant.

## 7. Mémoire

- aucune allocation dynamique ;
- environ 3 octets temporaires par équipement hors alignement ;
- relation persistante ou active : 16 octets.

## 8. Test hôte

Compilation :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Cas vérifiés :

- graphe valide ;
- cycle sur trois équipements ;
- exclusivité réciproque autorisée ;
- cible orpheline ;
- doublon ;
- taille exacte.

Résultat :

```text
Compilation hôte OK
EquipmentDependency = 16 octets
```

## 9. Compilation PlatformIO

Non exécutée. La validation complète reste différée :

```text
pio run -e ProgrammeArrosage
```

## 10. Documentation créée ou modifiée

```text
docs/architecture/adr/ADR-0010-equipment-dependencies-and-cycle-detection.md
docs/architecture/AQUALOOK_V4_DEPENDENCY_MODEL.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.6-dependencies-cycles.md
```

## 11. Fichiers volontairement non modifiés

- `src/main.cpp` ;
- managers historiques ;
- `RelayTopology` ;
- modèles Equipment, Intent et Execution ;
- `platformio.ini` ;
- NVS ;
- ressources Web et LCD.

## 12. Comportement runtime

Aucun changement. Le modèle de dépendances n’est utilisé par aucun chemin exécuté du firmware actuel.

## 13. Risques et limites

- résolution runtime non implémentée ;
- groupes d’exclusivité non modélisés ;
- recherche d’identifiants linéaire ;
- complexité actuelle adaptée aux petites configurations mais à mesurer ;
- délai limité à 65 535 ms ;
- compilation ESP32 complète à confirmer.

## 14. Invariants

1. Dépendances entre `EquipmentId` uniquement.
2. Aucun port ou driver référencé.
3. Références validées avant activation.
4. Cycles d’ordre refusés.
5. Relations symétriques hors graphe topologique.
6. Aucune allocation dynamique.
7. Runtime historique inchangé.

## 15. Prochaine action unique

Démarrer **Phase 1 — Run 1.7 — Consolidation du domaine et budget global**.

Le run devra :

- inventorier toutes les structures de Phase 1 ;
- mesurer leur coût unitaire et par scénario ;
- définir les registres bornés nécessaires ;
- consolider le catalogue de types et les validateurs ;
- décider si la Phase 1 peut être clôturée avant la Phase 2.
