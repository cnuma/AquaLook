# AquaLook V4 — Phase 4 — Run 4.13 — Câblage passif RelaisManagerBackend

**Date :** 10 juillet 2026  
**Statut :** code ajouté, compilation à valider  
**Base :** Run 4.12 — injection passive `RelayPhysicalBackend` dans `EquipmentOutputRuntimeAdapter`

## 1. Objectif

Run 4.13 câble `RelaisManagerBackend` dans `main.cpp` afin que `EquipmentOutputRuntimeAdapter` puisse utiliser la nouvelle frontière `RelayPhysicalBackend`.

Ce run ne branche aucun driver V4 réel.

Le backend actif reste basé sur `RelaisManager`, mais via l’adaptateur :

```text
EquipmentOutputRuntimeAdapter
  -> RelayPhysicalBackend
  -> RelaisManagerBackend
  -> RelaisManager
```

Le fallback direct est conservé :

```text
EquipmentOutputRuntimeAdapter
  -> RelaisManager
```

## 2. Fichier modifié

```text
src/main.cpp
```

Aucun autre fichier code n’est modifié dans ce run.

## 3. Modifications précises

### Include ajouté

Position : zone des includes, après `RelaisManager.h`.

```cpp
#include "RelaisManagerBackend.h"
```

### Instance globale ajoutée

Position : zone des instances globales, juste après `RelaisManager relaisMgr;`.

```cpp
AquaLook::Runtime::RelaisManagerBackend relaisBackend;
```

### Câblage ajouté

Position : `setup()`, juste après `relaisMgr.begin(&configMgr);`.

```cpp
relaisBackend.bind(&relaisMgr);
outputAdapter.setPhysicalBackend(&relaisBackend);
outputAdapter.bind(&relaisMgr);
```

`outputAdapter.bind(&relaisMgr)` est volontairement conservé.

## 4. Chaîne runtime après Run 4.13

Commande :

```text
ScheduleManager
  -> onRelayRequest(zone, state)
  -> EquipmentOutputRuntimeAdapter::setZoneValve(zone, state, millis())
  -> RelaisManagerBackend::setZoneValve(zone, state, nowMs)
  -> RelaisManager::setRelay(zone, state)
```

Fallback si le backend physique échoue :

```text
EquipmentOutputRuntimeAdapter
  -> RelaisManager::setRelay(zone, state)
```

Lecture Web/LCD :

```text
WebManager / DisplayManager
  -> EquipmentOutputRuntimeAdapter::getZoneValveState(zone)
  -> RelaisManagerBackend::getZoneValveState(zone, active)
  -> RelaisManager::getState(zone)
```

Fallback si le backend physique échoue :

```text
EquipmentOutputRuntimeAdapter
  -> RelaisManager::getState(zone)
```

## 5. Invariants préservés

1. Aucun changement NVS.
2. Aucun changement Web.
3. Aucun changement LCD.
4. Aucun changement JSON.
5. Aucun driver V4 réel activé.
6. `RelaisManager` reste le backend physique réel.
7. Fallback direct `RelaisManager` conservé.
8. `ScheduleManager` reste inchangé.
9. `RelaisManager::update()` reste inchangé.
10. Compatibilité historique `Zone N -> carte 0 -> voie N` conservée.

## 6. Point de vigilance

Run 4.11, Run 4.12 et Run 4.13 ont été enchaînés sans compilation intermédiaire.

La prochaine compilation valide donc simultanément :

```text
Run 4.11 — interface + adaptateur backend
Run 4.12 — injection backend optionnel dans EquipmentOutputRuntimeAdapter
Run 4.13 — câblage RelaisManagerBackend dans main.cpp
```

## 7. Validation attendue

Commande obligatoire avant toute suite :

```powershell
pio run -e ProgrammeArrosage
```

Tests rapides après compilation :

```text
/api/status
page principale
LCD veille/réveil
commande manuelle zone
état zone active Web/LCD
```

## 8. Suite possible après validation

Après compilation et test rapide OK, créer un checkpoint de transition backend :

```text
AquaLook V4 — Phase 4 — Checkpoint Run 4.13
Transition EquipmentOutput -> RelayPhysicalBackend -> RelaisManager validée
```

Ne pas brancher les drivers V4 réels avant ce checkpoint.
