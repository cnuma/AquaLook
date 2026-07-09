# AquaLook V4 — Phase 4 — Run 4.12 — Injection passive RelayPhysicalBackend

**Date :** 9 juillet 2026  
**Statut :** code ajouté, compilation à valider  
**Base :** Run 4.11 — `RelayPhysicalBackend` + `RelaisManagerBackend`

## 1. Objectif

Run 4.12 injecte passivement la frontière `RelayPhysicalBackend` dans `EquipmentOutputRuntimeAdapter`.

Le but est de préparer la chaîne cible :

```text
EquipmentOutputRuntimeAdapter
  -> RelayPhysicalBackend
  -> backend physique actif
```

sans supprimer le chemin historique :

```text
EquipmentOutputRuntimeAdapter
  -> RelaisManager
```

## 2. Fichiers modifiés

```text
src/EquipmentOutputRuntimeAdapter.h
src/EquipmentOutputRuntimeAdapter.cpp
```

Aucun autre fichier runtime n’est modifié.

## 3. Modifications dans le header

Fichier :

```text
src/EquipmentOutputRuntimeAdapter.h
```

Ajout d’une forward declaration :

```cpp
class RelayPhysicalBackend;
```

Ajout d’un setter public :

```cpp
void setPhysicalBackend(RelayPhysicalBackend* physicalBackend);
```

Ajout d’un pointeur privé :

```cpp
RelayPhysicalBackend* _physicalBackend = nullptr;
```

`bind(RelaisManager*)` reste présent.

## 4. Modifications dans l’implémentation

Fichier :

```text
src/EquipmentOutputRuntimeAdapter.cpp
```

Ajout de l’include :

```cpp
#include "RelayPhysicalBackend.h"
```

Ajout du setter :

```cpp
void EquipmentOutputRuntimeAdapter::setPhysicalBackend(
    RelayPhysicalBackend* physicalBackend
) {
    _physicalBackend = physicalBackend;
}
```

`isBound()` retourne désormais vrai si au moins un backend est présent :

```cpp
return _relayManager != nullptr || _physicalBackend != nullptr;
```

## 5. Chaîne de commande après Run 4.12

Dans `setZoneValve(...)` :

1. si `_physicalBackend` est renseigné, il est essayé en premier ;
2. si l’appel échoue ou si le backend est absent, fallback direct vers `_relayManager` ;
3. si aucun chemin n’est disponible, rejet `DEPENDENCY_UNAVAILABLE`.

Pseudo-chaîne :

```text
setZoneValve(zone, active)
  -> _physicalBackend->setZoneValve(zone, active, nowMs) si disponible
  -> fallback _relayManager->setRelay(zone, active)
```

## 6. Chaîne de lecture après Run 4.12

Dans `getZoneValveState(...)` :

1. si `_physicalBackend` est renseigné et renvoie un état valide, il est utilisé ;
2. sinon fallback direct vers `_relayManager->getState(zone)` ;
3. si aucun chemin n’est disponible, état `UNKNOWN`.

Pseudo-chaîne :

```text
getZoneValveState(zone)
  -> _physicalBackend->getZoneValveState(zone, active) si disponible et valide
  -> fallback _relayManager->getState(zone)
```

## 7. Caractère passif

Run 4.12 ne modifie pas `main.cpp`.

Aucun appel suivant n’est ajouté dans ce run :

```cpp
outputAdapter.setPhysicalBackend(...);
```

Donc dans l’état courant :

```text
_physicalBackend == nullptr
```

Le runtime actif reste donc équivalent au Run 4.11 :

```text
ScheduleManager
  -> EquipmentOutputRuntimeAdapter
  -> RelaisManager
```

## 8. Pourquoi le fallback reste nécessaire

Le fallback direct `RelaisManager` est volontairement conservé.

Il permet :

- de ne pas dépendre d’un backend non encore branché ;
- de revenir immédiatement au chemin historique ;
- de tester progressivement la nouvelle frontière ;
- d’éviter une panne d’arrosage si le backend physique futur échoue.

Suppression future possible uniquement après validation complète :

```text
EquipmentOutputRuntimeAdapter
  -> RelayPhysicalBackend
  -> backend physique validé
```

## 9. Invariants préservés

1. Aucun changement NVS.
2. Aucun changement Web.
3. Aucun changement LCD.
4. Aucun changement JSON.
5. Aucun changement `main.cpp`.
6. Aucun driver V4 réel activé.
7. `RelaisManager` reste le fallback actif.
8. Le runtime reste fonctionnel même sans `RelayPhysicalBackend`.
9. Compilation PlatformIO obligatoire avant suite.

## 10. Validation attendue

Commande :

```powershell
pio run -e ProgrammeArrosage
```

Ce run dépend du fait que Run 4.11 compile correctement. Comme Run 4.11 et Run 4.12 ont été enchaînés sans compilation intermédiaire, la prochaine compilation validera les deux runs à la fois.

## 11. Suite recommandée après compilation OK

```text
AquaLook V4 — Phase 4 — Run 4.13
Câblage passif RelaisManagerBackend dans main.cpp
```

Run 4.13 devra :

- instancier `RelaisManagerBackend` ;
- le binder à `RelaisManager` ;
- appeler `outputAdapter.setPhysicalBackend(&...)` ;
- conserver `outputAdapter.bind(&relaisMgr)` comme fallback ;
- ne pas brancher les drivers V4 réels ;
- ne pas toucher NVS.
