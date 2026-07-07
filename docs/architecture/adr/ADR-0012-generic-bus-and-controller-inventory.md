# ADR-0012 — Inventaire générique des bus et contrôleurs

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 2 — Run 2.1

## Contexte

AquaLook doit pouvoir décrire plusieurs moyens de connexion sans lier le domaine à une carte précise :

- GPIO locaux ;
- I²C ;
- SPI ;
- UART ;
- OneWire ;
- CAN ;
- RS485 ;
- nœuds distants ;
- bus virtuels.

Les contrôleurs attachés à ces bus doivent être identifiables, validables et capables d’exposer leurs fonctions sans initialiser réellement le matériel.

## Décision

Deux objets séparés sont introduits :

```text
BusDefinition
ControllerDefinition
```

`BusDefinition` décrit une instance de transport. `ControllerDefinition` décrit un composant connecté à cette instance.

## Bus

Un bus contient :

```text
BusId
BusType
instance
fréquence
timeout
flags
quatre positions de broches génériques
```

Les types initiaux sont :

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

La taille de `BusDefinition` est verrouillée à 16 octets.

## Contrôleur

Un contrôleur contient :

```text
ControllerId
ControllerTypeId
BusId
ControllerAddress
capabilities
channelCount
status
flags
```

La taille de `ControllerDefinition` est verrouillée à 24 octets.

## Adresse générique

`ControllerAddress` utilise deux mots de 32 bits :

```text
primary
secondary
```

Exemples :

```text
I2C       primary = adresse 7 bits
SPI       primary = identifiant de chip-select
RS485     primary = adresse esclave
CAN       primary = identifiant de trame ou nœud
ONEWIRE   primary + secondary = identifiant 64 bits
REMOTE    primary + secondary = identifiant distant
GPIO      adresse vide
```

Le modèle ne prétend pas définir encore tous les protocoles d’adressage. Il fournit une représentation compacte et stable.

## Capacités contrôleur

Les capacités initiales sont :

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

Elles décrivent ce que le contrôleur sait exposer, pas ce que l’équipement métier demande.

## Validation

Le validateur refuse :

- identifiant de bus invalide ;
- type de bus inconnu ;
- identifiant de bus dupliqué ;
- même type et même numéro d’instance ;
- fréquence absente sur un bus qui en exige une ;
- identifiant de contrôleur invalide ;
- type de contrôleur invalide ;
- bus référencé absent ;
- statut inconnu ;
- nombre de voies nul ;
- adresse manquante ;
- adresse interdite ;
- adresse hors plage ;
- identifiant de contrôleur dupliqué ;
- collision d’endpoint.

## Règles d’adresse initiales

```text
I2C     0x08 à 0x77
SPI     0 à 255
UART    0 à 247
RS485   0 à 247
CAN     0 à 0x1FFFFFFF
ONEWIRE non nul
REMOTE  non nul
GPIO    vide
VIRTUAL vide
```

Ces règles pourront être spécialisées par type de contrôleur ou protocole dans un run ultérieur.

## Collisions

Deux contrôleurs ne peuvent pas occuper le même endpoint non vide sur le même bus adressable.

Exemple refusé :

```text
I2C instance 0
contrôleur A adresse 0x20
contrôleur B adresse 0x20
```

Le même numéro d’adresse sur deux instances de bus différentes reste valide.

## Options rejetées

### Une seule structure fusionnant bus et contrôleur

Rejetée : plusieurs contrôleurs peuvent partager une même instance de bus.

### Stocker uniquement une adresse 8 bits

Rejeté : insuffisant pour CAN, OneWire et nœuds distants.

### Initialiser les drivers pendant la validation

Rejeté : le modèle doit rester testable sur hôte et indépendant du matériel.

### Référencer directement des objets Arduino

Rejeté : dépendance à `TwoWire`, `SPIClass`, `HardwareSerial` et aux plateformes.

## Conséquences

- l’inventaire matériel peut être validé avant activation ;
- les collisions I²C deviennent détectables ;
- les futurs modèles de cartes pourront référencer un `ControllerId` ;
- les drivers concrets resteront dans une couche d’adaptation ;
- l’ancien `RelayTopology` reste inchangé.

## Invariants

1. Un contrôleur référence toujours un `BusId` existant.
2. Deux bus ne partagent pas le même couple type/instance.
3. Une adresse doit respecter les règles de son type de bus.
4. Un endpoint adressable exclusif ne peut appartenir qu’à un contrôleur.
5. Aucun objet Arduino ou driver matériel n’apparaît dans le modèle.
6. La validation n’effectue aucune entrée-sortie matérielle.
