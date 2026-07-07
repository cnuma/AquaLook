# ADR-0003 — Inventaire matériel générique par cartes et ports

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.1

## Contexte

Le premier modèle de topologie a été construit autour de cartes relais, de voies relais et d’affectations par rôle. Cette étape a permis de découpler une zone de sa sortie physique, mais elle reste trop spécialisée pour les besoins V4.

AquaLook doit également pouvoir représenter :

- entrées TOR ;
- sorties TOR ;
- entrées analogiques ;
- sorties analogiques ;
- compteurs d’impulsions ;
- PWM ;
- cartes mixtes ;
- nœuds déportés ;
- GPIO locaux ;
- cartes de communication.

## Décision

Le modèle matériel supérieur devient :

```text
HardwareInventory
├── HardwareBoard
│   └── HardwarePort
└── PortBinding
```

Le mot « relais » n’apparaît plus dans les concepts génériques.

## HardwareBoard

Une carte installée porte au minimum :

```text
BoardId id
BoardModelId modelId
BusType busType
uint16_t busAddress
bool enabled
BoardPresenceState presence
uint8_t declaredPortCount
```

Les caractéristiques fixes du modèle de carte sont décrites séparément par un `BoardModelDescriptor` afin d’éviter leur duplication dans chaque instance.

## HardwarePort

Un port physique est identifié par :

```text
BoardId + portIndex
```

Il décrit :

```text
direction autorisée
nature du signal
capacités
état sûr
caractéristiques électriques ou temporelles utiles
```

Directions initiales :

```text
INPUT
OUTPUT
BIDIRECTIONAL
```

Natures initiales :

```text
DIGITAL
ANALOG
```

Capacités initiales possibles :

```text
DIGITAL_INPUT
DIGITAL_OUTPUT
ANALOG_INPUT
ANALOG_OUTPUT
PULSE_COUNTER
FREQUENCY_INPUT
INTERRUPT_INPUT
PWM_OUTPUT
PULSE_OUTPUT
```

Les capacités sont représentées par un masque compact, pas par une hiérarchie dynamique.

## PortBinding

Un binding relie une fonction logique à un port physique :

```text
logicalTargetId
function
boardId
portIndex
configuration spécifique du binding
```

Exemples :

```text
équipement vanne 1 / commande ON-OFF -> board 1 / port 0
capteur débit zone 1 / compteur -> board 2 / port 3
pompe principale / commande marche -> board 1 / port 7
```

## Séparation modèle / instance

`BoardModelDescriptor` décrit les capacités intrinsèques d’un modèle :

```text
MCP23017
- bus I2C
- 16 ports bidirectionnels TOR
- interruptions disponibles
- plage d’adresses
```

`HardwareBoard` décrit une instance installée :

```text
id = 3
model = MCP23017
adresse = 0x22
activée = oui
présente = oui
```

## Place de RelayTopology

`RelayTopology` reste inchangé pendant la Phase 1 et sert d’adaptateur de compatibilité.

À terme :

```text
RelayBoardConfig -> adaptateur spécialisé de HardwareBoard
RelayAssignment -> adaptateur spécialisé de PortBinding
```

La migration ne doit pas casser le chemin runtime actuel avant validation de la Phase 3.

## Options rejetées

### Continuer avec `RelayBoardConfig`

Rejeté comme modèle supérieur : impossible de représenter proprement les entrées, analogiques et compteurs.

### Une structure différente pour chaque famille de carte

Rejeté au niveau inventaire : multiplication des chemins et des APIs. Les drivers restent spécialisés, l’inventaire reste commun.

### Un tableau fixe de 16 ports lourds dans chaque carte

Rejeté par défaut : les descripteurs partagés et représentations compactes doivent éviter de réserver inutilement toutes les caractéristiques par instance.

## Conséquences

- les limites deviennent `MAX_HARDWARE_BOARDS`, `MAX_PORTS_PER_BOARD` et `MAX_PORT_BINDINGS` ;
- le relais devient une technologie de sortie parmi d’autres ;
- un port peut être physiquement configurable ou câblé dans une direction fixe ;
- le nombre d’entrées et sorties est calculé depuis les ports déclarés ou configurés ;
- les drivers XL9535, MCP23017 et futurs nœuds restent spécialisés ;
- la Phase 2 devra formaliser le catalogue de modèles et la détection.

## Invariants

1. Une carte ne connaît pas les zones ou équipements qui l’utilisent.
2. Un port ne contient pas de référence métier directe.
3. Un binding référence un port par `BoardId + portIndex`.
4. La capacité du contrôleur et le nombre de ports réellement câblés sont distincts.
5. La nature TOR/analogique est distincte des capacités PWM, compteur ou interruption.
6. Le modèle matériel ne dépend pas du Web, du NVS ou de l’interface utilisateur.
