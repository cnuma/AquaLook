# AquaLook V4 — Checkpoint Phase 1 Run 1.2 — Modèle Equipment minimal

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/relay-board-mapping`  
**Base de départ :** `78c4e75c317e328f93528f68ce17569bfdc4fc3a`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Décider et implémenter un modèle `Equipment` minimal :

- compact ;
- générique ;
- construit dans une arène bornée ;
- indépendant du planning, du NVS, du Web, du LCD et du matériel.

## 2. Décisions d’architecture

### Type et capacités

- le type est un `EquipmentTypeId` stable ;
- le type est défini par un descripteur partagé ;
- les capacités utilisent un masque de 32 bits ;
- le type détermine le schéma de paramètres et les capacités obligatoires/autorisées ;
- les capacités expriment le comportement, pas le câblage.

### Paramètres spécifiques

- l’objet fixe ne contient pas l’union de tous les paramètres ;
- les paramètres sont un bloc opaque versionné dans l’arène ;
- la référence contient offset, taille et version de schéma ;
- la taille est validée par le descripteur du type ;
- aucun pointeur brut n’est persisté.

### Noms

- le nom est stocké dans l’arène ;
- `TextRef` utilise offset et longueur ;
- le nom n’est pas l’identité.

## 3. Fichiers source créés

```text
src/domain/DomainIdentifiers.h
src/domain/BoundedArena.h
src/domain/EquipmentModel.h
src/domain/EquipmentModel.cpp
```

## 4. Modèle implémenté

`Equipment` contient :

```text
EquipmentId
EquipmentTypeId
CapabilityMask
ParameterBlockRef
TextRef
EquipmentMode
SafeState
flags
```

Taille mesurée sur test hôte :

```text
sizeof(Equipment) = 28 octets
```

Une assertion interdit une taille supérieure à 32 octets.

## 5. Capacités initiales

```text
BINARY_COMMAND
PROPORTIONAL_COMMAND
BIDIRECTIONAL
TIMED_OPERATION
PULSE_COMMAND
POSITION_FEEDBACK
STATE_FEEDBACK
FAULT_FEEDBACK
SAFE_STATE
SHARED_RESOURCE
```

## 6. Arène bornée

`BoundedArena` fournit une allocation :

- séquentielle ;
- alignée ;
- bornée ;
- sans libération individuelle ;
- référencée par offsets.

Elle reçoit un buffer externe et ne choisit aucun budget global.

## 7. Validation disponible

`validateEquipment()` contrôle :

- identifiants ;
- correspondance du type ;
- capacités connues, requises et supportées ;
- mode ;
- état sûr ;
- référence, taille et schéma des paramètres ;
- référence et terminaison du nom.

La validation retourne un code structuré et n’a aucun effet de bord.

## 8. Tests réalisés

Test hôte exécuté avec :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Scénario :

- arène de 256 octets ;
- équipement vanne ;
- bloc `ValveParameters` de 4 octets ;
- nom `Vanne nord` ;
- descripteur de type ;
- validation et résolution des références.

Résultat :

```text
Equipment size=28
arena used=16
validation OK
```

## 9. Compilation PlatformIO

Non exécutée : l’environnement local PlatformIO de l’utilisateur n’est pas disponible dans cette session.

Le code est C++11 sans dépendance Arduino et a été compilé sur hôte avec avertissements transformés en erreurs.

La compilation firmware `pio run -e ProgrammeArrosage` reste obligatoire dès que le dépôt local pourra être synchronisé.

## 10. Fichiers documentaires créés ou modifiés

```text
docs/architecture/adr/ADR-0004-equipment-types-and-capabilities.md
docs/architecture/adr/ADR-0005-equipment-parameter-blocks.md
docs/architecture/AQUALOOK_V4_EQUIPMENT_MODEL.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.2-equipment-model.md
```

## 11. Fichiers volontairement non modifiés

- `src/main.cpp` ;
- `src/ConfigManager.*` ;
- `src/ScheduleManager.*` ;
- `src/RelaisManager.*` ;
- `src/RelayTopology.*` ;
- `src/WebManager.*` ;
- `src/DisplayManager.*` ;
- `include/config.h` ;
- `platformio.ini` ;
- `data/` et `littlefs/` ;
- format NVS.

## 12. Comportement runtime

Aucun changement.

Les nouveaux fichiers sont compilables mais ne sont inclus, instanciés ou appelés par aucun chemin runtime.

## 13. Risques et limites

- le catalogue de types n’existe pas encore ;
- les validateurs de paramètres spécifiques ne sont pas encore branchés ;
- le budget exact des arènes n’est pas fixé ;
- la compilation ESP32 reste à confirmer ;
- `SafeState::HOLD_LAST` devra être interdit pour certains types ;
- les états runtime ne sont pas encore modélisés.

## 14. Invariants préservés

1. NVS inchangé.
2. Planning et algorithmes inchangés.
3. Relais et topologie runtime inchangés.
4. Aucun lien Equipment vers carte ou port.
5. Aucun `String` durable.
6. Aucun plafond fonctionnel fixe ajouté.
7. Aucune troisième copie des plannings.
8. Aucun effet matériel.

## 15. Prochaine action unique

Démarrer **Phase 1 — Run 1.3 — États, défauts et résultats**.

Le run devra maintenir la séparation :

```text
Equipment immuable de configuration
≠
EquipmentRuntimeState mutable
```

Il devra introduire :

- état demandé ;
- état autorisé ;
- état appliqué ;
- état observé ;
- santé ;
- défaut structuré ;
- résultat d’opération ;
- horodatages compacts ;
- aucune intégration runtime historique.

## 16. Message de reprise recommandé

```text
Projet AquaLook V4 — Phase 1 — Run 1.3 — États, défauts et résultats

Base :
- dépôt : cnuma/AquaLook
- branche : feature/relay-board-mapping
- HEAD distant : utiliser le commit exact du checkpoint Run 1.2

Références :
- docs/architecture/AQUALOOK_V4_EQUIPMENT_MODEL.md
- docs/architecture/AQUALOOK_V4_CONFIGURATION_LIFECYCLE.md
- docs/architecture/adr/ADR-0001-stable-identifiers.md
- docs/architecture/adr/ADR-0002-domain-capacity-limits.md
- docs/architecture/adr/ADR-0004-equipment-types-and-capabilities.md
- docs/architecture/adr/ADR-0005-equipment-parameter-blocks.md
- docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.2-equipment-model.md

État à préserver :
- Equipment = 28 octets sur test hôte ;
- paramètres et noms stockés par offsets dans l’arène ;
- aucun lien au planning, NVS, Web ou matériel ;
- runtime historique inchangé.

Objectif unique :
Créer les états, défauts et résultats runtime séparés de la configuration Equipment.
```
