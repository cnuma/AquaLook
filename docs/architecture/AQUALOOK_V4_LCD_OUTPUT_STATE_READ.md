# AquaLook V4 — Lecture LCD via EquipmentOutput

**Date :** 9 juillet 2026  
**Phase :** 4 — Run 4.9  
**Statut :** lecture d’état LCD migrée par façade, compilation à valider

## 1. Objectif

Le Run 4.9 vise à faire passer la lecture d’état LCD par `EquipmentOutputRuntimeAdapter`, avec fallback strict vers `RelaisManager`.

L’objectif initial était de migrer uniquement `anyActive` dans `DisplayManager::update()`.

Compte tenu de la taille de `DisplayManager.cpp` et pour éviter son remplacement massif, la migration est appliquée dans `DisplayManager.h` avec une façade interne `OutputAwareRelayState`, sur le même principe que le Run 4.7 côté Web.

## 2. Fichier modifié

```text
src/DisplayManager.h
```

`DisplayManager.cpp` n’est pas modifié dans ce run.

## 3. Position précise de la modification

### 3.1 Include

Dans `src/DisplayManager.h`, après :

```cpp
#include "ScreenManager.h"
```

le forward declaration du Run 4.8 est remplacé par :

```cpp
#include "EquipmentOutputRuntimeAdapter.h"
```

### 3.2 Setter enrichi

Avant :

```cpp
void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs) {
    _outputs = outputs;
}
```

Après :

```cpp
void setOutputAdapter(AquaLook::Runtime::EquipmentOutputRuntimeAdapter* outputs) {
    _outputs = outputs;
    _relais.outputs = outputs;
}
```

### 3.3 Façade interne ajoutée

Dans la section privée de `DisplayManager`, ajout :

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

## 4. Effet sur DisplayManager.cpp

La ligne historique dans `DisplayManager::update()` reste inchangée dans le fichier source :

```cpp
if (_relais->getState(z)) { anyActive = true; break; }
```

Mais l’appel est maintenant intercepté par `OutputAwareRelayState::getState(zone)`.

Chaîne effective :

```text
DisplayManager::_relais.getState(zone)
  -> EquipmentOutputRuntimeAdapter::getZoneValveState(zone)
  -> fallback RelaisManager::getState(zone)
```

## 5. Portée réelle

Cette approche intercepte tous les appels LCD existants à :

```cpp
_relais->getState(zone)
```

et pas uniquement celui de `anyActive`.

C’est plus large que le plan initial Run 4.9, mais le risque fonctionnel reste limité car :

1. `EquipmentOutputRuntimeAdapter::getZoneValveState(zone)` délègue encore à `RelaisManager::getState(zone)` ;
2. le fallback `RelaisManager` reste présent ;
3. aucun rendu, tactile ou planning n’est modifié ;
4. aucun changement NVS n’est introduit.

## 6. Ce qui ne change pas

Le Run 4.9 ne modifie pas :

- `DisplayManager.cpp` ;
- `main.cpp` ;
- les écrans ;
- les coordonnées tactiles ;
- la cadence de refresh ;
- la veille écran ;
- Web ;
- NVS ;
- `RelaisManager` ;
- `ScheduleManager` ;
- drivers V4 Phase 3.

## 7. Invariants préservés

1. Aucun changement NVS.
2. Aucun changement de rendu LCD.
3. Aucun changement de navigation tactile.
4. Aucun changement de cadence refresh.
5. Aucun changement Web.
6. Fallback obligatoire vers `RelaisManager`.
7. Aucun driver V4 Phase 3 activé.
8. Mapping relais legacy conservé.

## 8. Validation attendue

Commande :

```powershell
pio run -e ProgrammeArrosage
```

Critères :

1. compilation réussie ;
2. l’écran LCD démarre normalement ;
3. la veille écran suit toujours l’état actif des zones ;
4. les boutons et états de zones restent cohérents ;
5. aucune régression Web.

## 9. Suite recommandée

Après validation compilation et test rapide LCD/Web, créer un checkpoint de fin de sous-séquence Web/LCD lectures d’état.
