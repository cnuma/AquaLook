# AquaLook V4 — Checkpoint Phase 2 Run 2.3 — Binding Equipment vers ports

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/aqualook-v4-domain`  
**Base de départ :** `8b2f5524b663abba9eefc12bc2c6c2baa79bfd60`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Relier les équipements métier aux ports matériels génériques et préparer une passerelle conceptuelle depuis `RelayAssignment`, sans modifier le runtime historique.

## 2. Fichiers source créés

```text
src/domain/EquipmentPortBinding.h
src/domain/EquipmentPortBinding.cpp
```

## 3. Structure principale

`EquipmentPortBinding` contient :

```text
requiredPortCapabilities
EquipmentId
PortId
revision
kind
flags
```

Taille :

```text
16 octets
```

## 4. Types de binding

```text
PRIMARY_ACTUATOR
SECONDARY_ACTUATOR
OBSERVER
SAFETY_INPUT
```

## 5. Validation

Le validateur refuse :

- identifiants invalides ;
- équipement ou port absent ;
- type inconnu ;
- capacité requise vide ;
- capacité absente du port ;
- incompatibilité avec l’équipement ;
- direction incorrecte ;
- doublon ;
- plusieurs actionneurs primaires ;
- collision de port sans partage explicite.

## 6. Passerelle relais historique

Structures neutres :

```text
LegacyRelayReference      6 octets
LegacyEquipmentKey        4 octets
LegacyPortKey             4 octets
```

Résolution :

```text
(role, targetIndex)        -> EquipmentId
(boardIndex, channelIndex) -> PortId
```

Le binding produit est :

```text
PRIMARY_ACTUATOR
PORT_CAP_RELAY_OUTPUT
BINDING_FLAG_ENABLED
BINDING_FLAG_REQUIRED
```

Les rôles 1 à 5 restent alignés sur le modèle historique.

## 7. Isolation anti-régression

`EquipmentPortBinding` n’inclut pas `RelayTopology.h`.

Aucune modification de :

- `RelayTopology` ;
- `RelayAssignment` ;
- `RelaisManager` ;
- `main.cpp` ;
- NVS ;
- planning ;
- Web ;
- LCD.

## 8. Validation hôte

Compilation :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Résultat :

```text
Compilation hôte OK
EquipmentPortBinding = 16 octets
LegacyRelayReference = 6 octets
LegacyEquipmentKey = 4 octets
LegacyPortKey = 4 octets
```

## 9. Compilation PlatformIO

Non exécutée :

```text
pio run -e ProgrammeArrosage
```

## 10. Documentation

Créée ou mise à jour :

```text
docs/architecture/adr/ADR-0014-equipment-port-bindings-and-legacy-relay-bridge.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_PORT_BINDING.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase2-run2.3-equipment-port-binding.md
```

## 11. Limites

- pas de création automatique des équipements historiques ;
- pas de construction automatique des tables de migration ;
- pas de persistance ;
- pas de driver ;
- pas de commande matérielle ;
- pas d’intégration à `RelaisManager`.

## 12. Invariants

1. Le domaine métier cible un `PortId`, jamais une carte ou un canal.
2. Le binding déclare explicitement ses capacités requises.
3. La passerelle historique reste une traduction, pas le nouveau modèle principal.
4. Aucun include Arduino n’est introduit.
5. Le runtime actuel reste strictement inchangé.

## 13. Prochaine action unique

Démarrer **Phase 2 — Run 2.4 — Catalogues matériels et profil de protocoles compilés**.
