# AquaLook V4 — Lecture Web /api/status via EquipmentOutput

**Date :** 9 juillet 2026  
**Phase :** 4 — Run 4.7  
**Statut :** lecture Web migrée avec fallback `RelaisManager`

## 1. Objectif

Le Run 4.7 migre la lecture d’état Web utilisée par `/api/status` vers la couche `EquipmentOutputRuntimeAdapter`, tout en conservant un fallback strict vers `RelaisManager`.

Le format JSON reste inchangé.

## 2. Fichier modifié

```text
src/WebManager.h
```

`src/WebManager.cpp` n’est pas remplacé dans ce run pour éviter une réécriture large d’un fichier long alors que la modification fonctionnelle est limitée à la lecture d’état.

## 3. Position précise de la modification

### 3.1 Include

Dans `src/WebManager.h`, après :

```cpp
#include "SdStaticHandler.h"
```

ajout :

```cpp
#include "EquipmentOutputRuntimeAdapter.h"
```

### 3.2 Setter existant enrichi

Le setter `setOutputAdapter(...)` conserve `_outputs` et transmet aussi le pointeur à la façade interne :

```cpp
void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs) {
    _outputs = outputs;
    _relais.outputs = outputs;
}
```

### 3.3 Façade interne de lecture

Dans la section privée de `WebManager`, ajout d’une façade :

```cpp
struct OutputAwareRelayState {
    RelaisManager* relay = nullptr;
    AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs = nullptr;

    OutputAwareRelayState& operator=(RelaisManager* value) {
        relay = value;
        return *this;
    }

    explicit operator bool() const {
        return relay != nullptr;
    }

    OutputAwareRelayState* operator->() {
        return this;
    }

    const OutputAwareRelayState* operator->() const {
        return this;
    }

    bool getState(uint8_t zone) const {
        if (outputs) {
            const AquaLook::Domain::EquipmentStateValue state =
                outputs->getZoneValveState(zone);

            if (state.validity == AquaLook::Domain::StateValidity::VALID &&
                state.kind == AquaLook::Domain::StateValueKind::BINARY) {
                return state.value != 0;
            }
        }

        return relay ? relay->getState(zone) : false;
    }
};
```

### 3.4 Type du membre `_relais`

Avant :

```cpp
RelaisManager* _relais = nullptr;
```

Après :

```cpp
OutputAwareRelayState _relais;
```

## 4. Effet sur WebManager.cpp

La ligne existante dans `WebManager::handleStatus()` reste lisible :

```cpp
zo["active"] = _relais ? _relais->getState(z) : false;
```

Mais l’appel `getState(z)` est maintenant résolu par la façade `OutputAwareRelayState`.

La chaîne devient :

```text
/api/status
  -> WebManager::_relais.getState(zone)
     -> EquipmentOutputRuntimeAdapter::getZoneValveState(zone)
        -> RelaisManager::getState(zone)
```

Fallback :

```text
si outputs absent
si état invalid/unknown/not_supported
si état non binaire
  -> RelaisManager::getState(zone)
```

## 5. Format JSON inchangé

Le champ exposé reste :

```json
"active": true
```

Aucun nom de champ, type de champ ou endpoint n’est modifié.

## 6. Ce qui ne change pas

Le Run 4.7 ne modifie pas :

- NVS ;
- `RelaisManager` ;
- `ScheduleManager` ;
- `DisplayManager` ;
- LCD ;
- routes Web ;
- format JSON `/api/status` ;
- drivers V4 Phase 3 ;
- mapping relais legacy.

## 7. Pourquoi cette approche

L’approche initiale prévue était de modifier directement `WebManager.cpp` avec un helper privé `zoneValveActive(uint8_t zone) const`.

Comme `WebManager.cpp` est un fichier long, la modification a été appliquée dans `WebManager.h` pour éviter un remplacement massif de fichier et limiter le risque de régression hors périmètre.

Le résultat fonctionnel est équivalent : la lecture d’état Web passe d’abord par `EquipmentOutputRuntimeAdapter`, avec fallback `RelaisManager`.

## 8. Validation attendue

Commande :

```powershell
pio run -e ProgrammeArrosage
```

Critères :

1. compilation réussie ;
2. `/api/status` répond toujours ;
3. `zones[].active` reste booléen ;
4. l’état affiché Web reste cohérent avec les relais ;
5. aucune régression LCD.

## 9. Suite recommandée

```text
AquaLook V4 — Phase 4 — Run 4.8
Injection passive de EquipmentOutputRuntimeAdapter dans DisplayManager
```

Objectif : préparer la migration LCD de la même manière, sans changer encore les écrans ni le comportement tactile.
