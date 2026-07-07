# AquaLook V4 — Checkpoint Phase 1 Run 1.7 — Consolidation et budget

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/aqualook-v4-domain`  
**Base de départ :** `ecfa76a3c084bc1aaaf8c1083abc2a6a644f5c98`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Consolider les modèles de Phase 1, fournir un registre borné générique, établir un catalogue minimal de types et calculer le budget RAM du domaine.

## 2. Fichiers source créés

```text
src/domain/BoundedRegistry.h
src/domain/EquipmentTypeCatalog.h
src/domain/EquipmentTypeCatalog.cpp
src/domain/DomainCapacityPlan.h
```

## 3. Registre borné

`BoundedRegistry<T>` utilise un tableau externe et une capacité connue à la construction.

API initiale :

```text
size
capacity
empty
full
data
at
append
findIf
removeAt
clear
```

Aucune allocation dynamique.

## 4. Catalogue minimal

```text
1  ZONE_VALVE
2  PUMP
3  AUXILIARY
4  GREENHOUSE_VENT
5  LIGHTING
```

Chaque type définit ses capacités obligatoires, ses capacités admises, sa version de schéma et ses bornes de paramètres.

Les validateurs métier du contenu des blocs sont volontairement différés jusqu’à la définition de leurs schémas exacts.

## 5. Tailles consolidées

```text
Equipment                    28 octets
EquipmentRuntimeState        56 octets
EquipmentFault               16 octets
OperationResult              16 octets
EquipmentIntent              32 octets
EquipmentExecution           40 octets
EquipmentDependency          16 octets
```

## 6. Plans de capacité

```text
SMALL
16 équipements, 8 exécutions, 24 dépendances, arène 2 Kio
Budget domaine : 5 168 octets

STANDARD
32 équipements, 16 exécutions, 64 dépendances, arène 4 Kio
Budget domaine : 10 592 octets

EXTENDED
64 équipements, 32 exécutions, 128 dépendances, arène 8 Kio
Budget domaine : 21 184 octets
```

Le profil `STANDARD` devient la référence de conception.

## 7. Seuils de compilation

```text
SMALL     <= 8 Kio
STANDARD  <= 16 Kio
EXTENDED  <= 32 Kio
```

Des `static_assert` empêchent un dépassement silencieux.

## 8. Validation hôte

Test de contrôle C++11 :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Cas vérifiés :

- ajout dans un registre ;
- recherche par prédicat ;
- suppression et compactage ;
- refus en capacité pleine ;
- calcul constexpr des trois budgets.

Résultat :

```text
Compilation hôte OK
Budgets octets : 5168 10592 21184
```

Ce test valide le contrat et les calculs. La compilation exacte de l’ensemble du dépôt sous PlatformIO reste différée.

## 9. Documentation créée ou modifiée

```text
docs/architecture/adr/ADR-0011-bounded-domain-registries-and-capacity-plan.md
docs/architecture/AQUALOOK_V4_PHASE1_CONSOLIDATION.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.7-consolidation-budget.md
```

## 10. Fichiers volontairement non modifiés

- `src/main.cpp` ;
- managers historiques ;
- `RelayTopology` et `RelaisManager` ;
- modèles des Runs 1.2 à 1.6 ;
- `platformio.ini` ;
- NVS ;
- ressources Web et LCD.

## 11. Comportement runtime

Aucun changement. Aucun registre n’est instancié dans le firmware actuel.

## 12. Limites du budget

Le budget n’inclut pas :

- Wi-Fi et TCP/IP ;
- serveur Web et JSON ;
- stacks FreeRTOS ;
- buffers écran ;
- heap et fragmentation ;
- éventuelle coexistence configuration active/candidate ;
- journaux et historique.

## 13. Compilation et mesures différées

À effectuer dès que le poste local sera disponible :

```text
pio run -e ProgrammeArrosage
```

Puis mesurer :

```text
flash utilisée
RAM statique
heap libre après démarrage
heap minimum après activité Web
plus grand bloc libre
```

## 14. Décision de phase

La **Phase 1 est clôturée sur le plan architectural et des modèles isolés**.

Elle n’est pas encore intégrée au runtime historique ni validée matériellement.

Le passage à la Phase 2 est autorisé à condition de rester isolé du moteur actuel.

## 15. Prochaine action unique

Démarrer **AquaLook V4 — Phase 2 — Run 2.1 — Inventaire générique des bus et contrôleurs**.

Le run devra définir :

- identité et type de bus ;
- instance de bus ;
- contrôleur ;
- adresse ;
- capacités ;
- validation des collisions ;
- aucune intégration matérielle active.
