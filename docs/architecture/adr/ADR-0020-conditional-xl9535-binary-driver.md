# ADR-0020 — Driver XL9535 binaire conditionnel

- **Statut :** Acceptée
- **Date :** 8 juillet 2026
- **Phase :** V4 Phase 3 — Run 3.5

## Décision

Un driver binaire isolé est ajouté pour les contrôleurs `XL9535`.

Il est découpé en deux couches :

```text
Xl9535BinaryActuatorDriver
ArduinoI2cPlatform
```

La première couche reste indépendante d’Arduino et manipule une abstraction `Xl9535I2cOps`.

La seconde couche utilise `Wire` et reste isolée sous `src/drivers`.

## Condition de compilation

Le driver est compilé uniquement lorsque :

```text
AQUALOOK_V4_ENABLE_I2C=1
```

## Registres utilisés

```text
INPUT_PORT           0x00
OUTPUT_PORT          0x02
POLARITY_INVERSION   0x04
CONFIGURATION        0x06
```

Les lectures et écritures sont faites en 16 bits, octet bas puis octet haut.

## Configuration d’un port

La configuration :

1. vérifie le type `XL9535` ;
2. vérifie le port binaire ;
3. vérifie le canal 0 à 15 ;
4. sonde l’adresse I²C ;
5. prépare le latch de sortie dans l’état sûr ;
6. écrit le latch ;
7. bascule uniquement le canal ciblé en sortie.

L’écriture du latch avant le passage en sortie vise à limiter les glitches.

## Inversion

`PORT_FLAG_INVERTED` inverse le niveau physique sans modifier l’état logique :

```text
non inversé : INACTIVE -> 0, ACTIVE -> 1
inversé     : INACTIVE -> 1, ACTIVE -> 0
```

## État interne

Le contexte conserve :

```text
adresse I²C
latch de sortie 16 bits
masque de direction 16 bits
santé
état observé
```

## Options rejetées

### Inclure Wire dans le domaine

Rejeté : le domaine doit rester testable sur hôte.

### Configurer les 16 canaux en sortie

Rejeté : le driver ne doit modifier que le canal demandé.

### Utiliser directement le registre de polarité

Rejeté dans ce run : l’inversion logique reste gérée par le domaine via `PORT_FLAG_INVERTED`.

## Validation

Validation hôte du cœur logique :

```text
Compilation hôte OK
normalWrites=2 invertedWrites=2 reads=2 probes=3
```

## Invariants

1. Le driver n’est pas instancié par le runtime.
2. Aucun `RelaisManager` n’est modifié.
3. `Wire` reste dans l’adaptateur.
4. Le canal ciblé seul est basculé en sortie.
5. L’état sûr est écrit avant la configuration en sortie.
