# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 4 — Run 4.9 compilation validée, 9 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose maintenant d’un contrat binaire, d’un driver simulé, d’un driver GPIO concret isolé, d’un driver XL9535 conditionnel isolé, d’un bootstrap non-runtime du registre de drivers et d’une validation PlatformIO complète réussie après ajout du bootstrap.

La Phase 4 démarre par la stratégie d’intégration runtime des sorties. La notion générique retenue côté domaine/runtime est `EquipmentOutput`. La terminologie `Relay` reste réservée au backend physique relais.

Le Run 4.2 a ajouté un adaptateur `EquipmentOutputRuntimeAdapter` passif. Le Run 4.3 l’instancie et branche le callback `onRelayRequest(zone, state)` vers cet adaptateur, qui délègue encore à `RelaisManager::setRelay(zone, state)`.

Le Run 4.4 corrige la collision macro Arduino `DISABLED` / `OperationError::DISABLED`, renommé en `OperationError::TARGET_DISABLED`. La compilation PlatformIO complète est validée après correction. Les tests rapides utilisateur confirment que Web et LCD semblent encore fonctionner correctement.

Le Run 4.5 documente la stratégie de migration progressive des lectures d’état Web/LCD vers `EquipmentOutputRuntimeAdapter`.

Le Run 4.6 injecte passivement `EquipmentOutputRuntimeAdapter` dans `WebManager`, compilation validée.

Le Run 4.7 fait passer les lectures d’état Web existantes via une façade `OutputAwareRelayState`. Le JSON `/api/status` reste inchangé, compilation validée.

Le Run 4.8 injecte passivement `EquipmentOutputRuntimeAdapter` dans `DisplayManager`. Le pointeur est câblé depuis `main.cpp`, mais aucune lecture LCD n’est encore migrée.

Le Run 4.9 fait passer les lectures d’état LCD existantes via une façade `OutputAwareRelayState`. Cette approche intercepte les appels LCD existants à `_relais->getState(zone)`, avec lecture prioritaire `EquipmentOutputRuntimeAdapter::getZoneValveState(zone)` puis fallback `RelaisManager::getState(zone)`. La compilation PlatformIO est validée par l’utilisateur. Les métriques RAM/Flash n’ont pas été fournies dans le message de validation.

## Validation PlatformIO Run 4.9

```text
Compilation SUCCESS validée par l’utilisateur.
Métriques RAM/Flash non fournies dans le message de validation.
```

## Validation PlatformIO complète Run 4.7

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:03:07.759
```

```text
RAM:   20.6% — 67,392 / 327,680 octets
Flash: 62.6% — 1,272,505 / 2,031,616 octets
```

Capacité restante :

```text
RAM:   260,288 octets
Flash: 759,111 octets
```

Delta depuis Run 4.6 :

```text
RAM:   +0 octet
Flash: +128 octets
```

## Validation PlatformIO complète Run 4.6

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:00:20.060
```

```text
RAM:   20.6% — 67,392 / 327,680 octets
Flash: 62.6% — 1,272,377 / 2,031,616 octets
```

## Validation PlatformIO complète Run 4.4

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:02:20.491
```

```text
RAM:   20.6% — 67,384 / 327,680 octets
Flash: 62.6% — 1,272,369 / 2,031,616 octets
```

Le warning SdFat `__has_include(FS.h)` reste présent, non bloquant et sans lien avec les drivers V4.

## Validation PlatformIO complète Run 3.6

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:02:36.328
```

```text
RAM:   20.6% — 67,384 / 327,680 octets
Flash: 62.6% — 1,272,061 / 2,031,616 octets
```

## Décisions principales

