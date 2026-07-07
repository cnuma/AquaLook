# AquaLook V4 — Binding Equipment vers ports

**Date :** 7 juillet 2026  
**Run :** Phase 2 — Run 2.3

## 1. Relation centrale

```text
EquipmentId -> PortId
```

Le domaine métier ne connaît ni carte, ni adresse, ni canal physique.

## 2. Structure

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

## 3. Types de binding

```text
PRIMARY_ACTUATOR
SECONDARY_ACTUATOR
OBSERVER
SAFETY_INPUT
```

## 4. Validation

Le validateur contrôle :

- équipement et port existants ;
- capacités requises ;
- capacités réellement exposées ;
- compatibilité avec les capacités métier ;
- cohérence entrée/sortie ;
- doublons ;
- actionneur primaire unique ;
- collisions de ports.

## 5. Compatibilité relais

Le bridge historique repose sur :

```text
LegacyRelayReference
LegacyEquipmentKey
LegacyPortKey
```

Résolution :

```text
(role, targetIndex) -> EquipmentId
(boardIndex, channelIndex) -> PortId
```

Le résultat est un binding :

```text
PRIMARY_ACTUATOR
PORT_CAP_RELAY_OUTPUT
ENABLED + REQUIRED
```

## 6. Isolation

`RelayTopology.h` n’est pas inclus dans le domaine V4.

La couche de migration future copiera uniquement les champs simples du modèle historique vers `LegacyRelayReference`.

## 7. Validation hôte

```text
Compilation hôte OK
EquipmentPortBinding = 16 octets
LegacyRelayReference = 6 octets
LegacyEquipmentKey = 4 octets
LegacyPortKey = 4 octets
```

## 8. Hors périmètre

- modification de `RelayTopology` ;
- modification de `RelaisManager` ;
- création automatique des Equipment ;
- persistance ;
- commande physique ;
- migration NVS.

## 9. Suite

Le Run 2.4 devra définir le catalogue minimal des contrôleurs et cartes connus, ainsi que le profil de protocoles compilés avant l’introduction des drivers concrets.
