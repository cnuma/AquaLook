# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 4 — Run 4.10 plan du prochain raccord runtime, 9 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose maintenant d’un contrat binaire, d’un driver simulé, d’un driver GPIO concret isolé, d’un driver XL9535 conditionnel isolé, d’un bootstrap non-runtime du registre de drivers et d’une validation PlatformIO complète réussie après ajout du bootstrap.

La Phase 4 démarre par la stratégie d’intégration runtime des sorties. La notion générique retenue côté domaine/runtime est `EquipmentOutput`. La terminologie `Relay` reste réservée au backend physique relais.

Le Run 4.9 fige la migration des lectures d’état Web/LCD via `EquipmentOutputRuntimeAdapter`, avec fallback `RelaisManager`, compilation validée et test rapide Web/LCD validé.

Le Run 4.10 documente le prochain raccord runtime recommandé : ne pas brancher les drivers V4 réels immédiatement, mais préparer d’abord une frontière passive `RelayPhysicalBackend`, avec `RelaisManager` comme implémentation active.

## Validation PlatformIO complète Run 4.9

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

## Validation fonctionnelle Run 4.9

Test rapide validé par l’utilisateur :

```text
/api/status OK
/api/storage OK
page principale OK après réinsertion SD
LCD veille/réveil OK
état zone active LCD/Web OK
```

Observation SD : un appel de page a retourné temporairement `Not found`. `/api/storage` indiquait ensuite :

```text
status              ready
message             Carte SD operationnelle, ressources Web disponibles.
sdAvailable         true
webAssetsAvailable  true
cardType            SDHC/SDXC
capacityBytes       31914983424
```

Après retrait/remise de la carte SD, la page a de nouveau fonctionné. Aucun correctif code n’est appliqué à ce stade ; conserver comme point de vigilance matériel/runtime SD.

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
| ARCH-097 | Plan prochain raccord runtime | **Documenté** |

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
- la compilation PlatformIO est validée ;
- le test rapide Web/LCD est validé.

## Décisions du Run 4.10

- ne pas brancher directement les drivers V4 Phase 3 dans le runtime actif ;
- différer la factorisation des façades Web/LCD pour éviter de toucher une séquence fraîchement validée ;
- préparer d’abord une frontière runtime passive `RelayPhysicalBackend` ;
- garder `RelaisManager` comme implémentation active du backend physique ;
- conserver les drivers V4 réels hors runtime actif jusqu’à validation de cette frontière ;
- ne pas toucher NVS.

Documents de référence :

```text
docs/checkpoints/CHECKPOINT_2026-07-09_v4-phase4-run4-9-web-lcd-state-read-validated.md
docs/architecture/AQUALOOK_V4_RUNTIME_BRIDGE_NEXT_STEP_PLAN.md
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

```text
AquaLook V4 — Phase 4 — Run 4.11
Interface passive RelayPhysicalBackend + adaptateur RelaisManagerBackend
```

Invariants du Run 4.11 :

- aucun changement NVS ;
- aucun changement Web ;
- aucun changement LCD ;
- aucun changement JSON ;
- aucun driver V4 réel activé ;
- `RelaisManager` reste l’implémentation active ;
- compilation PlatformIO obligatoire.