| ID | Sujet | Statut |
|---|---|---|
| ARCH-001 à ARCH-012 | Domaine et inventaire matériel | **Clôturé architecturalement** |
| ARCH-013 | Actionneur binaire | **Contrat réalisé** |
| ARCH-014 | État sûr par port/actionneur | **Validé et appliqué par GPIO et XL9535** |
| ARCH-015 | Compatibilité `RelayAssignment` | Binding réalisé, intégration différée |
| ARCH-016 | Commandes idempotentes | **Validées** |
| ARCH-038 | CI et tests hôte | Workflow PlatformIO ajouté, tests hôte réalisés |
| ARCH-058 | Mesure PlatformIO | **Validée après bootstrap** |
| ARCH-059 | Mesure heap | Toujours requise avant intégration runtime |
| ARCH-070 | Drivers matériels conditionnels | GPIO et XL9535 réalisés isolément et compilés |
| ARCH-071 | Dépendances PlatformIO conditionnelles | Aucun ajout nécessaire pour GPIO/XL9535/bootstrap |
| ARCH-072 | Mesure du gain flash par profil | Toujours ouverte |
| ARCH-075 | Registre borné de drivers | **Réalisé et consolidé par bootstrap** |
| ARCH-076 | Driver binaire simulé | **Réalisé et validé** |
| ARCH-077 | Driver GPIO conditionnel | **Réalisé, testé hôte et compilé ESP32** |
| ARCH-078 | Driver XL9535 conditionnel | **Réalisé, testé hôte et compilé ESP32** |
| ARCH-079 | Synchronisation et concurrence | Ouvert |
| ARCH-080 | Politique de readback | Lecture explicite disponible |
| ARCH-082 | Adaptateur Arduino GPIO | **Isolé et compilé** |
| ARCH-083 | Collisions de macros Arduino | **Corrigées** |
| ARCH-084 | Adaptateur Arduino I²C/Wire | **Isolé et compilé** |
| ARCH-085 | Bootstrap non-runtime des drivers | **Réalisé, testé hôte et compilé ESP32** |
| ARCH-086 | Frontière `EquipmentOutput` / `Relay` | **Documentée** |
| ARCH-087 | Cartographie runtime `RelaisManager` | **Documentée** |
| ARCH-088 | Types domaine `EquipmentOutput` | **Ajoutés** |
| ARCH-089 | Adaptateur runtime `EquipmentOutput` passif | **Ajouté** |
| ARCH-090 | Branchement callback `onRelayRequest` | **Ajouté et compilé** |
| ARCH-091 | Collision macro Arduino `DISABLED` | **Corrigée** |
| ARCH-092 | Stratégie lecture état Web/LCD | **Documentée** |
| ARCH-093 | Injection passive `WebManager` | **Ajoutée et compilée** |
| ARCH-094 | Lecture état Web via `EquipmentOutput` | **Ajoutée et compilée** |
| ARCH-095 | Injection passive `DisplayManager` | **Ajoutée, compilation à valider** |
| ARCH-096 | Lecture état LCD via `EquipmentOutput` | **Ajoutée et compilée** |

## Décisions du Run 4.1

- la couche domaine/runtime doit parler de `EquipmentOutput` plutôt que de relais ;
- la terminologie `Relay` reste limitée à la couche physique relais ;
- les relais restent le premier backend matériel, mais ne sont pas la notion centrale du runtime V4 ;
- `RelaisManager` reste en place tant que la stratégie de transition n’est pas validée ;
- le point d’insertion futur recommandé est `onRelayRequest(zone, state)` dans `main.cpp` ;
- les lectures Web/LCD de `RelaisManager::getState(zone)` doivent être traitées séparément d’un premier adaptateur de commande ;
- aucune modification NVS n’est introduite.

## Décisions du Run 4.2

- les types génériques `EquipmentOutput` sont créés côté domaine pur ;
- l’adaptateur runtime est placé hors de `src/domain`, car il connaît `RelaisManager` ;
- l’adaptateur délègue encore les vannes de zone à `RelaisManager::setRelay(zone, state)` ;
- l’adaptateur expose une lecture d’état logique via `RelaisManager::getState(zone)` ;
- aucune modification NVS n’est introduite.

## Décisions du Run 4.3

- `main.cpp` inclut désormais `EquipmentOutputRuntimeAdapter.h` ;
- une instance globale `outputAdapter` est ajoutée ;
- `outputAdapter.bind(&relaisMgr)` est appelé juste après `relaisMgr.begin(&configMgr)` ;
- `onRelayRequest(zone, state)` appelle désormais `outputAdapter.setZoneValve(zone, state, millis())` ;
- l’adaptateur délègue encore à `RelaisManager::setRelay(zone, state)` ;
- aucun driver V4 Phase 3 n’est activé directement.

## Décisions du Run 4.4

- `OperationError::DISABLED` est renommé `OperationError::TARGET_DISABLED` ;
- aucune valeur numérique de l’énumération n’est déplacée ;
- aucune logique runtime n’est modifiée ;
- la compilation PlatformIO complète réussit après correction ;
- Web et LCD semblent encore fonctionner correctement après test utilisateur rapide.

