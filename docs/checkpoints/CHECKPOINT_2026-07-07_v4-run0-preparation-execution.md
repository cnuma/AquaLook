# AquaLook V4 — Checkpoint Phase 0 Run 0 — Préparation d’exécution

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/relay-board-mapping`  
**Base initiale du chantier V4 :** `b40c63720bc95a362bced9f26f3f78afaac76804`  
**Tag initial :** `relay-topology-v1`  
**HEAD de clôture :** commit contenant ce checkpoint et indiqué dans la transmission du run

## 1. Objet du run

Préparer l’exécution structurée d’AquaLook V4 sans produire ni modifier de code runtime.

Le run devait transformer l’architecture cible en programme de travail concret comprenant :

- cartographie de l’existant vers la cible ;
- backlog des décisions d’architecture ;
- plan d’exécution par phases ;
- protocole de segmentation des chats ;
- prochaine action unique.

## 2. Documents de référence

- `docs/architecture/AQUALOOK_V4_TARGET_ARCHITECTURE.md`
- `docs/architecture/RELAY_TOPOLOGY.md`
- `docs/architecture/EQUIPMENT_MODEL_ROADMAP.md`
- `docs/process/AQUALOOK_CHAT_SEGMENTATION_PROTOCOL.md`
- `docs/checkpoints/CHECKPOINT_2026-07-06_relay-assignment-roles_COMPILE_OK.md`

## 3. Documents produits pendant la préparation V4

- `docs/architecture/AQUALOOK_V4_CURRENT_TO_TARGET_MAPPING.md`
- `docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md`
- `docs/architecture/AQUALOOK_V4_PHASE_PLAN.md`
- `docs/checkpoints/CHECKPOINT_2026-07-07_v4-run0-preparation-execution.md`

## 4. État validé à préserver

- `RelayTopology` existe ;
- `RelayAssignment` est généralisé par rôles ;
- `RelaisManager` résout les sorties par la topologie ;
- une voie relais n’est plus assimilée conceptuellement à une zone ;
- compatibilité actuelle : `Zone N -> carte 0 -> voie N` ;
- compilation du checkpoint précédent validée par l’utilisateur ;
- persistance NVS volontairement inchangée ;
- moteur d’arrosage, météo, pluie, intervalle, Web, LCD, SD et LittleFS non modifiés par ce run.

## 5. Décisions prises

1. L’architecture cible n’est pas détaillée intégralement avant le besoin.
2. Les questions ouvertes sont gérées dans un backlog.
3. Les décisions structurantes sont prises par ADR au début du run qui en dépend.
4. Les phases sont découpées en runs courts, compilables et checkpointés.
5. Le changement de phase ou la saturation du chat impose une transmission structurée.
6. La Phase 1 commence par une inspection source ciblée, pas par du code.
7. Le modèle V4 est d’abord isolé en mémoire.
8. Le NVS V4 ne sera conçu qu’en Phase 7 après stabilisation du domaine.

## 6. Cartographie synthétique

```text
ScheduleManager actuel
    -> futur AutomationEngine pour la décision
    -> futur EquipmentOrchestrator pour l’exécution

RelaisManager actuel
    -> adaptateur matériel d’un actionneur binaire

RelayTopology actuel
    -> première implémentation de HardwareAssignment

ConfigManager actuel
    -> progressivement séparé entre service de configuration et adaptateur NVS

WebManager / DisplayManager
    -> adaptateurs vers des services applicatifs communs
```

## 7. Backlog immédiat

Les décisions P0 de la Phase 1 sont :

- ARCH-001 — identifiants stables ;
- ARCH-002 — limites et budget mémoire ;
- ARCH-003 — paramètres spécifiques des équipements ;
- ARCH-004 — types et capacités.

Elles ne doivent être traitées qu’après l’inspection source du Run 1.0.

## 8. Plan de la Phase 1

- Run 1.0 — inspection source ciblée ;
- Run 1.1 — identités et capacités maximales ;
- Run 1.2 — modèle Equipment minimal ;
- Run 1.3 — états, défauts et résultats ;
- Run 1.4 — intentions ;
- Run 1.5 — exécutions ;
- Run 1.6 — dépendances ;
- Run 1.7 — adaptateur de compatibilité en mémoire ;
- Run 1.8 — revue et validation de phase.

## 9. Fichiers source modifiés

Aucun.

## 10. Fichiers volontairement non modifiés

- tout `src/` ;
- tout `include/` ;
- `platformio.ini` ;
- `data/` ;
- schéma NVS ;
- API Web ;
- interface Web ;
- LCD ;
- moteur d’arrosage ;
- logique relais runtime.

## 11. Compilation et tests

### Compilation

Non exécutée : run exclusivement documentaire.

La dernière compilation connue reste celle validée dans :

```text
docs/checkpoints/CHECKPOINT_2026-07-06_relay-assignment-roles_COMPILE_OK.md
```

### Tests matériels

Aucun test matériel requis pour ce run.

## 12. Risques et limites

- la cartographie actuelle est encore macroscopique ;
- les références précises aux fichiers, structures et fonctions doivent être ajoutées pendant le Run 1.0 ;
- les limites mémoire ne sont pas encore mesurées ;
- aucun identifiant stable V4 n’est encore choisi ;
- aucun modèle V4 n’est encore implémenté ;
- l’état local du dépôt de l’utilisateur doit être synchronisé par `git pull --ff-only`.

## 13. Prochaine action unique

Démarrer **AquaLook V4 — Phase 1 — Run 1.0 — Inspection source ciblée**.

Le run doit :

- inspecter le dépôt réel sur la branche de reprise ;
- inventorier les structures de zones, programmes et états ;
- identifier les dépendances de `ConfigManager`, `ScheduleManager` et `RelaisManager` ;
- relever les limites, tableaux et tailles ;
- cartographier les appels manuels, automatiques et matériels ;
- mettre à jour `AQUALOOK_V4_CURRENT_TO_TARGET_MAPPING.md` ;
- ne produire aucun code V4.

## 14. Commandes de reprise

```powershell
git fetch origin
git switch feature/relay-board-mapping
git pull --ff-only
git status
git log -1 --oneline
```

Le working tree local doit être propre avant le Run 1.0.

## 15. Message de reprise recommandé

```text
Projet AquaLook V4 — Phase 1 — Run 1.0 — Inspection source ciblée

Base de travail :
- Dépôt : cnuma/AquaLook
- Branche : feature/relay-board-mapping
- HEAD local et distant : utiliser le commit exact du checkpoint Run 0 après git pull
- Working tree : propre attendu

Documents de référence :
- docs/architecture/AQUALOOK_V4_TARGET_ARCHITECTURE.md
- docs/architecture/AQUALOOK_V4_CURRENT_TO_TARGET_MAPPING.md
- docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
- docs/architecture/AQUALOOK_V4_PHASE_PLAN.md
- docs/process/AQUALOOK_CHAT_SEGMENTATION_PROTOCOL.md
- docs/checkpoints/CHECKPOINT_2026-07-07_v4-run0-preparation-execution.md

État validé à préserver :
- aucun changement runtime dans le Run 0 ;
- RelayTopology et RelayAssignment conservés ;
- compatibilité Zone N -> carte 0 -> voie N ;
- NVS, Web, LCD, météo, pluie et intervalle inchangés.

Objectif unique :
Inspecter les sources réelles et compléter la cartographie avec les fichiers, structures, fonctions, appels et limites mémoire, sans produire de code V4.
```
