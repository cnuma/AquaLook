# AquaLook V4 — Driver XL9535 binaire isolé

**Run :** Phase 3 — Run 3.5  
**Date :** 8 juillet 2026

## Fichiers

```text
src/domain/Xl9535BinaryActuatorDriver.h
src/domain/Xl9535BinaryActuatorDriver.cpp
src/drivers/ArduinoI2cPlatform.h
src/drivers/ArduinoI2cPlatform.cpp
```

## Architecture

```text
contrat BinaryActuator
-> Xl9535BinaryActuatorDriver
-> Xl9535I2cOps
-> ArduinoI2cPlatform
-> Wire
```

Le domaine ne dépend pas de `Wire`.

## Sécurité de configuration

Le driver applique l’état sûr dans le latch de sortie avant de configurer le canal en sortie.

```text
write OUTPUT_PORT
write CONFIGURATION
```

## Canaux

Le driver accepte les canaux :

```text
0 à 15
```

Chaque canal correspond à un bit du registre 16 bits.

## Inversion

`PORT_FLAG_INVERTED` est traité dans le driver logique, pas dans le registre de polarité.

## Validation hôte

```text
Compilation hôte OK
normalWrites=2 invertedWrites=2 reads=2 probes=3
```

Interprétation :

- deux écritures par configuration valide ;
- une lecture normale ;
- une lecture inversée ;
- une sonde absente validée.

## Hors périmètre

- aucune instanciation du driver dans `main.cpp` ;
- aucun raccord à `RelaisManager` ;
- aucune migration NVS ;
- aucune écriture I²C réelle tant que le runtime ne l’utilise pas.

## Validation restante

Compiler localement :

```powershell
pio run -e ProgrammeArrosage
```

## Suite

Après compilation réussie, la suite logique est une consolidation des drivers Phase 3 avant toute intégration runtime.
