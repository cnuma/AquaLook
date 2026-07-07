# ADR-0013 — Cartes, ports et canaux génériques

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 2 — Run 2.2

## Contexte

Après les bus et contrôleurs, AquaLook doit décrire les cartes fonctionnelles et leurs points de connexion sans dépendre d’un modèle matériel précis.

Une même carte peut exposer plusieurs ports. Chaque port correspond à un canal du contrôleur auquel la carte est rattachée.

## Décision

Deux objets sont introduits :

```text
BoardDefinition
PortDefinition
```

`BoardDefinition` décrit une carte logique ou physique. `PortDefinition` décrit un point d’entrée ou de sortie exposé par cette carte.

## Carte

Une carte contient :

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

La plage `firstPortIndex + portCount` référence un segment contigu du tableau global de ports.

La taille de `BoardDefinition` est verrouillée à 16 octets.

## Port

Un port contient :

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

La taille de `PortDefinition` est verrouillée à 16 octets.

## Types de ports

```text
DIGITAL
PWM
COUNTER
ANALOG
RELAY
SENSOR
VIRTUAL
```

## Directions

```text
INPUT
OUTPUT
BIDIRECTIONAL
```

## États sûrs

```text
UNSPECIFIED
INACTIVE
ACTIVE
HIGH_IMPEDANCE
HOLD_LAST
```

L’état sûr appartient au port matériel. Il reste distinct de l’état sûr métier de l’équipement.

## Capacités

Les capacités de port reprennent les fonctions exposables par les contrôleurs :

```text
DIGITAL_INPUT
DIGITAL_OUTPUT
PWM_OUTPUT
COUNTER_INPUT
ANALOG_INPUT
INTERRUPT_INPUT
RELAY_OUTPUT
SENSOR_INPUT
```

Un port ne peut pas annoncer une capacité absente de son contrôleur.

## Validation

Le validateur refuse :

- identifiant de carte invalide ;
- type de carte invalide ;
- contrôleur absent ;
- statut de carte inconnu ;
- plage de ports invalide ;
- identifiant de carte dupliqué ;
- identifiant de port invalide ;
- carte absente ;
- contrôleur du port différent de celui de la carte ;
- type, direction ou état sûr invalide ;
- absence de capacité ;
- capacité non supportée par le contrôleur ;
- canal hors plage ;
- identifiant de port dupliqué ;
- collision de canal.

## Canaux partagés

Par défaut, un canal de contrôleur ne peut appartenir qu’à un seul port.

Le flag `PORT_FLAG_SHARED_CHANNEL` permet une exception explicite. Cette possibilité est prévue pour des usages particuliers, mais elle devra être réservée à des modèles de cartes validés.

## Plages contiguës

Chaque carte référence un segment contigu du tableau des ports.

Cette décision simplifie :

- l’itération ;
- la sérialisation ;
- la construction dans une arène ou un tableau borné ;
- l’absence de pointeurs.

## Options rejetées

### Un tableau de ports intégré dans chaque carte

Rejeté : taille variable et pointeurs.

### Référencer seulement le numéro de canal

Rejeté : absence d’identité stable du port.

### Confondre carte et contrôleur

Rejeté : plusieurs modèles de cartes peuvent s’appuyer sur un même type de contrôleur.

### Réutiliser directement l’état sûr d’Equipment

Rejeté : la sécurité matérielle et l’intention métier ont des responsabilités différentes.

## Conséquences

- les futures cartes relais, entrées de débit et extensions deviennent descriptibles ;
- un binding futur pourra relier `EquipmentId` à `PortId` ;
- la compatibilité avec `RelayAssignment` pourra être réalisée par adaptation ;
- aucun driver matériel n’est encore activé.

## Protocoles compilés

Le modèle de carte et de port reste indépendant des protocoles réellement compilés.

La disponibilité de GPIO, I²C, SPI, RS485, CAN ou REMOTE sera contrôlée par un profil de build distinct dans un prochain run. La présence d’un type dans l’énumération n’entraîne aucune bibliothèque ou tâche supplémentaire.

## Invariants

1. Une carte référence un contrôleur existant.
2. Un port référence sa carte et le même contrôleur.
3. Le canal doit exister sur le contrôleur.
4. Les capacités du port sont un sous-ensemble de celles du contrôleur.
5. Les canaux sont exclusifs sauf déclaration explicite.
6. Aucun pointeur ou objet Arduino n’est stocké.
7. Aucun effet matériel n’est produit.
