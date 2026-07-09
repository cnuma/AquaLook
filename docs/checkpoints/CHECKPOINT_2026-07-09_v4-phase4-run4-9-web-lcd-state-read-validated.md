# CHECKPOINT — AquaLook V4 — Phase 4 — Run 4.9

**Date :** 9 juillet 2026  
**Branche :** `feature/aqualook-v4-domain`  
**Statut :** figé après compilation et test rapide Web/LCD utilisateur  
**Source de vérité :** dépôt GitHub `cnuma/AquaLook`, branche `feature/aqualook-v4-domain`

## Résumé

Ce checkpoint fige la fin de la sous-séquence Phase 4 consacrée à la migration progressive des lectures d’état Web/LCD vers la notion domaine/runtime `EquipmentOutput`.

État validé :

- `ScheduleManager` commande toujours les zones via callback runtime ;
- le callback passe par `EquipmentOutputRuntimeAdapter::setZoneValve(...)` ;
- l’adaptateur délègue encore à `RelaisManager::setRelay(...)` ;
- `/api/status` lit maintenant les états via une façade `OutputAwareRelayState` côté `WebManager` ;
- le LCD lit maintenant les états via une façade `OutputAwareRelayState` côté `DisplayManager` ;
- les deux façades utilisent d’abord `EquipmentOutputRuntimeAdapter::getZoneValveState(zone)` ;
- fallback strict vers `RelaisManager::getState(zone)` ;
- aucun changement JSON ;
- aucun changement NVS ;
- aucun driver V4 réel n’est branché au runtime actif.

## Dernier HEAD connu avant création du checkpoint

```text
4ff6ae32ef6ceb97822b95e608581bf85ddf10aa
docs: record Run 4.9 memory metrics and functional validation
```

Le commit de ce checkpoint est créé après ce HEAD.

## Commits clés de la séquence

```text
6833f3326ec75c0086b83f9954642aaf1f43c061
feat: route Web relay state reads through output adapter

9ad04614b3a40a883edd64a55a01d4e93a62d31c
docs: record Run 4.7 compile validation

6efdf34dc9f69f74fee09f27ffaec244d41ecc9b
feat: add passive output adapter injection to DisplayManager

e34f59bd5c503195f4b35ec8bf2b472005b5f7cc
feat: wire passive output adapter into DisplayManager

2dd137201e3458f09574c37ba533f16df0c19798
feat: route LCD relay state reads through output adapter

33c72f9f37bb7a68705ecd2e69fe93fd1b222e51
docs: record LCD output state read migration

0640322faa769b0e24bfc3e4e49550a12fac953a
docs: record Run 4.9 compile validation

4ff6ae32ef6ceb97822b95e608581bf85ddf10aa
docs: record Run 4.9 memory metrics and functional validation
```

## Validation PlatformIO Run 4.9

Compilation validée par l’utilisateur.

Métriques :

```text
RAM:   20.6% — 67,400 / 327,680 octets
Flash: 62.6% — 1,272,705 / 2,031,616 octets
```

Capacité restante :

```text
RAM:   260,280 octets
Flash: 758,911 octets
```

Delta depuis Run 4.7 :

```text
RAM:   +8 octets
Flash: +200 octets
```

## Validation fonctionnelle utilisateur

Validation utilisateur :

```text
c'est ok, on fige.
```

Tests validés :

```text
/api/status OK
/api/storage OK
page principale OK après réinsertion SD
LCD veille/réveil OK
état zone active LCD/Web OK
```

## Incident ponctuel SD observé

Symptôme : appel page Web retournant `Not found`.

État `/api/storage` observé ensuite :

```text
status              ready
message             Carte SD operationnelle, ressources Web disponibles.
sdAvailable         true
webAssetsAvailable  true
cardType            SDHC/SDXC
capacityBytes       31914983424
```

Après retrait/remise de la carte SD, la page principale a fonctionné de nouveau.

Décision :

- aucun correctif code appliqué ;
- conserver comme point de vigilance matériel/runtime SD ;
- ne pas mélanger cet incident avec la migration `EquipmentOutput`.

## État runtime après checkpoint

Commande runtime :

```text
ScheduleManager
  -> onRelayRequest(zone, state)
  -> EquipmentOutputRuntimeAdapter::setZoneValve(zone, state, millis())
  -> RelaisManager::setRelay(zone, state)
  -> RelayTopology legacy compatible
  -> écriture I2C historique
```

