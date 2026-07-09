# AquaLook V4 — Injection passive EquipmentOutputRuntimeAdapter dans DisplayManager

**Date :** 9 juillet 2026  
**Phase :** 4 — Run 4.8  
**Statut :** injection passive, aucune lecture LCD migrée

## 1. Objectif

Le Run 4.8 prépare `DisplayManager` à recevoir l’adaptateur runtime `EquipmentOutputRuntimeAdapter`.

Objectif strict : préparer une future migration des lectures d’état LCD sans changer encore :

- l’écran HOME ;
- la veille écran ;
- le refresh actif/nominal ;
- le tactile ;
- le bouton arroser/arrêter ;
- le comportement de `DisplayManager::update()`.

## 2. Fichiers modifiés

```text
src/DisplayManager.h
src/main.cpp
```

## 3. Modification dans DisplayManager.h

### 3.1 Forward declaration ajoutée

Ajout après les includes :

```cpp
namespace AquaLook { namespace Runtime {
class EquipmentOutputRuntimeAdapter;
}} // namespace AquaLook::Runtime
```

Cette déclaration évite d’inclure directement `EquipmentOutputRuntimeAdapter.h` dans `DisplayManager.h`.

### 3.2 Setter public ajouté

Ajout dans la section publique :

```cpp
void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs) {
    _outputs = outputs;
}
```

### 3.3 Pointeur privé ajouté

Ajout dans la zone managers :

```cpp
AquaLook::Runtime::EquipmentOutputRuntimeAdapter* _outputs = nullptr;
```

## 4. Modification dans main.cpp

Ajout juste avant `displayMgr.begin(...)` :

```cpp
displayMgr.setOutputAdapter(&outputAdapter);
```

Position actuelle :

```text
weatherMgr.begin(&configMgr)
splashStep("Meteo")
delay(800)
displayMgr.setOutputAdapter(&outputAdapter)
displayMgr.begin(...)
```

## 5. Ce qui ne change pas

Le Run 4.8 ne modifie pas :

- `DisplayManager.cpp` ;
- `DisplayManager::update()` ;
- `DisplayManager::handleTouchZone()` ;
- `DisplayManager::drawHomeFull()` ;
- `DisplayManager::updateHomeDynamic()` ;
- les sprites ;
- la veille écran ;
- le refresh LCD ;
- les zones tactiles ;
- NVS ;
- `RelaisManager` ;
- `ScheduleManager` ;
- Web ;
- drivers V4 Phase 3.

## 6. Chaîne LCD après Run 4.8

Les lectures LCD restent encore historiques :

```text
DisplayManager
  -> RelaisManager::getState(zone)
```

Le pointeur futur existe cependant :

```text
DisplayManager
  -> _outputs : EquipmentOutputRuntimeAdapter*
```

## 7. Invariants préservés

1. Aucun changement NVS.
2. Aucun changement tactile.
3. Aucun changement du bouton manuel.
4. Aucun changement de veille écran.
5. Aucun changement de cadence de refresh.
6. Aucun changement de rendu LCD.
7. Aucun changement Web.
8. Aucun changement de commande runtime.
9. Aucun driver V4 Phase 3 activé.

## 8. Risque principal

Le risque du Run 4.8 est uniquement un risque de compilation lié à la déclaration et au stockage du pointeur `EquipmentOutputRuntimeAdapter`.

Le risque fonctionnel est très faible, car `_outputs` n’est pas encore utilisé par `DisplayManager.cpp`.

## 9. Validation attendue

Commande :

```powershell
pio run -e ProgrammeArrosage
```

Critère : compilation réussie, métriques RAM/Flash à enregistrer.

## 10. Suite recommandée

```text
AquaLook V4 — Phase 4 — Run 4.9
Lecture LCD anyActive via helper EquipmentOutput avec fallback RelaisManager
```

Priorité du Run 4.9 : migrer uniquement la lecture `anyActive` de `DisplayManager::update()`, sans toucher encore au bouton manuel.
