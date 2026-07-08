# CHECKPOINT — AquaLook V4 — Phase 4 Run 4.4

**Date :** 8 juillet 2026  
**Branche :** `feature/aqualook-v4-domain`  
**Statut :** compilation PlatformIO validée après branchement `EquipmentOutputRuntimeAdapter`

## 1. Résumé

La Phase 4 a introduit progressivement la frontière runtime `EquipmentOutput` sans remplacer le backend historique `RelaisManager`.

Le Run 4.4 valide la compilation complète après :

1. ajout des types `EquipmentOutput` ;
2. ajout de l’adaptateur runtime passif ;
3. branchement du callback `onRelayRequest(zone, state)` via l’adaptateur ;
4. correction d’une collision macro Arduino `DISABLED`.

## 2. Source de vérité

Dernier HEAD attendu après checkpoint : à vérifier après commit de ce fichier.

Commits importants de la séquence :

```text
2b42ce8965d3d3c3c1b39c08401eb1cc8fc29297
feat: add generic equipment output domain types

2a756c72e320f4fba7a2f2617c4dd45152a5019a
feat: add passive equipment output runtime adapter header

5012517a29708456cfe4df8a34a65d3c03769c89
feat: add passive equipment output runtime adapter implementation

4c28411f97d3c06fc1e8525fe9bbc2bbc1ea5805
feat: route relay callback through EquipmentOutput adapter

2150d26a307907fc26bcea846a620d71bf818a8d
fix: avoid Arduino DISABLED macro collision

e0a81a1310f69a2468c2a7a0dbf035dbdbf17211
docs: record Run 4.4 compile validation
```

## 3. Validation compilation

Commande exécutée par l’utilisateur :

```powershell
pio run -e ProgrammeArrosage
```

Résultat :

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:02:20.491
```

Mémoire :

```text
RAM:   20.6% — 67,384 / 327,680 octets
Flash: 62.6% — 1,272,369 / 2,031,616 octets
```

Capacité restante :

```text
RAM:   260,296 octets
Flash: 759,247 octets
```

Delta depuis Run 3.6 :

```text
RAM:   +0 octet
Flash: +308 octets
```

## 4. Warning connu

Le warning SdFat reste présent :

```text
#warning File not defined because __has_include(FS.h)
```

Statut : connu, non bloquant, sans lien avec le branchement `EquipmentOutput`.

## 5. Fichiers ajoutés

```text
src/domain/EquipmentOutputTypes.h
src/EquipmentOutputRuntimeAdapter.h
src/EquipmentOutputRuntimeAdapter.cpp
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_RUNTIME_ADAPTER.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_CALLBACK_INTEGRATION.md
```

## 6. Fichiers modifiés

```text
src/main.cpp
src/domain/EquipmentRuntimeState.h
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
```

## 7. Chaîne runtime validée à la compilation

```text
ScheduleManager
  -> onRelayRequest(zone, state)
  -> EquipmentOutputRuntimeAdapter::setZoneValve(zone, state, millis())
  -> RelaisManager::setRelay(zone, state)
  -> RelayTopology legacy compatible
  -> écriture I2C historique Wire
```

## 8. Invariants préservés

1. Pas de modification NVS.
2. Pas de migration de configuration.
3. Pas de remplacement de `RelaisManager`.
4. Pas d’activation runtime automatique des drivers V4 Phase 3.
5. Pas de changement volontaire du mapping legacy.
6. Compatibilité maintenue : `Zone N -> carte 0 -> voie N`.
7. `ScheduleManager` continue de commander via callback.
8. Web et LCD ne sont pas modifiés dans ce run.
9. Le backend matériel reste `RelaisManager` + `Wire` historique.
10. Les commandes manuelles Web/LCD passent toujours par `ScheduleManager`.

## 9. Correction Run 4.4

Erreur rencontrée : collision entre la macro Arduino :

```cpp
#define DISABLED 0x00
```

et :

```cpp
OperationError::DISABLED
```

Correction :

```cpp
OperationError::TARGET_DISABLED
```

La valeur numérique reste `3`.

## 10. Risques et limites

### 10.1 Statut optimiste de l’adaptateur

`RelaisManager::setRelay()` ne retourne pas de booléen.

L’adaptateur retourne donc `OperationStatus::APPLIED` après délégation appelée.

Ce résultat n’est pas encore exploité par le runtime.

### 10.2 Tests matériels non réalisés dans ce checkpoint

La compilation est validée, mais les tests matériels restent à effectuer :

- démarrage carte ;
- scan I2C ;
- activation manuelle d’une zone ;
- arrêt manuel ;
- activation planifiée ;
- vérification affichage Web/LCD ;
- absence de défaut relais I2C inattendu.

## 11. Prochaine étape recommandée

```text
AquaLook V4 — Phase 4 — Run 4.5 — stratégie de lecture d’état Web/LCD
```

Objectif : préparer la migration progressive des lectures `RelaisManager::getState(zone)` vers l’état logique `EquipmentOutput`, sans modifier NVS et sans changer encore les écrans Web/LCD tant que la stratégie n’est pas validée.

## 12. Commandes de reprise

```powershell
git pull --ff-only
pio run -e ProgrammeArrosage
```

Ne pas poursuivre vers une migration Web/LCD active tant que les tests matériels de base du Run 4.4 n’ont pas été observés sur carte.