Lecture Web :

```text
/api/status
  -> WebManager::_relais.getState(zone)
  -> EquipmentOutputRuntimeAdapter::getZoneValveState(zone)
  -> fallback RelaisManager::getState(zone)
```

Lecture LCD :

```text
DisplayManager::_relais.getState(zone)
  -> EquipmentOutputRuntimeAdapter::getZoneValveState(zone)
  -> fallback RelaisManager::getState(zone)
```

## Fichiers code modifiés dans la séquence

### `src/WebManager.h`

- include `EquipmentOutputRuntimeAdapter.h` ;
- façade `OutputAwareRelayState` ;
- `_relais` remplacé par une façade compatible avec les usages existants ;
- `setOutputAdapter(...)` renseigne `_outputs` et la façade.

`WebManager.cpp` conserve ses appels historiques à `_relais->getState(zone)`.

### `src/DisplayManager.h`

- include `EquipmentOutputRuntimeAdapter.h` ;
- façade `OutputAwareRelayState` ;
- `_relais` remplacé par une façade compatible avec les usages existants ;
- `setOutputAdapter(...)` renseigne `_outputs` et la façade.

`DisplayManager.cpp` n’est pas modifié dans cette séquence.

### `src/main.cpp`

Ajouts de câblage :

```cpp
webMgr.setOutputAdapter(&outputAdapter);
displayMgr.setOutputAdapter(&outputAdapter);
```

## Documentation liée

```text
docs/architecture/AQUALOOK_V4_WEB_STATUS_OUTPUT_STATE_READ.md
docs/architecture/AQUALOOK_V4_DISPLAYMANAGER_OUTPUT_ADAPTER_INJECTION.md
docs/architecture/AQUALOOK_V4_LCD_OUTPUT_STATE_READ.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_RUNTIME_INTEGRATION_STRATEGY.md
docs/architecture/AQUALOOK_V4_RELAISMANAGER_RUNTIME_CARTOGRAPHY.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_RUNTIME_ADAPTER.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_CALLBACK_INTEGRATION.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_STATE_READ_STRATEGY.md
docs/architecture/AQUALOOK_V4_WEBMANAGER_OUTPUT_ADAPTER_INJECTION.md
```

## Invariants préservés

1. Aucun changement NVS.
2. Aucun changement format JSON.
3. Aucun changement nom de champ `/api/status`.
4. Aucun changement `zones[].active`.
5. Aucun changement planning.
6. Aucun changement tactile.
7. Aucun changement cadence refresh LCD.
8. Aucun changement veille écran.
9. Aucun driver V4 réel activé dans le runtime actif.
10. Fallback `RelaisManager` conservé pour Web et LCD.
11. Compatibilité historique `Zone N -> carte 0 -> voie N` conservée.
12. `RelaisManager::update()` reste en place.
13. `ScheduleManager` reste le pilote runtime de l’arrosage.
14. `DisplayManager.cpp` n’est pas réécrit dans cette séquence.
15. `WebManager.cpp` n’est pas réécrit dans cette séquence.

## Limites connues

- Les façades `OutputAwareRelayState` existent à la fois côté Web et LCD ; une factorisation ultérieure pourra être envisagée, mais pas avant stabilisation complète.
- `EquipmentOutputRuntimeAdapter` délègue encore au backend historique `RelaisManager`.
- Les drivers V4 Phase 3 ne sont pas encore branchés au runtime actif.
- L’incident `Not found` ponctuel sur page Web semble lié à la SD ou à son accès runtime, pas à la migration `EquipmentOutput`.
- Les lectures LCD ont été migrées par interception de tous les appels `_relais->getState(zone)` côté `DisplayManager`, pas uniquement `anyActive`.

## Commandes de reprise

```powershell
git checkout feature/aqualook-v4-domain
git pull --ff-only
pio run -e ProgrammeArrosage
```

## Prochaine étape recommandée

```text
AquaLook V4 — Phase 4 — Run 4.10
Plan du prochain raccord runtime après validation Web/LCD lectures d’état
```

Objectifs :

- décider si l’on consolide/factorise les façades de lecture ;
- décider si l’on migre une partie du backend physique vers drivers V4 ;
- identifier le prochain point de raccord sans toucher NVS ;
- préserver `RelaisManager` comme fallback tant que le nouveau chemin matériel n’est pas éprouvé ;
- éviter tout remplacement massif de fichiers sensibles.
