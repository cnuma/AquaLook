# AquaLook V4 — Checkpoint Phase 1 Run 1.4 — Modèle Intent

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/relay-board-mapping`  
**Base de départ :** `17d6dcc30056472211739eacbed93bfede990f8d`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Introduire un modèle `EquipmentIntent` compact et isolé pour exprimer une demande avant arbitrage et exécution.

## 2. Fichiers source créés ou modifiés

```text
src/domain/DomainIdentifiers.h
src/domain/IntentModel.h
src/domain/IntentModel.cpp
```

`DomainIdentifiers.h` reçoit :

```text
IntentId
CorrelationId
```

## 3. Structure Intent

`EquipmentIntent` contient :

```text
état demandé
création monotone
fin de validité
origine et source
IntentId
EquipmentId cible
CorrelationId
motif de refus
priorité
statut
flags
révision
```

Taille mesurée et verrouillée :

```text
sizeof(EquipmentIntent) = 32 octets
sizeof(IntentSourceRef) = 4 octets
```

Une première disposition des champs produisait 36 octets. Elle a été corrigée par réordonnancement sans modifier le contenu fonctionnel.

## 4. Origines

```text
AUTOMATION
MANUAL
SAFETY
RECOVERY
API
SYSTEM
```

## 5. Priorités

```text
BACKGROUND = 0
NORMAL     = 64
HIGH       = 128
SAFETY     = 192
EMERGENCY  = 255
```

## 6. Arbitrage primitif

`outranks()` départage par :

1. priorité la plus élevée ;
2. intention la plus récente ;
3. `IntentId` le plus élevé.

Cette fonction ne remplace pas le futur arbitre complet.

## 7. Validité temporelle

- `validUntilMs == 0` : pas d’expiration automatique ;
- sinon expiration monotone ;
- comparaison compatible avec le rebouclage de `millis()` ;
- fenêtre maximale sûre inférieure à environ 24,8 jours.

## 8. Statuts

```text
PENDING
ACCEPTED
REJECTED
SUPERSEDED
EXPIRED
CANCELLED
```

Fonctions disponibles :

```text
acceptIntent()
rejectIntent()
supersedeIntent()
expireIntent()
cancelIntent()
```

## 9. Validation

`validateIntent()` contrôle :

- identité ;
- cible ;
- origine ;
- valeur demandée ;
- statut ;
- fenêtre de validité.

Les contrôles liés au type d’équipement, aux capacités, dépendances, défauts et conflits restent au futur arbitre.

## 10. Tests réalisés

Compilation hôte :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Cas couverts :

- validation d’une intention normale ;
- expiration à l’échéance ;
- priorité haute ;
- départage par récence ;
- acceptation ;
- rejet avec défaut bloquant ;
- fenêtre traversant le rebouclage de `millis()` ;
- taille exacte.

Résultat :

```text
Compilation hôte OK
32 4
```

## 11. Documentation créée ou modifiée

```text
docs/architecture/adr/ADR-0008-intent-model-and-priority.md
docs/architecture/AQUALOOK_V4_INTENT_MODEL.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.4-intent-model.md
```

## 12. Fichiers volontairement non modifiés

- `src/main.cpp` ;
- `src/ConfigManager.*` ;
- `src/ScheduleManager.*` ;
- `src/RelaisManager.*` ;
- `src/RelayTopology.*` ;
- `src/WebManager.*` ;
- `src/DisplayManager.*` ;
- `src/domain/EquipmentModel.*` ;
- `src/domain/EquipmentRuntimeState.*` ;
- `platformio.ini` ;
- format NVS ;
- ressources Web et LCD.

## 13. Comportement runtime

Aucun changement.

Le modèle Intent n’est relié à aucun chemin exécuté du firmware actuel.

## 14. Risques et limites

- aucun arbitre complet ;
- aucune file bornée ;
- pas de contrôle de transition strict ;
- `sourceId` reste interprété selon l’origine ;
- corrélation non orchestrée ;
- compilation PlatformIO complète à confirmer.

## 15. Invariants préservés

1. Une intention ne commande pas le matériel.
2. Elle cible un `EquipmentId`.
3. Elle reste distincte d’une exécution.
4. L’expiration utilise un temps monotone.
5. Aucun `String`, pointeur ou allocation dynamique.
6. NVS, planning, relais, Web et LCD inchangés.
7. Aucun effet matériel.

## 16. Prochaine action unique

Démarrer **Phase 1 — Run 1.5 — Modèle Execution**.

Le run devra définir :

- identité d’exécution ;
- intention source ;
- cible ;
- machine d’états ;
- étape courante ;
- délais ;
- annulation ;
- compensation ;
- résultat final ;
- aucune intégration au runtime historique.
