# AquaLook V4 — Cartographie runtime de RelaisManager

**Date :** 8 juillet 2026  
**Phase :** 4 — Run 4.1  
**Statut :** analyse documentaire avant modification runtime

## 1. Objectif

Ce document cartographie l’usage actuel de `RelaisManager` dans le runtime AquaLook afin de préparer la future abstraction `EquipmentOutput` sans modifier le comportement existant.

Cette analyse complète :

```text
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_RUNTIME_INTEGRATION_STRATEGY.md
```

## 2. Invariant de départ

Le runtime historique ne pilote pas encore les drivers V4 Phase 3.

La chaîne active reste :

```text
ScheduleManager
  -> callback relais historique
  -> RelaisManager::setRelay(zone, state)
  -> RelayTopology legacy compatible
  -> écriture I2C directe Wire
```

## 3. Instanciation et câblage dans main.cpp

`RelaisManager` est instancié globalement dans `main.cpp` :

```cpp
RelaisManager relaisMgr;
```

Il est initialisé après `ConfigManager` et avant `ScheduleManager` :

```cpp
configMgr.begin();
relaisMgr.begin(&configMgr);
scheduleMgr.begin();
scheduleMgr.setRelayCallback(onRelayRequest);
configMgr.applyToSchedule(scheduleMgr);
```

Le callback runtime est très étroit :

```cpp
static void onRelayRequest(uint8_t zone, bool state) {
    relaisMgr.setRelay(zone, state);
    EventBus::displayDirty = true;
}
```

Conséquence architecturale : la première future insertion `EquipmentOutput` peut se faire autour de ce callback sans modifier immédiatement le moteur de planification.

## 4. Points d’entrée publics de RelaisManager

API actuellement exposée :

```cpp
void begin(ConfigManager* config = nullptr);
void update();
void setRelay(uint8_t relay, bool state);
bool getState(uint8_t relay) const;
bool setAssignment(uint8_t assignmentIndex, bool state);
bool getAssignmentState(uint8_t assignmentIndex) const;
const RelayTopology::RelayTopologyConfig& topology() const;
```

Lecture V4 :

| Méthode | Rôle actuel | Lecture cible |
|---|---|---|
| `begin` | construit une topologie legacy et initialise les cartes | futur bootstrap runtime contrôlé |
| `update` | sécurité durée max par zone | watchdog de sorties logiques ou équipements |
| `setRelay` | commande zone -> relais | futur `setEquipmentOutput(zone_valve_N, state)` |
| `getState` | état logique par zone | futur état d'`EquipmentOutput` |
| `setAssignment` | commande une affectation matérielle relais | backend physique relais |
| `getAssignmentState` | état par affectation relais | diagnostic backend |
| `topology` | expose la topologie relais | diagnostic / compatibilité |

## 5. Responsabilités internes actuelles

`RelaisManager` regroupe aujourd’hui plusieurs responsabilités :

1. construction de topologie runtime compatible legacy ;
2. initialisation matérielle I2C ;
3. maintien de l’état logique par zone ;
4. maintien de l’état par affectation relais ;
5. traduction zone -> affectation relais ;
6. traduction affectation -> carte/voie/registre ;
7. écriture directe Wire ;
8. gestion du défaut `RELAY_I2C` ;
9. notification `EventBus::displayDirty` ;
10. sécurité de durée maximale d’arrosage.

Cette concentration est acceptable pour le runtime historique, mais elle ne doit pas devenir la forme cible du domaine V4.

## 6. Construction de topologie actuelle

`buildRuntimeTopology()` lit les paramètres existants depuis `ConfigManager` :

```cpp
nbZones
nbRelais
relayController
relayLogic
```

Puis appelle :

```cpp
RelayTopology::buildLegacyCompatibleTopology(
    _topology,
    nbZ,
    nbR,
    controller,
    logic
);
```

La compatibilité obtenue est :

```text
Zone N -> RelayAssignment ROLE_ZONE_VALVE targetIndex=N
       -> board 0
       -> channel N
```

Ce comportement doit rester le profil de compatibilité par défaut tant que la migration NVS n’est pas décidée.

## 7. Chemin de commande automatique

Le chemin d’activation planifiée est :

```text
loop()
  -> scheduleMgr.update(...)
  -> ScheduleManager::activateZone(zone, duration, manual=false)
  -> _relayCallback(zone, true)
  -> onRelayRequest(zone, true)
  -> relaisMgr.setRelay(zone, true)
```

Le chemin d’arrêt planifié est :

```text
loop()
  -> scheduleMgr.update(...)
  -> ScheduleManager::checkSlotEnd(zone)
  -> ScheduleManager::deactivateZone(zone)
  -> _relayCallback(zone, false)
  -> onRelayRequest(zone, false)
  -> relaisMgr.setRelay(zone, false)
```

Le planning ne connaît donc pas la topologie relais. Il connaît uniquement un index de zone et un état ON/OFF.

## 8. Chemin de commande manuelle

Les commandes manuelles Web passent par `ScheduleManager` :

```text
WebManager::handleManual
  -> ScheduleManager::startManualWatering(zone)
  -> callback relais
```

ou :

```text
WebManager::handleManual
  -> ScheduleManager::stopManualWatering(zone)
  -> callback relais
```

Les commandes manuelles LCD suivent le même principe :

```text
DisplayManager::handleTouchZone
  -> ScheduleManager::startManualWatering(_selectedZone)
```

ou :

```text
DisplayManager::handleTouchZone
  -> ScheduleManager::stopManualWatering(_selectedZone)
```

Point important : Web et LCD ne commandent pas directement `RelaisManager::setRelay`. Ils passent par le planning, ce qui préserve la cohérence durée/restant/manuel.

## 9. États lus par le Web et le LCD

