# AquaLook V4 — Checkpoint Phase 1 Run 1.0 — Inspection source ciblée

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/relay-board-mapping`  
**Base inspectée :** `8b0625fadcc684495625593945fed5b154634d89`  
**Commit cartographie détaillée :** `76151cd4095c1a12ef38bff15283ab51dcbd8a21`  
**HEAD de clôture :** commit contenant le présent checkpoint

## 1. Objet

Inspecter les sources réelles afin de compléter la cartographie V4 avant toute création de code de domaine.

Le run devait identifier :

- structures de configuration ;
- structures de planning ;
- états runtime ;
- appels automatiques et manuels ;
- chemin matériel ;
- limites de capacité ;
- contraintes mémoire préliminaires ;
- couplages à réduire.

## 2. Fichiers inspectés

- `src/main.cpp`
- `src/ConfigManager.h`
- `src/ConfigManager.cpp`
- `src/ScheduleManager.h`
- `src/ScheduleManager.cpp`
- `src/RelaisManager.h`
- `src/RelaisManager.cpp`
- `src/RelayTopology.h`
- `src/WebManager.h`
- `include/config.h`
- documentation V4 et checkpoints précédents

## 3. Constats structurants

### 3.1 Duplication des zones

Deux représentations complètes coexistent :

```text
ConfigManager::_zones[MAX_ZONES]
ScheduleManager::_zones[MAX_ZONES]
```

Les deux sont dimensionnées à 16 zones et réservent 40 créneaux par zone.

Le modèle V4 initial ne doit pas introduire une troisième copie complète.

### 3.2 ScheduleManager cumule quatre responsabilités

Il porte :

1. configuration runtime du planning ;
2. évaluation jours / intervalle / pluie ;
3. état d’exécution ;
4. commande du relais par callback.

La Phase 1 doit modéliser ces concepts séparément sans modifier le comportement courant.

### 3.3 État logique avant succès matériel

`ScheduleManager::activateZone()` marque la zone active avant l’appel au callback.

`RelaisManager::setRelay()` marque également `_state[zone]` avant validation du mapping et avant succès I²C.

Cela confirme la nécessité de distinguer :

```text
état demandé
état autorisé
état appliqué
état observé
```

### 3.4 Persistance fortement couplée à la taille de structure

`PersistedConfig` contient directement `CfgZone zones[MAX_ZONES]`.

`loadNvs()` exige :

```text
longueur du blob == sizeof(PersistedConfig)
```

Le schéma est `CFG_NVS_SCHEMA = 1`.

Aucune structure persistée ne doit être modifiée pendant la Phase 1.

### 3.5 Contraintes historiques du nombre de zones

Valeurs constatées :

```text
MAX_ZONES = 16
MAX_ACTIVE_ZONES = 8
NB_ZONES = 2
```

`normalizeActiveZones()` impose en plus une valeur paire de 2 à 8 pour le XL9535.

Cette contrainte matérielle ne doit pas devenir une règle métier V4.

### 3.6 Topologie relais déjà exploitable

La branche possède :

```text
MAX_RELAY_BOARDS = 8
MAX_CHANNELS_PER_BOARD = 8
MAX_RELAY_ASSIGNMENTS = 16
```

Les rôles pompe, auxiliaire, serre et éclairage existent déjà, mais `role + targetIndex` reste un mécanisme de transition et non une identité d’équipement.

### 3.7 WebManager fortement dépendant

`WebManager` reçoit directement six managers et porte des handlers de planning, manuel, système, réseau, météo, affichage et défauts.

Ce couplage est documenté mais volontairement hors périmètre de la Phase 1.

## 4. Contraintes mémoire préliminaires

Les tailles exactes restent à mesurer par `sizeof()` sur la cible.

Constats certains :

- deux grands tableaux de zones sont réservés globalement ;
- 16 objets `String` sont conservés dans `ScheduleManager` ;
- `loadNvs()` alloue temporairement un bloc de la taille de `PersistedConfig` ;
- les futurs modèles V4 doivent être statiques, compacts et bornés ;
- les raisons et résultats doivent privilégier des codes plutôt que des chaînes dynamiques durables.

## 5. Document mis à jour

```text
docs/architecture/AQUALOOK_V4_CURRENT_TO_TARGET_MAPPING.md
```

La cartographie contient désormais :

- les chemins de fichiers ;
- les structures exactes ;
- les flux de boot, planning et commande ;
- les couplages ;
- les contraintes de capacité ;
- les règles imposées au modèle V4 initial.

## 6. Fichiers source modifiés

Aucun.

## 7. Fichiers volontairement non modifiés

- `src/main.cpp`
- `src/ConfigManager.*`
- `src/ScheduleManager.*`
- `src/RelaisManager.*`
- `src/RelayTopology.*`
- `src/WebManager.*`
- `src/DisplayManager.*`
- `include/config.h`
- `platformio.ini`
- `data/`
- schéma et données NVS

## 8. Compilation et tests

### Compilation

Non exécutée : aucune source n’a été modifiée.

### Tests matériels

Aucun test matériel requis.

### Limite

Les tailles exactes des structures et la marge RAM/flash doivent être mesurées pendant le prochain run avant de fixer les limites V4.

## 9. Invariants confirmés

- aucune zone V4 ne doit connaître un relais ;
- aucun nouveau modèle ne doit dépendre de NVS ou du Web ;
- la Phase 1 ne modifie pas `PersistedConfig` ;
- aucune troisième copie complète des plannings n’est autorisée ;
- le callback historique reste intact jusqu’à l’intégration progressive ;
- l’état demandé ne doit plus être assimilé au succès matériel dans la cible ;
- la sécurité de durée maximale reste active dans `RelaisManager` ;
- le profil `Zone N -> carte 0 -> voie N` reste le fallback compatible.

## 10. Prochaine action unique

Démarrer **Phase 1 — Run 1.1 — Identités et capacités maximales**.

Ce run doit rester principalement architectural et produire :

- une ADR sur les identifiants stables ;
- une ADR sur les capacités maximales ;
- une mesure exacte de `sizeof()` des structures existantes ;
- un budget RAM/flash V4 ;
- une décision sur la conversion identifiant/index ;
- aucun changement du chemin runtime.

## 11. Message de reprise recommandé

```text
Projet AquaLook V4 — Phase 1 — Run 1.1 — Identités et capacités maximales

Base de travail :
- Dépôt : cnuma/AquaLook
- Branche : feature/relay-board-mapping
- HEAD distant : utiliser le commit exact du checkpoint Run 1.0
- Working tree local : non vérifié tant que le git pull utilisateur n’est pas possible

Documents de référence :
- docs/architecture/AQUALOOK_V4_TARGET_ARCHITECTURE.md
- docs/architecture/AQUALOOK_V4_CURRENT_TO_TARGET_MAPPING.md
- docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
- docs/architecture/AQUALOOK_V4_PHASE_PLAN.md
- docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.0-source-inspection.md

État validé à préserver :
- aucun code runtime V4 produit ;
- NVS inchangé ;
- deux représentations de zones existantes identifiées ;
- aucune troisième copie complète autorisée ;
- callback planning/relais et topologie conservés.

Objectif unique :
Décider les identifiants stables et les limites embarquées V4 sur la base d’un budget mémoire mesuré, sans modifier le chemin runtime.
```
