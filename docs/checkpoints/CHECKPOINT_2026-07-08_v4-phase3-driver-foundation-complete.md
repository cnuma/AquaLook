# AquaLook V4 — Phase 3 — Checkpoint fin de socle drivers

Date: 8 juillet 2026

Branch: `feature/aqualook-v4-domain`

Base de clôture avant ce checkpoint:

```text
767a2cd2239e293f3cc384bba7fa99d0862f17b1
```

## Objectif du checkpoint

Figer un état autonome de fin de socle drivers avant ouverture d’un nouveau chat et avant toute intégration runtime.

Ce checkpoint doit permettre de reprendre AquaLook V4 sans relire l’historique de conversation.

## État général

La Phase 3 a produit un socle complet mais encore isolé pour les actionneurs binaires :

```text
contrat binaire générique
registre borné de drivers
driver simulé
driver GPIO
driver XL9535
bootstrap non-runtime du registre
adaptateurs Arduino isolés
```

Le firmware historique compile et son comportement runtime n’est pas modifié.

## Validation PlatformIO de référence

Dernière compilation locale validée : Run 3.6.

Commande :

```powershell
pio run -e ProgrammeArrosage
```

Résultat :

```text
ProgrammeArrosage  SUCCESS
1 succeeded, 0 failed
Duration: 00:02:36.328
```

Mémoire :

```text
RAM:   20.6% — 67,384 bytes used of 327,680 bytes
Flash: 62.6% — 1,272,061 bytes used of 2,031,616 bytes
```

Capacité restante :

```text
RAM available:   260,296 bytes
Flash available: 759,555 bytes
```

Warning connu non bloquant :

```text
SdFat: #warning File not defined because __has_include(FS.h)
```

Ce warning est sans lien avec les ajouts V4.

## Chaîne d’architecture obtenue

```text
EquipmentIntent / future runtime decision
-> EquipmentPortBinding
-> PortDefinition
-> ControllerDefinition
-> BinaryActuatorDriverRegistry
-> BinaryActuatorDriverBinding
-> BinaryActuatorDriverOps
-> concrete driver
-> platform adapter
```

Aucun appel runtime à cette chaîne n’existe encore.

## Fichiers source V4 du socle drivers

### Contrat générique

```text
src/domain/BinaryActuatorDriver.h
src/domain/BinaryActuatorDriver.cpp
```

Rôle :

- définir états logiques `ACTIVE`, `INACTIVE`, `UNKNOWN` ;
- définir santé driver ;
- définir erreurs driver ;
- fournir l’interface `BinaryActuatorDriverOps` ;
- fournir les wrappers `configure`, `command`, `read`, `applySafeState` ;
- garantir l’idempotence des commandes.

### Registre borné

```text
src/domain/BinaryActuatorDriverRegistry.h
```

Rôle :

- stockage fourni par l’appelant ;
- aucun heap ;
- refus doublons ;
- refus capacité dépassée ;
- recherche par `ControllerTypeId`.

### Driver simulé

```text
src/domain/SimulatedBinaryActuatorDriver.h
src/domain/SimulatedBinaryActuatorDriver.cpp
```

Rôle :

- validation sans matériel ;
- injection de fautes ;
- validation readback ;
- validation santé driver.

### Driver GPIO

```text
src/domain/GpioBinaryActuatorDriver.h
src/domain/GpioBinaryActuatorDriver.cpp
src/drivers/ArduinoGpioPlatform.h
src/drivers/ArduinoGpioPlatform.cpp
```

Rôle :

- driver logique GPIO conditionné par `AQUALOOK_V4_ENABLE_GPIO` ;
- domaine indépendant d’Arduino ;
- adaptateur Arduino isolé ;
- validation broche de sortie ESP32 ;
- inversion logique via `PORT_FLAG_INVERTED` ;
- application de l’état sûr à la configuration.

Correction importante déjà appliquée :

```text
GpioPinMode::INPUT          -> MODE_INPUT
GpioPinMode::OUTPUT         -> MODE_OUTPUT
GpioPinMode::INPUT_PULLUP   -> MODE_INPUT_PULLUP
GpioPinMode::INPUT_PULLDOWN -> MODE_INPUT_PULLDOWN
GpioLevel::LOW              -> LEVEL_LOW
GpioLevel::HIGH             -> LEVEL_HIGH
```