Le Web lit l’état actif affiché depuis :

```cpp
_relais->getState(z)
```

et lit en parallèle l’état du planning :

```cpp
_schedule->isZoneActive(z)
_schedule->getElapsedMs(z)
_schedule->getRemainingMs(z)
_schedule->getLastReason(z)
```

Le LCD utilise aussi `RelaisManager::getState(zone)` pour déterminer si une zone est active, notamment pour le réveil écran et l’affichage des zones.

Conséquence pour V4 : l’abstraction `EquipmentOutput` ne doit pas seulement commander les sorties ; elle doit aussi fournir un état logique lisible et cohérent avec l’affichage.

## 10. Dépendances matérielles encore dans RelaisManager

`RelaisManager` contient encore :

```text
#include <Wire.h>
registres XL9535
registres MCP23017
adresses I2C
writeReg()
readReg()
applyBoard()
initBoard()
```

Ces éléments appartiennent à terme au backend physique relais, pas au domaine `EquipmentOutput`.

## 11. Sécurité runtime actuelle

`RelaisManager::update()` applique une sécurité de durée maximale :

```text
si _state[zone] est actif
et si now - _startMs[zone] >= maxWateringMs()
alors setRelay(zone, false)
```

Cette sécurité est actuellement portée par le manager relais alors qu’elle concerne en réalité une sortie logique de type `zone_valve`.

Décision recommandée : ne pas déplacer cette sécurité pendant Run 4.1. La déplacer uniquement lors d’un run dédié, avec test de non-régression.

## 12. Points de friction pour la migration V4

### 12.1 Nom de l’API historique

`setRelay(uint8_t relay, bool state)` reçoit en réalité un index de zone.

Le commentaire du header le précise déjà :

```text
API historique conservée : l'index reste un index de zone.
```

Le nom `relay` du paramètre est donc trompeur.

Correction possible future : ajouter une API wrapper plus claire sans supprimer l’ancienne :

```cpp
void setZoneValveOutput(uint8_t zone, bool state);
```

ou, plus générique :

```cpp
void setEquipmentOutput(EquipmentOutputId id, bool state);
```

### 12.2 État logique stocké par zone

`_state[MAX_ZONES]` représente aujourd’hui l’état logique des zones, pas l’état des relais physiques.

Ce tableau est un bon candidat futur pour devenir un état d’`EquipmentOutput`, mais il ne faut pas le déplacer avant stabilisation de l’adaptateur.

### 12.3 Mapping limité par MAX_RELAY_ASSIGNMENTS

`RelayTopology::MAX_RELAY_ASSIGNMENTS` est actuellement égal à `MAX_ZONES`.

Cela suffit pour les vannes de zones, mais pas pour ajouter simultanément pompe, auxiliaires, éclairage ou ventilation en plus des zones.

Ce point doit être traité dans un run ultérieur, pas dans Run 4.1.

### 12.4 `nbRelaisPhysical == nbZones`

`WebManager::handleSetSystem()` force actuellement :

```cpp
next.nbRelaisPhysical = next.nbZones;
```

C’est cohérent avec le profil historique, mais incompatible à terme avec des sorties non-zone ou des cartes d’extension plus riches.

Ce point touche NVS/configuration persistée : il est explicitement hors périmètre Run 4.1.

## 13. Position cible recommandée

La cible progressive est :

```text
ScheduleManager
  -> OutputRequestCallback(EquipmentOutputRole::ZONE_VALVE, zone, state)
  -> EquipmentOutputRuntimeAdapter
  -> profil de compatibilité zone_valve_N
  -> backend Relay
  -> BinaryActuatorDriverRegistry
```

Mais la première étape ne doit pas modifier `ScheduleManager`.

Insertion minimale recommandée pour un run ultérieur :

```text
onRelayRequest(zone, state)
  -> EquipmentOutputRuntimeAdapter::setZoneValve(zone, state)
  -> RelaisManager::setRelay(zone, state)
```

Cette insertion permet de journaliser et valider le modèle `EquipmentOutput` sans changer le pilotage matériel.

## 14. Plan de modification futur fichier par fichier

### Run 4.2 possible — adaptateur non intrusif

Créer :

```text
src/domain/EquipmentOutputTypes.h
src/domain/EquipmentOutputRuntimeAdapter.h
src/domain/EquipmentOutputRuntimeAdapter.cpp
```

Ne pas modifier encore le comportement runtime.

### Run 4.3 possible — insertion passive dans main.cpp

Modifier uniquement :

```text
src/main.cpp
```

Position cible : remplacer le corps de `onRelayRequest()` par un appel à l’adaptateur, lequel délègue encore à `RelaisManager`.

### Run 4.4 possible — lecture d’état logique

Étudier :

```text
src/WebManager.cpp
src/DisplayManager.cpp
```

Objectif : remplacer progressivement les lectures `RelaisManager::getState(zone)` par une lecture d’état logique `EquipmentOutput`, tout en gardant le fallback historique.

## 15. Interdits maintenus

Toujours interdit à ce stade :

- migration NVS ;
- changement de format de configuration ;
- modification des commandes matérielles ;
- activation des drivers V4 dans le boot ;
- renommage de `RelaisManager` ;
- suppression de `setRelay()` ;
- changement de comportement manuel Web ou LCD.

## 16. Conclusion

`RelaisManager` est aujourd’hui à la fois :

```text
manager logique de zones
adaptateur de topologie relais
backend matériel I2C
watchdog de sécurité
source d’état affichable
```

La migration V4 doit donc être progressive.

La meilleure première insertion n’est pas de remplacer `RelaisManager`, mais d’introduire une couche `EquipmentOutputRuntimeAdapter` qui encapsule d’abord le chemin existant sans modifier le résultat matériel.
