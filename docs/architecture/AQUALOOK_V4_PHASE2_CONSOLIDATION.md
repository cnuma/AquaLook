# AquaLook V4 — Consolidation de la Phase 2

**Date :** 7 juillet 2026  
**Branche :** `feature/aqualook-v4-domain`  
**Run :** Phase 2 — Run 2.5

## 1. Périmètre consolidé

La Phase 2 a défini :

```text
BusDefinition
ControllerDefinition
BoardDefinition
PortDefinition
EquipmentPortBinding
ProtocolBuildProfile
ControllerTypeCatalog
BoardTypeCatalog
Legacy relay bridge
HardwareCapacityPlan
```

## 2. Chaîne de description

```text
BusId
  -> ControllerId
      -> BoardId
          -> PortId
              -> EquipmentId via EquipmentPortBinding
```

Le domaine métier reste séparé du matériel concret.

## 3. Tailles unitaires

```text
BusDefinition             16 octets
ControllerDefinition      24 octets
BoardDefinition           16 octets
PortDefinition            16 octets
EquipmentPortBinding      16 octets
LegacyRelayReference       6 octets
LegacyEquipmentKey         4 octets
LegacyPortKey              4 octets
```

## 4. Profil matériel recommandé

```text
STANDARD
4 bus
8 contrôleurs
8 cartes
64 ports
64 bindings
32 clés Equipment historiques
64 clés Port historiques
```

Budget inventaire actif :

```text
2 816 octets
```

Budget actif + candidat :

```text
5 632 octets
```

## 5. Protocoles

Le profil initial compile pour la couche V4 :

```text
GPIO
I2C
VIRTUAL
```

Les protocoles suivants restent connus mais indisponibles dans ce build :

```text
SPI
UART
ONEWIRE
CAN
RS485
REMOTE
```

## 6. Catalogues initiaux

Contrôleurs :

```text
LOCAL_GPIO
XL9535
MCP23017
MCP23S17
REMOTE_GENERIC
```

Cartes :

```text
LOCAL_GPIO_BANK
RELAY_8_XL9535
IO_16_MCP23017
IO_16_MCP23S17
REMOTE_GENERIC
```

## 7. Validations disponibles

- identifiants et références ;
- collisions de bus et d’adresses ;
- plages de canaux ;
- capacités des contrôleurs et ports ;
- cohérence carte/contrôleur ;
- binding Equipment/Port ;
- disponibilité selon le profil compilé ;
- traduction conceptuelle de `RelayAssignment`.

## 8. État de clôture

La **Phase 2 peut être clôturée sur le plan architectural et des modèles isolés**.

Elle ne comprend pas encore :

- drivers matériels ;
- initialisation réelle des bus ;
- lecture ou écriture de ports ;
- persistance de l’inventaire ;
- migration effective de la configuration historique ;
- intégration à `RelaisManager` ;
- mesures PlatformIO et heap.

## 9. Budget global indicatif Phase 1 + Phase 2

Profil STANDARD :

```text
Domaine Phase 1              10 592 octets
Inventaire Phase 2 actif      2 816 octets
Total isolé                  13 408 octets
```

Avec inventaire actif + candidat :

```text
10 592 + 5 632 = 16 224 octets
```

Ce total exclut toujours les drivers, stacks, Wi-Fi, Web, écran et autres buffers du firmware.

## 10. Décision

Le passage à la Phase 3 est autorisé, mais doit commencer par une abstraction d’actionneur et des drivers conditionnels isolés. Aucun raccord direct au moteur historique ne doit être réalisé avant compilation complète et tests ciblés.

## 11. Phase suivante

```text
AquaLook V4 — Phase 3 — Run 3.1
Contrat générique des actionneurs binaires et registre de drivers
```
