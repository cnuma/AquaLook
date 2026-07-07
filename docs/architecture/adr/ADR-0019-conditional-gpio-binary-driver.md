# ADR-0019 — Driver GPIO binaire conditionnel

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 3 — Run 3.3

## Décision

Le premier driver concret utilise deux couches :

```text
GpioBinaryActuatorDriver
ArduinoGpioPlatform
```

La première reste testable sur hôte. La seconde est la seule à inclure `Arduino.h` et `driver/gpio.h`.

## Condition de compilation

Tout le driver est protégé par :

```text
AQUALOOK_V4_ENABLE_GPIO
```

Lorsque la valeur vaut `0`, les déclarations et implémentations GPIO sont exclues.

## Mapping logique et électrique

Sans inversion :

```text
INACTIVE -> LOW
ACTIVE   -> HIGH
```

Avec `PORT_FLAG_INVERTED` :

```text
INACTIVE -> HIGH
ACTIVE   -> LOW
```

## Configuration

La configuration :

1. vérifie `LOCAL_GPIO` ;
2. vérifie le port binaire ;
3. valide la broche comme sortie ESP32 ;
4. configure la broche en sortie ;
5. applique immédiatement l’état sûr déclaré par le port.

## Adaptateur ESP32

L’adaptateur utilise :

```text
pinMode
digitalWrite
digitalRead
GPIO_IS_VALID_OUTPUT_GPIO
```

Il n’est pas instancié dans le runtime actuel.

## Validation

Les tests hôte couvrent :

- sortie normale ;
- sortie inversée ;
- lecture logique ;
- état sûr à la configuration ;
- broche invalide ;
- échec d’écriture ;
- compilation avec GPIO activé et désactivé.

## Invariants

1. Seul l’adaptateur plateforme dépend d’Arduino.
2. Une broche invalide n’est jamais configurée.
3. L’état sûr est appliqué pendant la configuration.
4. L’inversion reste une propriété du port.
5. Aucun raccord à `RelaisManager` n’est réalisé.
