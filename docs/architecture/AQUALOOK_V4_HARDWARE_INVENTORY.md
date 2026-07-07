# AquaLook V4 — Inventaire matériel générique

**Date :** 7 juillet 2026  
**Run :** Phase 2 — Run 2.1

## 1. Objets

```text
BusDefinition
ControllerDefinition
```

Un bus représente une instance de transport. Un contrôleur représente un composant connecté à cette instance.

## 2. Bus

Types supportés :

```text
GPIO
I2C
SPI
UART
ONEWIRE
CAN
RS485
REMOTE
VIRTUAL
```

`BusDefinition` contient :

```text
BusId
type
instance
fréquence
timeout
flags
quatre broches génériques
```

Taille :

```text
16 octets
```

## 3. Contrôleur

`ControllerDefinition` contient :

```text
ControllerId
ControllerTypeId
BusId
adresse générique
capacités
nombre de voies
statut
flags
```

Taille :

```text
24 octets
```

## 4. Capacités

```text
DIGITAL_INPUT
DIGITAL_OUTPUT
PWM_OUTPUT
COUNTER_INPUT
ANALOG_INPUT
INTERRUPT_INPUT
RELAY_OUTPUT
SENSOR_INPUT
REMOTE_NODE
```

## 5. Adresse générique

Deux mots de 32 bits permettent de représenter :

- adresse I²C ;
- chip-select SPI ;
- adresse Modbus ou RS485 ;
- identifiant CAN ;
- identifiant OneWire 64 bits ;
- identifiant de nœud distant.

## 6. Validation

Le validateur contrôle :

- identifiants ;
- types ;
- fréquence ;
- unicité type/instance ;
- existence du bus ;
- statut ;
- nombre de voies ;
- présence ou absence d’adresse ;
- plage d’adresse ;
- collisions d’endpoints.

## 7. Exemples

### I²C

```text
BusId 1
I2C instance 0
100 kHz

ControllerId 1
bus 1
adresse 0x20
16 voies
```

### SPI

```text
BusId 2
SPI instance 0
10 MHz

ControllerId 2
bus 2
chip-select 5
```

### GPIO local

```text
BusId 3
GPIO instance 0
adresse vide
```

## 8. Test hôte

Cas validés :

- inventaire valide ;
- collision I²C ;
- bus orphelin ;
- adresse interdite sur GPIO ;
- doublon type/instance ;
- plages I²C et CAN ;
- tailles exactes.

Résultat :

```text
Compilation hôte OK
BusDefinition = 16 octets
ControllerDefinition = 24 octets
validation inventory OK
```

## 9. Hors périmètre

- initialisation réelle des bus ;
- objets Arduino ;
- détection physique des périphériques ;
- modèle de carte ;
- ports et canaux détaillés ;
- bindings vers `Equipment` ;
- persistance ;
- intégration à `RelayTopology`.

## 10. Suite

Le Run 2.2 devra définir les cartes, ports et canaux génériques, puis relier ces ports aux contrôleurs sans encore commander le matériel.
