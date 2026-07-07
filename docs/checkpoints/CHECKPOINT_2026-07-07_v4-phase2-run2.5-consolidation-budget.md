# AquaLook V4 — Checkpoint Phase 2 Run 2.5 — Consolidation et budget

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/aqualook-v4-domain`  
**Base de départ :** `97e52846868692e35cd28fec506ae30331637166`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Consolider l’inventaire matériel générique, calculer son budget RAM et décider la clôture de la Phase 2.

## 2. Fichier source créé

```text
src/domain/HardwareCapacityPlan.h
```

## 3. Structures consolidées

```text
BusDefinition             16 octets
ControllerDefinition      24 octets
BoardDefinition           16 octets
PortDefinition            16 octets
EquipmentPortBinding      16 octets
LegacyRelayReference       6 octets
LegacyEquipmentKey         4 octets
LegacyPortKey              4 octets
```

## 4. Profils de capacité

### SMALL

```text
2 bus
4 contrôleurs
4 cartes
16 ports
16 bindings
16 clés Equipment
16 clés Port
```

Budget :

```text
832 octets
1 664 octets actif + candidat
```

### STANDARD

```text
4 bus
8 contrôleurs
8 cartes
64 ports
64 bindings
32 clés Equipment
64 clés Port
```

Budget :

```text
2 816 octets
5 632 octets actif + candidat
```

### EXTENDED

```text
8 bus
16 contrôleurs
16 cartes
128 ports
128 bindings
64 clés Equipment
128 clés Port
```

Budget :

```text
5 632 octets
11 264 octets actif + candidat
```

Le profil `STANDARD` devient la référence.

## 5. Seuils verrouillés

```text
SMALL     <= 2 Kio
STANDARD  <= 4 Kio
EXTENDED  <= 8 Kio
```

Les seuils portent sur un inventaire unique.

## 6. Budget Phase 1 + Phase 2

```text
Phase 1 STANDARD           10 592 octets
Phase 2 STANDARD actif      2 816 octets
Total isolé                13 408 octets
```

Avec inventaire actif + candidat :

```text
16 224 octets
```

Sont exclus : drivers, stacks, Wi-Fi, Web, écran, JSON, buffers protocoles et fragmentation.

## 7. Validation hôte

Compilation :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Résultat :

```text
Compilation hôte OK
Budgets octets : 832 2816 5632 5632
```

La quatrième valeur correspond au profil STANDARD actif + candidat.

## 8. Explication simple C++11 / C++14

C++11 et C++14 sont deux versions du langage C++.

Le projet vise C++11 pour rester compatible avec l’environnement de compilation embarqué. Une fonction `constexpr` du Run 2.4 utilisait une écriture acceptée seulement à partir de C++14. Le test hôte l’a détectée et la fonction a été réécrite dans une forme compatible C++11.

Aucune conséquence fonctionnelle : il s’agissait uniquement d’une règle de syntaxe plus stricte.

## 9. Documentation

Créée ou mise à jour :

```text
docs/architecture/adr/ADR-0016-phase2-hardware-capacity-budget.md
docs/architecture/AQUALOOK_V4_PHASE2_CONSOLIDATION.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase2-run2.5-consolidation-budget.md
```

## 10. Éléments inchangés

- `main.cpp` ;
- `RelayTopology` ;
- `RelaisManager` ;
- NVS ;
- Web ;
- LCD ;
- drivers matériels ;
- initialisation des bus.

## 11. Décision

La **Phase 2 est clôturée sur le plan architectural et des modèles isolés**.

Elle n’est pas encore intégrée au firmware actif ni validée sur le matériel.

## 12. Prochaine action unique

Démarrer **Phase 3 — Run 3.1 — Contrat générique des actionneurs binaires et registre de drivers**.