Cause : collision avec les macros Arduino `INPUT`, `OUTPUT`, `LOW`, `HIGH`.

### Driver XL9535

```text
src/domain/Xl9535BinaryActuatorDriver.h
src/domain/Xl9535BinaryActuatorDriver.cpp
src/drivers/ArduinoI2cPlatform.h
src/drivers/ArduinoI2cPlatform.cpp
```

Rôle :

- driver logique XL9535 conditionné par `AQUALOOK_V4_ENABLE_I2C` ;
- domaine indépendant de `Wire` ;
- adaptateur `Wire` isolé ;
- canaux 0 à 15 ;
- registres 16 bits, octet bas puis octet haut ;
- état sûr écrit dans le latch avant passage du canal en sortie ;
- inversion logique via `PORT_FLAG_INVERTED`.

Registres :

```text
INPUT_PORT          0x00
OUTPUT_PORT         0x02
POLARITY_INVERSION  0x04
CONFIGURATION       0x06
```

### Bootstrap non-runtime

```text
src/domain/BinaryActuatorDriverBootstrap.h
src/domain/BinaryActuatorDriverBootstrap.cpp
```

Rôle :

- préparer l’enregistrement explicite des drivers ;
- aucun registre global ;
- aucune allocation dynamique ;
- aucun appel automatique ;
- filtrage par macros compilées ;
- propagation des erreurs du registre.

Drivers couverts :

```text
BOOTSTRAP_DRIVER_SIMULATED
BOOTSTRAP_DRIVER_GPIO
BOOTSTRAP_DRIVER_XL9535
```

## Macros de profil concernées

```text
AQUALOOK_V4_ENABLE_GPIO=1
AQUALOOK_V4_ENABLE_I2C=1
AQUALOOK_V4_ENABLE_SPI=0
AQUALOOK_V4_ENABLE_UART=0
AQUALOOK_V4_ENABLE_ONEWIRE=0
AQUALOOK_V4_ENABLE_CAN=0
AQUALOOK_V4_ENABLE_RS485=0
AQUALOOK_V4_ENABLE_REMOTE=0
AQUALOOK_V4_ENABLE_VIRTUAL=1
```

Le SPI V4 reste désactivé. Cela ne désactive pas le SPI historique utilisé par TFT/touch/SD.

## Validations hôte réalisées

### Run 3.1 — Contrat binaire

```text
Compilation hôte OK
Résultat : 8 6 1
```

Interprétation :

- `BinaryActuatorDriverResult` : 8 bytes ;
- `BinaryActuatorSession` : 6 bytes ;
- commande répétée idempotente : une seule écriture.

### Run 3.2 — Driver simulé

```text
Compilation hôte OK
ok 2 2 3 1
```

### Run 3.3 — Driver GPIO

```text
Compilation hôte OK
normalWrites=2 invertedWrites=2 normalReads=1 invertedReads=1
GPIO driver enabled
GPIO driver excluded
Profils conditionnels OK
```

### Run 3.5 — Driver XL9535

```text
Compilation hôte OK
normalWrites=2 invertedWrites=2 reads=2 probes=3
```

### Run 3.6 — Bootstrap registre

```text
Compilation hôte OK
registered=3 requested=3 failures-ok
```

## Validations PlatformIO complètes

### Run 3.4

```text
ProgrammeArrosage SUCCESS
RAM:   67,384 / 327,680 bytes
Flash: 1,271,749 / 2,031,616 bytes
```

### Run 3.5

```text
ProgrammeArrosage SUCCESS
RAM:   67,384 / 327,680 bytes
Flash: 1,271,997 / 2,031,616 bytes
```

Delta Run 3.5 vs Run 3.4 :

```text
RAM:   +0 bytes
Flash: +248 bytes
```

### Run 3.6

```text
ProgrammeArrosage SUCCESS
RAM:   67,384 / 327,680 bytes
Flash: 1,272,061 / 2,031,616 bytes
```

Delta Run 3.6 vs Run 3.5 :

```text
RAM:   +0 bytes
Flash: +64 bytes
```

## Runtime explicitement non modifié

Aucun changement fonctionnel runtime dans :

