# AquaLook V4 — Backlog addendum — Run 6.3

**Date :** 10 juillet 2026  
**Statut :** implémenté, validation à effectuer

## ARCH-103 — Résolution RelayAssignment vers driver/port

Statut : **ajoutée, compilation et test module à valider**.

Décisions :

- contrats réels inspectés : `RelayTopology`, `BoardPortModel`, `HardwareInventoryModel`, `BinaryActuatorDriver`, `BinaryActuatorDriverRegistry` ;
- `V4RelayPhysicalBackend::bind(...)` reçoit topologie, contrôleurs, cartes, ports et registre ;
- résolution zone vers `MappingResolution` ;
- résolution de la carte par `boardIndex` ;
- résolution du contrôleur lié à la carte ;
- résolution du port par `channelIndex` ;
- validation du port comme sortie binaire ;
- résolution du driver par `ControllerTypeId` ;
- session binaire distincte par zone ;
- préparation des appels de configuration, commande et lecture ;
- masque de migration toujours nul ;
- aucune API d’activation de zone ;
- aucun changement de `main.cpp` ;
- aucun changement NVS, Web, LCD ou JSON ;
- fallback historique conservé.

Validation :

```powershell
pio run -e ProgrammeArrosage -t upload && pio device monitor -e ProgrammeArrosage
```

Suite prévue :

```text
AquaLook V4 — Phase 6 — Run 6.4
Profils compile-time legacy / V4, sans migration de zone
```
