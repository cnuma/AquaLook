# AquaLook V4 — Driver GPIO binaire isolé

**Run :** Phase 3 — Run 3.3  
**Date :** 7 juillet 2026

## Fichiers

```text
src/domain/GpioBinaryActuatorDriver.h
src/domain/GpioBinaryActuatorDriver.cpp
src/drivers/ArduinoGpioPlatform.h
src/drivers/ArduinoGpioPlatform.cpp
```

## Architecture

```text
contrat binaire générique
-> driver GPIO logique
-> GpioPlatformOps
-> adaptateur Arduino ESP32
```

Le domaine n’inclut pas Arduino. L’adaptateur concret est isolé sous `src/drivers`.

## Sécurité

La configuration applique immédiatement l’état sûr du port. Les broches non valides en sortie ESP32 sont refusées via `GPIO_IS_VALID_OUTPUT_GPIO`.

## Inversion

`PORT_FLAG_INVERTED` inverse le niveau physique sans modifier l’état logique demandé.

## Validation hôte

```text
Compilation hôte OK
normalWrites=2 invertedWrites=2 normalReads=1 invertedReads=1
GPIO driver enabled
GPIO driver excluded
Profils conditionnels OK
```

## Hors périmètre

Aucune instance n’est créée dans `main.cpp`, aucun port réel n’est commandé et `RelaisManager` reste inchangé.

## Suite

Run 3.4 : driver XL9535 conditionnel et isolé, ou consolidation/compilation PlatformIO avant son introduction.
