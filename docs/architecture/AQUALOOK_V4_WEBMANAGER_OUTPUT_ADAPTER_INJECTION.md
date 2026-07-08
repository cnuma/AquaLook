# AquaLook V4 — Injection passive EquipmentOutputRuntimeAdapter dans WebManager

**Date :** 8 juillet 2026  
**Phase :** 4 — Run 4.6  
**Statut :** injection passive, aucun changement JSON actif

## 1. Objectif

Le Run 4.6 prépare `WebManager` à recevoir l’adaptateur runtime `EquipmentOutputRuntimeAdapter`.

Objectif strict : permettre une future lecture d’état logique `EquipmentOutput` côté Web, sans modifier encore le comportement de `/api/status`.

## 2. Fichiers modifiés

```text
src/WebManager.h
src/main.cpp
```

## 3. Modification dans WebManager.h

### 3.1 Forward declaration ajoutée

Ajout avant la classe `WebManager` :

```cpp
namespace AquaLook { namespace Runtime {
class EquipmentOutputRuntimeAdapter;
}} // namespace AquaLook::Runtime
```

Cette déclaration évite d’inclure `EquipmentOutputRuntimeAdapter.h` dans `WebManager.h`.

### 3.2 Setter public ajouté

Ajout dans la section `public` :

```cpp
void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs) {
    _outputs = outputs;
}
```

Ce setter ne déclenche aucune route et ne modifie aucun état métier.

### 3.3 Pointeur privé ajouté

Ajout dans la section `private` :

```cpp
AquaLook::Runtime::EquipmentOutputRuntimeAdapter* _outputs = nullptr;
```

Ce pointeur est volontairement inutilisé dans Run 4.6.

## 4. Modification dans main.cpp

Ajout avant `webMgr.registerSdStaticHandler(&storageMgr)` :

```cpp
webMgr.setOutputAdapter(&outputAdapter);
```

Position logique : après l’initialisation NTP et avant le démarrage du serveur Web.

## 5. Ce qui ne change pas

Le Run 4.6 ne modifie pas :

- `WebManager::handleStatus()` ;
- le JSON `/api/status` ;
- le champ `zones[].active` ;
- les routes Web ;
- `RelaisManager` ;
- `ScheduleManager` ;
- `DisplayManager` ;
- NVS ;
- drivers V4 Phase 3.

## 6. Chaîne Web après Run 4.6

L’état Web reste lu comme avant :

```text
WebManager::handleStatus()
  -> RelaisManager::getState(zone)
```

Le pointeur futur existe cependant :

```text
WebManager
  -> _outputs : EquipmentOutputRuntimeAdapter*
```

Il sera exploité dans un run ultérieur.

## 7. Invariants préservés

1. Aucun changement NVS.
2. Aucun changement de format JSON.
3. Aucun changement de nom de champ Web.
4. Aucun changement de comportement affiché.
5. Aucun changement de commande runtime.
6. Aucun changement LCD.
7. Fallback `RelaisManager` intact.
8. Aucun driver V4 activé.

## 8. Risque principal

Le risque du Run 4.6 est uniquement un risque de compilation lié à la déclaration du type `EquipmentOutputRuntimeAdapter`.

Le risque fonctionnel est faible car le pointeur `_outputs` n’est pas utilisé.

## 9. Validation attendue

Commande :

```powershell
pio run -e ProgrammeArrosage
```

Critère : compilation réussie, métriques RAM/Flash à enregistrer.

## 10. Suite recommandée

```text
AquaLook V4 — Phase 4 — Run 4.7
Lecture Web /api/status via helper EquipmentOutput avec fallback RelaisManager
```

Le Run 4.7 devra modifier uniquement `WebManager::handleStatus()` et ajouter un helper privé `zoneValveActive(uint8_t zone) const`.
