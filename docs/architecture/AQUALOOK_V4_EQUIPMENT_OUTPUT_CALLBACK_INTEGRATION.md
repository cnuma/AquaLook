# AquaLook V4 — Branchement contrôlé du callback EquipmentOutput

**Date :** 8 juillet 2026  
**Phase :** 4 — Run 4.3  
**Statut :** branchement runtime minimal, compilation locale à valider

## 1. Objectif

Le Run 4.3 branche l’adaptateur `EquipmentOutputRuntimeAdapter` au point d’insertion le moins risqué identifié en Run 4.1 :

```text
main.cpp -> onRelayRequest(zone, state)
```

Le but est de faire passer la commande logique de zone par une abstraction `EquipmentOutput`, tout en conservant la délégation finale vers `RelaisManager`.

## 2. Fichier modifié

```text
src/main.cpp
```

Aucun autre fichier source n’est modifié dans ce run.

## 3. Modification exacte dans main.cpp

### 3.1 Include ajouté

Ajout :

```cpp
#include "EquipmentOutputRuntimeAdapter.h"
```

Position : après les includes des managers et diagnostics.

### 3.2 Instance globale ajoutée

Ajout :

```cpp
AquaLook::Runtime::EquipmentOutputRuntimeAdapter outputAdapter;
```

Position : avec les managers globaux, après `StorageManager storageMgr;`.

### 3.3 Callback modifié

Avant :

```cpp
static void onRelayRequest(uint8_t zone, bool state) {
    relaisMgr.setRelay(zone, state);
    EventBus::displayDirty = true;
}
```

Après :

```cpp
static void onRelayRequest(uint8_t zone, bool state) {
    outputAdapter.setZoneValve(zone, state, millis());
    EventBus::displayDirty = true;
}
```

### 3.4 Bind ajouté

Après :

```cpp
relaisMgr.begin(&configMgr);
```

Ajout :

```cpp
outputAdapter.bind(&relaisMgr);
```

## 4. Chaîne runtime après Run 4.3

La chaîne active devient :

```text
ScheduleManager
  -> onRelayRequest(zone, state)
  -> EquipmentOutputRuntimeAdapter::setZoneValve(zone, state, millis())
  -> RelaisManager::setRelay(zone, state)
  -> RelayTopology legacy compatible
  -> écriture I2C historique Wire
```

La sortie logique `zone_valve` est maintenant représentée dans la chaîne, mais le backend matériel reste inchangé.

## 5. Ce qui reste inchangé

Le Run 4.3 ne modifie pas :

- `ScheduleManager` ;
- `RelaisManager` ;
- `RelayTopology` ;
- `ConfigManager` ;
- NVS ;
- Web ;
- LCD ;
- drivers V4 Phase 3 ;
- logique I2C historique ;
- mapping legacy `Zone N -> carte 0 -> voie N`.

## 6. Risque fonctionnel

Le risque principal est un risque de compilation, pas un changement volontaire de comportement.

L’adaptateur appelle encore :

```cpp
_relayManager->setRelay(zoneIndex, active);
```

Donc le comportement matériel attendu reste celui de `RelaisManager`.

## 7. Point de vigilance

`RelaisManager::setRelay()` ne retourne pas encore de statut.

L’adaptateur retourne donc `OperationStatus::APPLIED` après délégation appelée. Ce statut n’est pas exploité par `main.cpp` dans Run 4.3.

## 8. Validation obligatoire locale

Commande attendue :

```powershell
pio run -e ProgrammeArrosage
```

La compilation n’a pas été validée dans ce run côté agent.

## 9. Critères de réussite Run 4.3

Le Run 4.3 sera pleinement validé lorsque :

1. `pio run -e ProgrammeArrosage` réussira ;
2. le boot conservera le même ordre fonctionnel ;
3. les commandes planifiées activeront toujours les zones ;
4. les commandes manuelles Web/LCD passeront toujours par `ScheduleManager` ;
5. aucun changement NVS ne sera observé ;
6. aucun driver V4 ne sera instancié automatiquement.

## 10. Suite recommandée

```text
AquaLook V4 — Phase 4 — Run 4.4
Contrôle de compilation et micro-correction si nécessaire
```

Ne pas poursuivre vers une migration Web/LCD ou NVS tant que Run 4.3 n’est pas compilé et testé.
