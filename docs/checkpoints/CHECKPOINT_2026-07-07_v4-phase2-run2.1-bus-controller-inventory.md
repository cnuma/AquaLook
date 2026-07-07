# AquaLook V4 — Checkpoint Phase 2 Run 2.1 — Bus et contrôleurs

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/aqualook-v4-domain`  
**Base de départ :** `b14b106c452d343c4289c587e4306e5e4261ac16`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Définir un inventaire générique et validable des bus et contrôleurs, sans initialisation matérielle.

## 2. Fichiers source

Créés :

```text
src/domain/HardwareInventoryModel.h
src/domain/HardwareInventoryModel.cpp
```

Modifié :

```text
src/domain/DomainIdentifiers.h
```

## 3. Identifiants ajoutés

```text
BusId
ControllerId
ControllerTypeId
```

Tous restent des identifiants forts sur 16 bits.

## 4. Bus

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
fréquence
timeout
BusId
type
instance
flags
quatre broches génériques
```

Taille :

```text
16 octets
```

## 5. Contrôleurs

`ControllerDefinition` contient :

```text
adresse générique 64 bits
capacités
ControllerId
ControllerTypeId
BusId
nombre de voies
statut
flags
```

Taille :

```text
24 octets
```

## 6. Validation

Le validateur refuse :

- bus invalide ou inconnu ;
- doublon d’identifiant de bus ;
- doublon type/instance ;
- fréquence invalide ;
- contrôleur invalide ;
- type de contrôleur invalide ;
- bus orphelin ;
- statut invalide ;
- nombre de voies nul ;
- adresse manquante ;
- adresse interdite ;
- adresse hors plage ;
- doublon d’identifiant de contrôleur ;
- collision d’endpoint.

## 7. Plages initiales

```text
I2C     0x08 à 0x77
SPI     0 à 255
UART    0 à 247
RS485   0 à 247
CAN     0 à 0x1FFFFFFF
ONEWIRE identifiant non nul
REMOTE  identifiant non nul
GPIO    adresse vide
VIRTUAL adresse vide
```

## 8. Tests hôte

Compilation :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Cas vérifiés :

- tailles exactes ;
- plages I²C et CAN ;
- inventaire valide ;
- collision I²C ;
- contrôleur sur bus absent ;
- adresse interdite sur GPIO ;
- doublon type/instance.

Résultat :

```text
Compilation hôte OK
BusDefinition = 16 octets
ControllerDefinition = 24 octets
validation inventory OK
```

## 9. Compilation PlatformIO

Non exécutée. Commande différée :

```text
pio run -e ProgrammeArrosage
```

## 10. Documentation

Créée ou mise à jour :

```text
docs/architecture/adr/ADR-0012-generic-bus-and-controller-inventory.md
docs/architecture/AQUALOOK_V4_HARDWARE_INVENTORY.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase2-run2.1-bus-controller-inventory.md
```

## 11. Fichiers volontairement non modifiés

- `src/main.cpp` ;
- `ConfigManager` ;
- `ScheduleManager` ;
- `RelaisManager` ;
- `RelayTopology` ;
- `WebManager` ;
- `DisplayManager` ;
- modèles de Phase 1 hors identifiants ;
- `platformio.ini` ;
- NVS ;
- ressources Web et LCD.

## 12. Comportement runtime

Aucun changement. Aucun bus n’est initialisé, aucun contrôleur n’est interrogé et aucune sortie n’est commandée.

## 13. Limites

- pas de catalogue de types de contrôleurs ;
- pas de modèle de carte ;
- pas de ports ou canaux détaillés ;
- pas de binding vers `Equipment` ;
- règles d’adresse encore génériques ;
- aucune détection physique ;
- aucune persistance.

## 14. Invariants

1. Les bus et contrôleurs sont décrits sans objet Arduino.
2. Un contrôleur référence un bus existant.
3. Un couple type/instance de bus est unique.
4. Un endpoint adressable ne peut pas être dupliqué sur le même bus.
5. Le runtime historique reste inchangé.
6. Aucun effet matériel.

## 15. Prochaine action unique

Démarrer **Phase 2 — Run 2.2 — Cartes, ports et canaux génériques**.
