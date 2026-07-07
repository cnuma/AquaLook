# AquaLook V4 — Checkpoint Phase 2 Run 2.2 — Cartes, ports et canaux

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/aqualook-v4-domain`  
**Base de départ :** `c4b0fb8f7673c14ec9a3b8014461fadd6254d9f1`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Définir les cartes, ports et canaux génériques au-dessus des contrôleurs du Run 2.1, sans commander le matériel.

## 2. Fichiers source

Créés :

```text
src/domain/BoardPortModel.h
src/domain/BoardPortModel.cpp
```

Modifié :

```text
src/domain/DomainIdentifiers.h
```

## 3. Identifiants ajoutés

```text
BoardTypeId
PortId
```

## 4. Structures

```text
BoardDefinition = 16 octets
PortDefinition  = 16 octets
```

`BoardDefinition` référence un contrôleur et une plage contiguë de ports.

`PortDefinition` référence :

```text
ControllerId
BoardId
PortId
channel
capabilities
type
direction
safeState
flags
```

## 5. Types et directions

Types :

```text
DIGITAL
PWM
COUNTER
ANALOG
RELAY
SENSOR
VIRTUAL
```

Directions :

```text
INPUT
OUTPUT
BIDIRECTIONAL
```

États sûrs :

```text
UNSPECIFIED
INACTIVE
ACTIVE
HIGH_IMPEDANCE
HOLD_LAST
```

## 6. Validation

Le validateur refuse notamment :

- carte ou type invalide ;
- contrôleur absent ;
- plage de ports invalide ;
- port orphelin ;
- incohérence carte/contrôleur ;
- type, direction ou état sûr invalide ;
- capacité absente ou non supportée ;
- canal hors plage ;
- identifiant dupliqué ;
- collision de canal.

Le partage de canal est interdit par défaut et nécessite `PORT_FLAG_SHARED_CHANNEL`.

## 7. Validation hôte

```text
g++ -std=c++11 -Wall -Wextra -Werror
Compilation hôte OK
BoardDefinition = 16 octets
PortDefinition = 16 octets
```

## 8. Protocoles

Principe enregistré :

```text
protocole connu du modèle
≠ driver compilé
```

Un profil de build sélectif sera défini avant l’introduction des drivers concrets.

## 9. Compilation PlatformIO

Non exécutée :

```text
pio run -e ProgrammeArrosage
```

## 10. Documentation

Créée ou mise à jour :

```text
docs/architecture/adr/ADR-0013-generic-boards-ports-and-channels.md
docs/architecture/AQUALOOK_V4_BOARD_PORT_MODEL.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase2-run2.2-boards-ports-channels.md
```

## 11. Éléments inchangés

- runtime historique ;
- `RelayTopology` ;
- `RelayAssignment` ;
- managers ;
- NVS ;
- Web et LCD ;
- `platformio.ini`.

## 12. Prochaine action unique

Démarrer **Phase 2 — Run 2.3 — Binding Equipment vers ports et compatibilité relais**.
