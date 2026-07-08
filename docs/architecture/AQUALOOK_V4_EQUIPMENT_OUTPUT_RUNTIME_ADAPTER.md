# AquaLook V4 — Adaptateur runtime EquipmentOutput passif

**Date :** 8 juillet 2026  
**Phase :** 4 — Run 4.2  
**Statut :** socle de code ajouté, non branché au runtime

## 1. Objectif

Le Run 4.2 introduit un adaptateur `EquipmentOutput` non intrusif.

Objectif strict : préparer une couche de transition entre le runtime historique et `RelaisManager`, sans modifier le comportement actuel.

Le runtime actif reste inchangé :

```text
ScheduleManager
  -> onRelayRequest(zone, state)
  -> RelaisManager::setRelay(zone, state)
```

L’adaptateur ajouté n’est pas instancié dans `main.cpp`.

## 2. Fichiers ajoutés

```text
src/domain/EquipmentOutputTypes.h
src/EquipmentOutputRuntimeAdapter.h
src/EquipmentOutputRuntimeAdapter.cpp
```

## 3. Séparation domaine / runtime

Le domaine pur contient uniquement les types génériques :

```text
src/domain/EquipmentOutputTypes.h
```

Il ne dépend pas de `RelaisManager`, de `Wire`, d’Arduino, de NVS ou du runtime historique.

L’adaptateur runtime est placé hors de `src/domain` :

```text
src/EquipmentOutputRuntimeAdapter.h
src/EquipmentOutputRuntimeAdapter.cpp
```

Raison : cet adaptateur connaît volontairement `RelaisManager`, donc il ne doit pas polluer la couche domaine pure.

## 4. Types génériques introduits

### 4.1 `EquipmentOutputRole`

```cpp
UNKNOWN
ZONE_VALVE
PUMP
AUX
GREENHOUSE_VENT
LIGHTING
```

Ces rôles reprennent la cible fonctionnelle déjà identifiée pour les sorties AquaLook, mais sans imposer un backend relais.

### 4.2 `EquipmentOutputKind`

```cpp
UNKNOWN
BINARY
```

Le Run 4.2 couvre seulement les sorties binaires.

### 4.3 `EquipmentOutputRef`

Référence compacte d’une sortie logique :

```cpp
role
targetIndex
```

Exemple :

```text
role=ZONE_VALVE
targetIndex=0
```

signifie : vanne de zone 1.

### 4.4 `EquipmentOutputCommand`

Commande logique compacte :

```cpp
output
kind
active
```

Pour le Run 4.2, seule une commande binaire est exploitée.

## 5. API de l’adaptateur

L’adaptateur expose :

```cpp
void bind(RelaisManager* relayManager);
bool isBound() const;

Domain::OperationResult command(
    const Domain::EquipmentOutputCommand& command,
    uint32_t nowMs = 0U
);

Domain::OperationResult setZoneValve(
    uint8_t zoneIndex,
    bool active,
    uint32_t nowMs = 0U
);

Domain::EquipmentStateValue getZoneValveState(uint8_t zoneIndex) const;
```

## 6. Comportement interne actuel

Pour une commande `ZONE_VALVE`, l’adaptateur délègue encore directement :

```cpp
_relayManager->setRelay(zoneIndex, active);
```

Il retourne ensuite un `OperationResult` générique avec :

```text
status = APPLIED
stage  = APPLICATION
error  = NONE
```

Si l’adaptateur n’est pas lié à un `RelaisManager`, il retourne :

```text
status = REJECTED
error  = DEPENDENCY_UNAVAILABLE
```

Si la cible est invalide :

```text
status = REJECTED
error  = INVALID_TARGET
```

Si le rôle n’est pas supporté :

```text
status = REJECTED
error  = CAPABILITY_NOT_SUPPORTED
```

## 7. Lecture d’état logique

`getZoneValveState(zoneIndex)` délègue encore à :

```cpp
_relayManager->getState(zoneIndex)
```

Cela prépare la future substitution progressive des lectures Web/LCD sans changer le comportement actuel.

## 8. Ce qui n’est pas fait au Run 4.2

Le Run 4.2 ne fait pas :

- d’instanciation globale de l’adaptateur ;
- de modification de `main.cpp` ;
- de modification de `ScheduleManager` ;
- de modification de `RelaisManager` ;
- de modification de `WebManager` ;
- de modification de `DisplayManager` ;
- de modification NVS ;
- d’activation des drivers V4 Phase 3 ;
- de changement de chemin matériel.

## 9. Position de modification future

Le premier branchement possible se fera dans `main.cpp`, dans le callback :

```cpp
static void onRelayRequest(uint8_t zone, bool state) {
    relaisMgr.setRelay(zone, state);
    EventBus::displayDirty = true;
}
```

Remplacement futur possible :

```cpp
static void onRelayRequest(uint8_t zone, bool state) {
    outputAdapter.setZoneValve(zone, state, millis());
    EventBus::displayDirty = true;
}
```

Ce remplacement ne doit pas être fait sans run dédié et validation PlatformIO.

## 10. Risques identifiés

### 10.1 Compilation non encore validée localement

Les fichiers C++ ajoutés sont simples et isolés, mais ils doivent être validés par :

```powershell
pio run -e ProgrammeArrosage
```

### 10.2 Résultat de commande optimiste

`RelaisManager::setRelay()` ne retourne pas de statut.

L’adaptateur considère donc une délégation appelée comme `APPLIED`.

Ce point est acceptable pour Run 4.2, car l’adaptateur n’est pas encore branché. Un run ultérieur pourra améliorer le retour d’état si `RelaisManager` expose une API booléenne.

### 10.3 Mapping `EquipmentId`

Pour les vannes de zone, l’identifiant logique provisoire est :

```text
EquipmentId = zoneIndex + 1
```

Ce mapping est suffisant pour un adaptateur passif, mais devra être consolidé quand le modèle d’équipements sera persisté.

## 11. Critères de réussite Run 4.2

Le Run 4.2 est réussi si :

1. les types `EquipmentOutput` existent côté domaine pur ;
2. l’adaptateur runtime existe hors du domaine pur ;
3. l’adaptateur délègue encore à `RelaisManager` ;
4. aucun code actif ne l’instancie ;
5. aucune modification NVS n’est introduite ;
6. aucun driver V4 n’est activé ;
7. la prochaine étape peut décider du branchement dans `main.cpp`.

## 12. Suite recommandée

```text
AquaLook V4 — Phase 4 — Run 4.3
Branchement contrôlé du callback onRelayRequest vers EquipmentOutputRuntimeAdapter
```

Cette étape devra être accompagnée d’une compilation PlatformIO et d’un contrôle précis du diff de `main.cpp`.
