# AquaLook V4 — Modèle cartes, ports et canaux

**Date :** 7 juillet 2026  
**Run :** Phase 2 — Run 2.2

## 1. Objets

```text
BoardDefinition
PortDefinition
```

Une carte référence un contrôleur. Un port référence à la fois sa carte, le même contrôleur et un canal de ce contrôleur.

## 2. Carte

```text
BoardId
BoardTypeId
ControllerId
modelVersion
firstPortIndex
portCount
status
flags
```

Taille :

```text
16 octets
```

Les ports d’une carte forment une plage contiguë dans le tableau global.

## 3. Port

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

Taille :

```text
16 octets
```

## 4. Types de ports

```text
DIGITAL
PWM
COUNTER
ANALOG
RELAY
SENSOR
VIRTUAL
```

## 5. Directions

```text
INPUT
OUTPUT
BIDIRECTIONAL
```

## 6. États sûrs

```text
UNSPECIFIED
INACTIVE
ACTIVE
HIGH_IMPEDANCE
HOLD_LAST
```

## 7. Validation

Le validateur contrôle :

- les identifiants ;
- l’existence du contrôleur ;
- les statuts ;
- les plages de ports ;
- la cohérence carte/contrôleur ;
- les types, directions et états sûrs ;
- les capacités ;
- le numéro de canal ;
- les identifiants dupliqués ;
- les collisions de canaux.

## 8. Capacités

Un port ne peut pas déclarer plus que son contrôleur.

Exemple : un contrôleur uniquement `DIGITAL_OUTPUT` ne peut pas exposer un port `ANALOG_INPUT`.

## 9. Canaux partagés

Le partage est interdit par défaut. `PORT_FLAG_SHARED_CHANNEL` autorise explicitement une exception, à réserver à des modèles maîtrisés.

## 10. Activation des protocoles

Le modèle connaît les bus sans imposer leurs drivers. Les protocoles réellement compilés seront sélectionnés par un profil de build ultérieur.

Ainsi :

```text
BusType::CAN connu
≠ driver CAN compilé
```

## 11. Validation hôte

```text
Compilation hôte OK
BoardDefinition = 16 octets
PortDefinition = 16 octets
```

## 12. Hors périmètre

- catalogue de modèles de cartes ;
- binding vers `EquipmentId` ;
- driver réel ;
- activation sélective des protocoles ;
- persistance ;
- migration `RelayAssignment`.

## 13. Suite

Le Run 2.3 devra définir le binding entre `EquipmentId` et `PortId`, ainsi que la compatibilité conceptuelle avec `RelayAssignment` et `RelayTopology` sans modifier leur runtime.
