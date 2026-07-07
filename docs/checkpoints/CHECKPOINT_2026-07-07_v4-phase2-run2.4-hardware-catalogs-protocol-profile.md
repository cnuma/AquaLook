# AquaLook V4 — Checkpoint Phase 2 Run 2.4 — Catalogues et protocoles compilés

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/aqualook-v4-domain`  
**Base de départ :** `0ce9c6dc87a256a6c1a15bc2cfe62b332876fadc`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Définir les catalogues minimaux de contrôleurs et cartes, puis sélectionner explicitement les protocoles matériels disponibles dans chaque build.

## 2. Fichiers source créés

```text
src/domain/ProtocolBuildProfile.h
src/domain/HardwareCatalog.h
src/domain/HardwareCatalog.cpp
```

## 3. Fichier de build modifié

```text
platformio.ini
```

Position : section `[env:ProgrammeArrosage]`, bloc `build_flags` immédiatement après `CORE_DEBUG_LEVEL`.

## 4. Profil initial

```text
GPIO      1
I2C       1
SPI       0
UART      0
ONEWIRE   0
CAN       0
RS485     0
REMOTE    0
VIRTUAL   1
```

Masque calculé :

```text
259
```

## 5. Activation ultérieure

Exemple :

```ini
-DAQUALOOK_V4_ENABLE_SPI=1
-DAQUALOOK_V4_ENABLE_RS485=1
```

Une macro activée rend le protocole disponible pour l’inventaire V4. Le driver concret et ses dépendances devront encore être ajoutés conditionnellement.

## 6. Catalogue des contrôleurs

```text
LOCAL_GPIO
XL9535
MCP23017
MCP23S17
REMOTE_GENERIC
```

Chaque descripteur contient :

```text
type de bus requis
capacités
bornes de canaux
plage d’adresse
nom technique
```

## 7. Catalogue des cartes

```text
LOCAL_GPIO_BANK
RELAY_8_XL9535
IO_16_MCP23017
IO_16_MCP23S17
REMOTE_GENERIC
```

Chaque modèle contient :

```text
contrôleur requis
capacités de ports
bornes du nombre de ports
version du modèle
nom technique
```

## 8. Validation contre le build

Le validateur refuse :

- un bus dont le protocole est désactivé ;
- un contrôleur non disponible dans ce profil ;
- un contrôleur placé sur un mauvais type de bus ;
- une adresse ou un nombre de canaux hors catalogue ;
- une carte non disponible ;
- une carte liée au mauvais contrôleur ;
- une version ou un nombre de ports incompatible.

## 9. Validation hôte

Compilation :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Profils testés :

```text
profil initial : masque 259
profil distant : masque 385
```

Résultat :

```text
Compilation hôte OK
```

Le test a détecté puis permis de corriger une fonction `constexpr` initialement incompatible avec C++11.

## 10. Compilation PlatformIO

Non exécutée :

```text
pio run -e ProgrammeArrosage
```

## 11. Documentation

Créée ou mise à jour :

```text
docs/architecture/adr/ADR-0015-hardware-catalogs-and-compiled-protocol-profile.md
docs/architecture/AQUALOOK_V4_PROTOCOL_PROFILE_AND_CATALOGS.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase2-run2.4-hardware-catalogs-protocol-profile.md
```

## 12. Éléments inchangés

- `main.cpp` ;
- `RelayTopology` ;
- `RelaisManager` ;
- NVS ;
- Web ;
- LCD ;
- initialisation réelle des bus ;
- bibliothèques de protocoles supplémentaires.

## 13. Invariants

1. Un protocole connu n’est pas automatiquement compilé.
2. Le profil de build est explicite.
3. Une configuration incompatible est refusée avant activation.
4. Les futurs drivers suivront les mêmes macros.
5. Aucun driver ou buffer supplémentaire n’est introduit dans ce run.
6. Le runtime historique reste inchangé.

## 14. Prochaine action unique

Démarrer **Phase 2 — Run 2.5 — Consolidation de l’inventaire matériel et budget de Phase 2**.