```text
main.cpp
RelayTopology
RelayAssignment
RelaisManager
ConfigManager
ScheduleManager
NVS
Web
LCD
```

Conséquence :

```text
aucun nouveau driver n’est instancié automatiquement
aucun relais n’est piloté par la nouvelle couche V4
aucune écriture I2C V4 réelle n’est déclenchée
aucune broche GPIO V4 n’est configurée au boot
```

## Invariants à préserver

1. Ne pas raccorder les drivers au runtime sans run dédié.
2. Ne pas modifier NVS dans la phase drivers.
3. Ne pas remplacer `RelaisManager` tant que la stratégie de transition n’est pas décidée.
4. Ne pas recréer de registre global implicite.
5. Garder les adaptateurs matériels hors du domaine pur.
6. Garder les drivers conditionnés par les macros de profil.
7. Toujours compiler PlatformIO après ajout d’un fichier dans `src/domain` ou `src/drivers`.
8. Préserver la compatibilité historique des sorties : `Zone N -> carte 0 -> voie N` tant que l’intégration runtime n’est pas planifiée.
9. Préserver l’absence de migration NVS tant qu’elle n’est pas explicitement demandée.
10. Mesurer flash/RAM à chaque activation plus concrète.

## Documentation de référence

```text
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/architecture/AQUALOOK_V4_BINARY_ACTUATOR_DRIVERS.md
docs/architecture/AQUALOOK_V4_SIMULATED_BINARY_DRIVER.md
docs/architecture/AQUALOOK_V4_GPIO_BINARY_DRIVER.md
docs/architecture/AQUALOOK_V4_XL9535_BINARY_DRIVER.md
docs/architecture/AQUALOOK_V4_BINARY_DRIVER_BOOTSTRAP.md
```

ADR :

```text
docs/architecture/adr/ADR-0017-binary-actuator-driver-contract.md
docs/architecture/adr/ADR-0018-simulated-binary-actuator-driver.md
docs/architecture/adr/ADR-0019-conditional-gpio-binary-driver.md
docs/architecture/adr/ADR-0020-conditional-xl9535-binary-driver.md
docs/architecture/adr/ADR-0021-binary-driver-bootstrap-non-runtime.md
```

Checkpoints récents :

```text
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase3-run3.1.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase3-run3.2.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase3-run3.3.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase3-run3.4-ci-integration.md
docs/checkpoints/CHECKPOINT_2026-07-08_v4-phase3-run3.5-xl9535.md
docs/checkpoints/CHECKPOINT_2026-07-08_v4-phase3-run3.6-driver-bootstrap.md
```

## Git — état de reprise

Avant création de ce checkpoint, le HEAD validé est :

```text
767a2cd2239e293f3cc384bba7fa99d0862f17b1
```

Après création de ce checkpoint, utiliser le HEAD du commit qui porte ce fichier.

Commandes locales de reprise :

```powershell
git fetch --all --prune
git switch feature/aqualook-v4-domain
git pull --ff-only
git rev-parse HEAD
pio run -e ProgrammeArrosage
```

## Recommandation pour nouveau chat

Ouvrir un nouveau chat avec le message :

```text
Reprise AquaLook V4 sur la base du checkpoint :
docs/checkpoints/CHECKPOINT_2026-07-08_v4-phase3-driver-foundation-complete.md
Branche : feature/aqualook-v4-domain
Objectif : poursuivre après fin du socle drivers Phase 3, sans modifier NVS ni runtime tant que la stratégie d’intégration n’est pas décidée.
```

## Suite recommandée

Deux options :

### Option A — plus prudente

```text
AquaLook V4 — Phase 4 — Run 4.1
Stratégie d’intégration runtime et migration progressive RelaisManager
```

Objectif : définir avant de coder comment le runtime historique basculera vers la chaîne V4.

### Option B — plus technique mais encore isolée

```text
AquaLook V4 — Phase 3 — Run 3.7
Point d’instanciation expérimental désactivé par défaut
```

Objectif : préparer un point d’instanciation compilé mais désactivé par macro, sans effet au boot.

Recommandation actuelle : Option A, car le socle drivers est suffisant. La priorité doit devenir la stratégie d’intégration runtime, pas l’ajout de couches supplémentaires.
