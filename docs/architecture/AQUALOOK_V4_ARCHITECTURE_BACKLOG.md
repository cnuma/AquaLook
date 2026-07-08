# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 4 — Run 4.4 compilation validée, 8 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose maintenant d’un contrat binaire, d’un driver simulé, d’un driver GPIO concret isolé, d’un driver XL9535 conditionnel isolé, d’un bootstrap non-runtime du registre de drivers et d’une validation PlatformIO complète réussie après ajout du bootstrap.

La Phase 4 démarre par la stratégie d’intégration runtime des sorties. La notion générique retenue côté domaine/runtime est `EquipmentOutput`. La terminologie `Relay` reste réservée au backend physique relais.

La cartographie de `RelaisManager` a confirmé que le point d’insertion le moins risqué est le callback runtime `onRelayRequest(zone, state)` dans `main.cpp`.

Le Run 4.2 a ajouté un adaptateur `EquipmentOutputRuntimeAdapter` passif. Le Run 4.3 l’instancie et branche le callback `onRelayRequest(zone, state)` vers cet adaptateur, qui délègue encore à `RelaisManager::setRelay(zone, state)`.

Le Run 4.4 corrige une collision de macro Arduino détectée à la compilation : `OperationError::DISABLED` est renommé `OperationError::TARGET_DISABLED`. La compilation PlatformIO complète est validée après correction.

## Validation PlatformIO complète Run 4.4

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:02:20.491
```

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

## Décisions du Run 3.6

- un plan `BinaryActuatorDriverBootstrapPlan` est ajouté ;
- le bootstrap enregistre les drivers demandés dans un registre fourni ;
- les contextes doivent être fournis explicitement par l’appelant ;
- aucun registre global n’est créé ;
- aucun driver n’est instancié automatiquement ;
- les drivers disponibles sont filtrés par le profil compilé ;
- les erreurs de registre sont propagées ;
- la compilation PlatformIO complète réussit ;
- aucun raccord à `RelaisManager` ou au runtime actif n’est introduit.

## Décisions du Run 4.1

- la couche domaine/runtime doit parler de `EquipmentOutput` plutôt que de relais ;
- la terminologie `Relay` reste limitée à la couche physique relais ;
- les relais restent le premier backend matériel, mais ne sont pas la notion centrale du runtime V4 ;
- `RelaisManager` reste en place tant que la stratégie de transition n’est pas validée ;
- le point d’insertion futur recommandé est `onRelayRequest(zone, state)` dans `main.cpp` ;
- les lectures Web/LCD de `RelaisManager::getState(zone)` devront être traitées séparément d’un premier adaptateur de commande ;
- aucune modification NVS n’est introduite ;
- aucun changement runtime n’est introduit.

## Décisions du Run 4.2

- les types génériques `EquipmentOutput` sont créés côté domaine pur ;
- l’adaptateur runtime est placé hors de `src/domain`, car il connaît `RelaisManager` ;
- l’adaptateur délègue encore les vannes de zone à `RelaisManager::setRelay(zone, state)` ;
- l’adaptateur expose une lecture d’état logique via `RelaisManager::getState(zone)` ;
- aucune modification NVS n’est introduite ;
- aucun changement matériel ou runtime actif n’est introduit.

## Décisions du Run 4.3

- `main.cpp` inclut désormais `EquipmentOutputRuntimeAdapter.h` ;
- une instance globale `outputAdapter` est ajoutée ;
- `outputAdapter.bind(&relaisMgr)` est appelé juste après `relaisMgr.begin(&configMgr)` ;
- `onRelayRequest(zone, state)` appelle désormais `outputAdapter.setZoneValve(zone, state, millis())` ;
- l’adaptateur délègue encore à `RelaisManager::setRelay(zone, state)` ;
- aucun changement NVS n’est introduit ;
- aucun driver V4 Phase 3 n’est activé directement.

## Décisions du Run 4.4

- la compilation locale a révélé une collision entre la macro Arduino `DISABLED` et `OperationError::DISABLED` ;
- `OperationError::DISABLED` est renommé `OperationError::TARGET_DISABLED` ;
- aucune valeur numérique de l’énumération n’est déplacée ;
- aucune logique runtime n’est modifiée ;
- aucune modification NVS n’est introduite ;
- la compilation PlatformIO complète réussit après correction.

Documents de référence :

```text
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_RUNTIME_INTEGRATION_STRATEGY.md
docs/architecture/AQUALOOK_V4_RELAISMANAGER_RUNTIME_CARTOGRAPHY.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_RUNTIME_ADAPTER.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_CALLBACK_INTEGRATION.md
```

## Prochaine étape

Poursuivre **AquaLook V4 — Phase 4 — Run 4.5 — stratégie de lecture d’état Web/LCD**.

Objectif immédiat : préparer la migration progressive des lectures `RelaisManager::getState(zone)` vers l’état logique `EquipmentOutput`, sans modifier NVS et sans changer encore les écrans Web/LCD tant que la stratégie n’est pas validée.
