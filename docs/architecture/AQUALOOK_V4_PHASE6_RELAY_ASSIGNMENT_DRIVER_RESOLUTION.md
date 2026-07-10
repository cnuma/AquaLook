# AquaLook V4 — Phase 6 — Run 6.3

## Résolution RelayAssignment vers registre de drivers et port physique

**Date :** 10 juillet 2026  
**Statut :** implémenté, validation à effectuer

## Objectif

Relier passivement `V4RelayPhysicalBackend` aux contrats Phase 3 existants sans activer aucune zone et sans modifier le runtime courant.

## Chaîne de résolution

```text
zone logique
  -> RelayTopology::resolveZoneValve(...)
  -> MappingResolution
  -> BoardDefinition
  -> ControllerDefinition
  -> PortDefinition
  -> BinaryActuatorDriverRegistry::find(...)
  -> BinaryActuatorDriverBinding
```

## Fichiers modifiés

```text
src/V4RelayPhysicalBackend.h
src/V4RelayPhysicalBackend.cpp
```

`V4RelayPhysicalBackend::bind(...)` reçoit désormais la topologie, les contrôleurs, les cartes, les ports et le registre de drivers binaires.

`resolveZoneTarget(...)` résout la zone, la carte, le contrôleur, le port binaire puis le driver adapté au type de contrôleur.

Pour une future zone migrée, le backend préparera la session, configurera l’actionneur, commandera la sortie et pourra lire son état.

## Inertie garantie

Le masque interne reste à zéro et aucune méthode publique ne permet de le modifier.

Conséquences :

- aucune zone migrée ;
- aucune commande envoyée à un driver V4 ;
- aucune lecture envoyée à un driver V4 ;
- `main.cpp` inchangé ;
- backend historique et fallback conservés.

## Invariants préservés

- aucun changement NVS ;
- aucun changement Web ;
- aucun changement LCD ;
- aucun changement JSON ;
- aucun driver V4 activé ;
- aucune zone migrée.

## Validation attendue

```powershell
pio run -e ProgrammeArrosage -t upload && pio device monitor -e ProgrammeArrosage
```

## Suite proposée

```text
AquaLook V4 — Phase 6 — Run 6.4
Profils compile-time legacy / V4, sans migration de zone
```
