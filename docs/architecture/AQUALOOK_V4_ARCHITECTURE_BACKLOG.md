# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 4 — Runs 4.11 à 4.13 compilation validée, 10 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose maintenant d’un contrat binaire, d’un driver simulé, d’un driver GPIO concret isolé, d’un driver XL9535 conditionnel isolé, d’un bootstrap non-runtime du registre de drivers et d’une validation PlatformIO complète réussie après ajout du bootstrap.

La Phase 4 démarre par la stratégie d’intégration runtime des sorties. La notion générique retenue côté domaine/runtime est `EquipmentOutput`. La terminologie `Relay` reste réservée au backend physique relais.

Le Run 4.9 fige la migration des lectures d’état Web/LCD via `EquipmentOutputRuntimeAdapter`, avec fallback `RelaisManager`, compilation validée et test rapide Web/LCD validé.

Le Run 4.10 documente le prochain raccord runtime recommandé : ne pas brancher les drivers V4 réels immédiatement, mais préparer d’abord une frontière passive `RelayPhysicalBackend`, avec `RelaisManager` comme implémentation active.

Le Run 4.11 ajoute l’interface passive `RelayPhysicalBackend` et l’adaptateur `RelaisManagerBackend`.

Le Run 4.12 injecte passivement `RelayPhysicalBackend` dans `EquipmentOutputRuntimeAdapter`, avec fallback direct `RelaisManager` conservé.

Le Run 4.13 câble `RelaisManagerBackend` dans `main.cpp`. Le chemin actif passe maintenant par la frontière `RelayPhysicalBackend`, tandis que `outputAdapter.bind(&relaisMgr)` conserve le fallback direct.

Les Runs 4.11, 4.12 et 4.13 compilent ensemble avec succès.

## Validation PlatformIO complète Runs 4.11 à 4.13

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:03:37.415
```

```text
RAM:   20.6% — 67,416 / 327,680 octets
Flash: 62.7% — 1,273,005 / 2,031,616 octets
```

Capacité restante :

```text
RAM:   260,264 octets
Flash: 758,611 octets
```

Delta depuis Run 4.9 :

```text
RAM:   +16 octets
Flash: +300 octets
```

Le firmware binaire a été généré avec succès par `esptool.py v4.11.0`.

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
| ARCH-095 | Injection passive `DisplayManager` | **Ajoutée et compilée** |
| ARCH-096 | Lecture état LCD via `EquipmentOutput` | **Ajoutée et compilée** |
| ARCH-097 | Plan prochain raccord runtime | **Documenté** |
| ARCH-098 | Interface passive `RelayPhysicalBackend` | **Ajoutée et compilée** |
| ARCH-099 | Injection passive `RelayPhysicalBackend` dans `EquipmentOutputRuntimeAdapter` | **Ajoutée et compilée** |
| ARCH-100 | Câblage passif `RelaisManagerBackend` dans `main.cpp` | **Ajouté et compilé** |

## Décisions du Run 4.11

- ajout de `src/RelayPhysicalBackend.h` ;
- ajout de `src/RelaisManagerBackend.h` ;
- ajout de `src/RelaisManagerBackend.cpp` ;
- `RelayPhysicalBackend` expose `setZoneValve(...)` et `getZoneValveState(...)` ;
- `RelaisManagerBackend` adapte `RelaisManager::setRelay(...)` et `RelaisManager::getState(...)` ;
- aucun driver V4 réel n’est activé ;
- les fallbacks Web/LCD restent en place.

## Décisions du Run 4.12

- `EquipmentOutputRuntimeAdapter` expose `setPhysicalBackend(...)` ;
- `setZoneValve(...)` essaie `_physicalBackend` puis le fallback direct `_relayManager` ;
- `getZoneValveState(...)` essaie `_physicalBackend` puis le fallback direct `_relayManager` ;
- aucun driver V4 réel n’est activé ;
- aucun changement NVS, Web, LCD ou JSON.

## Décisions du Run 4.13

- `main.cpp` inclut `RelaisManagerBackend.h` ;
- une instance globale `AquaLook::Runtime::RelaisManagerBackend relaisBackend` est ajoutée ;
- `relaisBackend.bind(&relaisMgr)` est appelé après `relaisMgr.begin(&configMgr)` ;
- `outputAdapter.setPhysicalBackend(&relaisBackend)` est appelé ;
- `outputAdapter.bind(&relaisMgr)` est conservé comme fallback direct ;
- aucun driver V4 réel n’est activé ;
- aucun changement NVS, Web, LCD ou JSON ;
- compilation PlatformIO validée.

Documents de référence :

```text
docs/checkpoints/CHECKPOINT_2026-07-09_v4-phase4-run4-9-web-lcd-state-read-validated.md
docs/architecture/AQUALOOK_V4_RUNTIME_BRIDGE_NEXT_STEP_PLAN.md
docs/architecture/AQUALOOK_V4_RELAY_PHYSICAL_BACKEND.md
docs/architecture/AQUALOOK_V4_OUTPUT_ADAPTER_PHYSICAL_BACKEND_INJECTION.md
docs/architecture/AQUALOOK_V4_RELAISMANAGER_BACKEND_WIRING.md
```

## Prochaine étape

Effectuer un test rapide de non-régression :

```text
/api/status
page principale
LCD veille/réveil
commande manuelle zone
état zone active Web/LCD
```

Ensuite, deux trajectoires possibles :

```text
AquaLook V4 — Phase 4 — checkpoint de la transition backend
```

ou

```text
AquaLook V4 — Phase 5 — Tests et validation automatisée
Run 5.1 — Stratégie et matrice de tests
```
