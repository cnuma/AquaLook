# AquaLook V4 — Profil de protocoles et catalogues matériels

**Date :** 7 juillet 2026  
**Run :** Phase 2 — Run 2.4

## 1. Principe

```text
connu par le modèle
≠ compilé dans ce firmware
≠ driver concret déjà implémenté
```

Le modèle reste générique tandis que chaque build reste minimal.

## 2. Profil initial

```text
GPIO      activé
I2C       activé
VIRTUAL   activé
SPI       désactivé
UART      désactivé
ONEWIRE   désactivé
CAN       désactivé
RS485     désactivé
REMOTE    désactivé
```

Le masque calculé vaut :

```text
259 = GPIO + I2C + VIRTUAL
```

## 3. Activation

Dans `platformio.ini` :

```ini
-DAQUALOOK_V4_ENABLE_SPI=1
-DAQUALOOK_V4_ENABLE_RS485=1
```

La valeur `0` désactive le protocole pour la couche matérielle V4.

Cette activation ne suffit pas à elle seule : le driver concret devra exister et être conditionné par la même macro.

## 4. Contrôleurs catalogués

```text
1 LOCAL_GPIO
2 XL9535
3 MCP23017
4 MCP23S17
5 REMOTE_GENERIC
```

## 5. Cartes cataloguées

```text
1 LOCAL_GPIO_BANK
2 RELAY_8_XL9535
3 IO_16_MCP23017
4 IO_16_MCP23S17
5 REMOTE_GENERIC
```

## 6. Disponibilité dans le profil initial

```text
LOCAL_GPIO          disponible
XL9535              disponible
MCP23017            disponible
MCP23S17            indisponible car SPI=0
REMOTE_GENERIC      indisponible car REMOTE=0
```

## 7. Validation

`validateInventoryAgainstBuildProfile()` contrôle successivement :

- les protocoles des bus ;
- les types de contrôleurs ;
- la cohérence contrôleur/bus ;
- la plage d’adresse ;
- le nombre de canaux ;
- les modèles de cartes ;
- la cohérence carte/contrôleur ;
- la version de modèle et le nombre de ports.

## 8. Coût et dépendances

Les catalogues sont de petites tables constantes. Ils n’initialisent aucune bibliothèque.

Les futurs drivers devront suivre ce patron :

```cpp
#if AQUALOOK_V4_ENABLE_I2C
#include "drivers/I2cDriver.h"
#endif
```

Les dépendances PlatformIO spécifiques devront également être ajoutées uniquement dans les environnements qui activent le protocole.

## 9. Validation hôte

Deux profils ont été compilés en C++11 :

```text
profil initial : masque 259
profil distant : masque 385
```

Résultat :

```text
Compilation hôte OK
```

Le test a aussi permis de corriger une incompatibilité `constexpr` C++11 avant clôture du run.

## 10. Hors périmètre

- implémentation des drivers ;
- initialisation matérielle ;
- détection physique ;
- dépendances de bibliothèques par protocole ;
- mesure réelle du gain de flash ;
- interface Web de sélection du profil.

## 11. Suite

Le Run 2.5 devra consolider la Phase 2 et recalculer le budget matériel complet, puis décider le passage à la Phase 3 dédiée aux actionneurs et drivers.