## Décisions du Run 4.5

- les lectures Web/LCD de `RelaisManager::getState(zone)` sont cartographiées ;
- `WebManager::handleStatus()` publie encore `zones[].active` depuis `RelaisManager` ;
- `DisplayManager::update()` utilise encore `RelaisManager` pour `anyActive` ;
- `DisplayManager::handleTouchZone()` utilise encore `RelaisManager` pour choisir marche/arrêt manuel ;
- la cible de migration est `EquipmentOutputRuntimeAdapter::getZoneValveState(zone)` ;
- le fallback vers `RelaisManager` reste obligatoire.

## Décisions du Run 4.6

- `WebManager.h` déclare `EquipmentOutputRuntimeAdapter` ;
- `WebManager` expose `setOutputAdapter(...)` ;
- `WebManager` stocke un pointeur optionnel `_outputs` ;
- `main.cpp` appelle `webMgr.setOutputAdapter(&outputAdapter)` avant `webMgr.begin(...)` ;
- aucune route Web n’utilise encore `_outputs` ;
- `/api/status` reste inchangé ;
- la compilation PlatformIO complète réussit.

## Décisions du Run 4.7

- `WebManager.h` inclut désormais `EquipmentOutputRuntimeAdapter.h` ;
- le membre `_relais` de `WebManager` devient une façade `OutputAwareRelayState` ;
- `OutputAwareRelayState::getState(zone)` lit d’abord `EquipmentOutputRuntimeAdapter::getZoneValveState(zone)` ;
- si l’état est `VALID`, `BINARY`, la valeur binaire est utilisée ;
- sinon, fallback vers `RelaisManager::getState(zone)` ;
- `/api/status` conserve `zones[].active` en booléen ;
- la compilation PlatformIO complète réussit.

## Décisions du Run 4.8

- `DisplayManager.h` déclare `EquipmentOutputRuntimeAdapter` ;
- `DisplayManager` expose `setOutputAdapter(...)` ;
- `DisplayManager` stocke un pointeur optionnel `_outputs` ;
- `main.cpp` appelle `displayMgr.setOutputAdapter(&outputAdapter)` avant `displayMgr.begin(...)` ;
- aucune lecture LCD n’utilise encore `_outputs` ;
- `DisplayManager.cpp` n’est pas modifié.

## Décisions du Run 4.9

- `DisplayManager.h` inclut désormais `EquipmentOutputRuntimeAdapter.h` ;
- le membre `_relais` de `DisplayManager` devient une façade `OutputAwareRelayState` ;
- `OutputAwareRelayState::getState(zone)` lit d’abord `EquipmentOutputRuntimeAdapter::getZoneValveState(zone)` ;
- si l’état est `VALID`, `BINARY`, la valeur binaire est utilisée ;
- sinon, fallback vers `RelaisManager::getState(zone)` ;
- `DisplayManager.cpp` n’est pas modifié ;
- les appels LCD existants à `_relais->getState(zone)` passent désormais par la façade ;
- la portée réelle est plus large que le seul `anyActive`, mais le comportement reste équivalent tant que l’adaptateur délègue à `RelaisManager` ;
- aucune modification NVS n’est introduite ;
- aucune modification Web n’est introduite ;
- la compilation PlatformIO est validée.

Documents de référence :

```text
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_RUNTIME_INTEGRATION_STRATEGY.md
docs/architecture/AQUALOOK_V4_RELAISMANAGER_RUNTIME_CARTOGRAPHY.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_RUNTIME_ADAPTER.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_CALLBACK_INTEGRATION.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_STATE_READ_STRATEGY.md
docs/architecture/AQUALOOK_V4_WEBMANAGER_OUTPUT_ADAPTER_INJECTION.md
docs/architecture/AQUALOOK_V4_WEB_STATUS_OUTPUT_STATE_READ.md
docs/architecture/AQUALOOK_V4_DISPLAYMANAGER_OUTPUT_ADAPTER_INJECTION.md
docs/architecture/AQUALOOK_V4_LCD_OUTPUT_STATE_READ.md
```

## Prochaine étape

Effectuer un test rapide Web/LCD :

```text
/api/status
/api/storage
page principale
LCD veille/réveil
état zone active LCD/Web
```

Si le test est bon, créer un checkpoint de fin de séquence Web/LCD lectures d’état.
