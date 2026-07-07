# AquaLook V4 — Modèle de dépendances

**Statut :** référence de Phase 1  
**Date :** 7 juillet 2026  
**Run :** Phase 1 — Run 1.6

## 1. Position

Les dépendances appartiennent à la configuration candidate. Elles relient des équipements logiques et sont validées avant activation.

```text
Equipment
+ EquipmentDependency
-> validation du graphe
-> configuration acceptée ou refusée
```

## 2. Fichiers

```text
src/domain/DependencyModel.h
src/domain/DependencyModel.cpp
```

## 3. Structure

`EquipmentDependency` occupe 16 octets :

```text
sourceId       2 octets
targetId       2 octets
requiredState  8 octets
delayMs        2 octets
type           1 octet
flags          1 octet
```

Convention :

```text
sourceId = équipement dépendant
targetId = équipement requis ou contraignant
```

## 4. Types

```text
REQUIRES_STATE
START_AFTER
STOP_BEFORE
MUTUALLY_EXCLUSIVE
INHIBITS
```

`REQUIRES_STATE` impose un état de la cible. `START_AFTER` et `STOP_BEFORE` expriment un ordre. `MUTUALLY_EXCLUSIVE` interdit une activité simultanée. `INHIBITS` permet à la cible de bloquer la source.

## 5. Délai

`delayMs` permet un délai de 0 à 65 535 ms. Les délais plus longs devront être portés par l’automatisme ou l’exécution.

## 6. Validation unitaire

`validateDependency()` contrôle :

- les deux identifiants ;
- l’absence d’auto-référence ;
- le type ;
- l’état requis ;
- la présence des deux équipements.

## 7. Validation globale

`validateDependencyGraph()` contrôle aussi :

- les relations dupliquées ;
- les cycles d’ordre ;
- la taille du workspace fourni.

Le résultat précise le type d’erreur, l’index de relation et l’équipement concerné.

## 8. Détection des cycles

Les types suivants participent au graphe orienté :

```text
REQUIRES_STATE
START_AFTER
STOP_BEFORE
```

Les types suivants n’y participent pas :

```text
MUTUALLY_EXCLUSIVE
INHIBITS
```

Le validateur utilise un tri topologique de type Kahn. Un cycle est présent lorsque tous les nœuds ne peuvent pas être retirés.

## 9. Mémoire de travail

L’appelant fournit deux tableaux temporaires : un degré entrant sur 16 bits et une marque de traitement sur 8 bits par équipement.

Le coût utile est donc d’environ trois octets par équipement, hors alignement. Aucune allocation dynamique n’est réalisée.

## 10. Test hôte

Compilation :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Cas validés :

- graphe orienté valide ;
- cycle sur trois équipements ;
- exclusivité mutuelle réciproque ;
- cible orpheline ;
- doublon ;
- taille exacte.

Résultat :

```text
Compilation hôte OK
EquipmentDependency = 16 octets
```

## 11. Hors périmètre

- résolution runtime ;
- propagation effective des arrêts ;
- orchestration ;
- groupes d’exclusivité ;
- intégration au moteur actuel.

## 12. Intégration future

L’arbitre consultera les dépendances pour autoriser ou refuser une intention. L’orchestrateur utilisera les relations d’ordre et les délais pour lancer les exécutions.

## 13. Suite

Le Run 1.7 devra consolider la Phase 1 avant le passage à l’inventaire matériel de Phase 2.
