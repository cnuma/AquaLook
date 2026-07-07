# ADR-0015 — Catalogues matériels et profil de protocoles compilés

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 2 — Run 2.4

## Contexte

Le modèle V4 connaît plusieurs types de bus et de matériels. Cette connaissance ne doit pas entraîner l’inclusion systématique des bibliothèques, drivers, tâches et buffers de tous les protocoles.

## Décision

Le système distingue désormais trois niveaux :

```text
protocole connu du modèle
protocole autorisé dans ce build
driver concret réellement implémenté
```

Un type présent dans `BusType` n’est pas automatiquement disponible dans le firmware.

## Profil de build

Les macros suivantes sélectionnent les protocoles V4 :

```text
AQUALOOK_V4_ENABLE_GPIO
AQUALOOK_V4_ENABLE_I2C
AQUALOOK_V4_ENABLE_SPI
AQUALOOK_V4_ENABLE_UART
AQUALOOK_V4_ENABLE_ONEWIRE
AQUALOOK_V4_ENABLE_CAN
AQUALOOK_V4_ENABLE_RS485
AQUALOOK_V4_ENABLE_REMOTE
AQUALOOK_V4_ENABLE_VIRTUAL
```

Le profil initial AquaLook active :

```text
GPIO = 1
I2C = 1
VIRTUAL = 1
```

et désactive les autres protocoles V4.

## Effet sur l’espace mémoire

Les enums et petits catalogues restent présents et ont un coût limité. Les drivers, bibliothèques et buffers des protocoles désactivés ne doivent pas être inclus par les futurs modules de drivers.

Les fichiers de drivers devront être protégés par les mêmes macros ou exclus du `build_src_filter`.

## Catalogue de contrôleurs

Le catalogue minimal contient :

```text
LOCAL_GPIO
XL9535
MCP23017
MCP23S17
REMOTE_GENERIC
```

Chaque descripteur déclare :

```text
ControllerTypeId
BusType requis
capacités
nombre minimal et maximal de canaux
plage d’adresse
nom technique
```

## Catalogue de cartes

Le catalogue minimal contient :

```text
LOCAL_GPIO_BANK
RELAY_8_XL9535
IO_16_MCP23017
IO_16_MCP23S17
REMOTE_GENERIC
```

Chaque descripteur déclare son type de contrôleur requis, ses capacités de ports, son nombre de ports et sa version de modèle.

## Validation contre le build

Une configuration candidate est refusée lorsqu’elle contient :

- un bus dont le protocole n’est pas compilé ;
- un contrôleur dépendant d’un protocole désactivé ;
- une carte dépendant d’un contrôleur non disponible ;
- un type matériel inconnu ou incompatible ;
- une adresse, un nombre de canaux ou une version hors catalogue.

## Exemple

Avec le profil initial :

```text
XL9535 sur I2C       accepté
MCP23017 sur I2C     accepté
MCP23S17 sur SPI     refusé
REMOTE_GENERIC       refusé
```

Pour activer SPI :

```ini
-DAQUALOOK_V4_ENABLE_SPI=1
```

Cette macro rend le type disponible au modèle de build. Le driver SPI concret devra encore exister et être compilé.

## Options rejetées

### Compiler tous les drivers

Rejeté : augmentation inutile de la flash, de la RAM et des dépendances.

### Supprimer les types désactivés du modèle

Rejeté : imports, configurations et diagnostics doivent pouvoir identifier clairement un type non disponible.

### Déduire la disponibilité depuis les bibliothèques Arduino

Rejeté : comportement implicite et difficile à tester.

## Conséquences

- le firmware peut rester minimal ;
- les variantes matérielles sont pilotées par `platformio.ini` ;
- une configuration incompatible est refusée proprement ;
- les catalogues deviennent la base des futurs drivers ;
- le modèle reste générique sans imposer tous les protocoles.

## Invariants

1. Connaître un protocole ne signifie pas le compiler.
2. Les macros de build sont la source de vérité de disponibilité.
3. Les futurs drivers doivent respecter ces macros.
4. Une configuration ne peut pas activer un matériel indisponible dans le build.
5. Le runtime historique reste inchangé.
