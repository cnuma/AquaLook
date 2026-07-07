# AquaLook V4 — Checkpoint Phase 1 Run 1.1 corrigé — Identités et capacité dynamique bornée

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/relay-board-mapping`  
**Base de départ :** `1ba0256fafab0242b254241ac9957f0a21e5f2c9`  
**Correction :** abandon des plafonds fonctionnels fixes au profit de l’option C  
**HEAD de clôture :** commit contenant ce checkpoint corrigé

## 1. Objet

Décider :

- les identifiants stables ;
- le modèle supérieur de l’inventaire matériel ;
- la manière de dimensionner une configuration générique sur ESP32.

La première version du checkpoint retenait des constantes fonctionnelles fixes. Cette décision est corrigée.

## 2. Identifiants validés

- identifiants stables sur `uint16_t` ;
- `0x0000` invalide, `0xFFFF` réservé ;
- types distincts : ZoneId, EquipmentId, SensorId, BoardId, AutomationId, ExecutionId ;
- index runtime compacts ;
- identifiant et index jamais supposés égaux ;
- un port référencé par `BoardId + portIndex`.

## 3. Inventaire matériel validé

```text
HardwareInventory
├── HardwareBoard
│   └── HardwarePort
└── PortBinding
```

Les modèles de cartes décrivent leur nombre réel de ports et leurs capacités.

Les cartes relais deviennent un cas spécialisé et transitoire.

## 4. Correction de capacité

Les plafonds suivants sont abandonnés comme décisions d’architecture :

```text
MAX_ZONES_V4
MAX_EQUIPMENTS_V4
MAX_SENSORS_V4
MAX_AUTOMATIONS_V4
MAX_DEPENDENCIES_V4
MAX_ACTIVE_EXECUTIONS
MAX_HARDWARE_BOARDS
MAX_PORTS_PER_BOARD
MAX_PORT_BINDINGS
```

Ils ne doivent pas servir à préallouer les catégories métier.

La configuration réelle est dérivée des éléments déclarés :

```text
modèles de cartes
cartes installées
ports exposés
équipements
capteurs
automatismes
dépendances
bindings
```

## 5. Option C retenue

La configuration est construite dynamiquement dans une mémoire bornée :

```text
source de configuration
-> ConfigurationBuilder
-> CandidateConfigurationArena
-> validation complète
-> activation atomique
-> ActiveConfiguration
```

L’allocation dans l’arène est séquentielle, alignée et sans libération individuelle.

Une candidate refusée est abandonnée globalement et ne modifie pas l’active.

## 6. Budgets techniques

Les constantes futures décrivent des budgets en octets et des gardes absolues :

```text
CONFIGURATION_ARENA_BYTES
CANDIDATE_CONFIGURATION_ARENA_BYTES
RUNTIME_EXECUTION_ARENA_BYTES
MAX_CONFIGURATION_INPUT_BYTES
ABSOLUTE_MAX_OBJECTS
ABSOLUTE_MAX_RELATIONS
MAX_CONFIGURATION_DEPTH
```

Leur valeur exacte n’est pas encore fixée.

Elles seront définies après mesures réelles de RAM, heap, PSRAM éventuelle et temps de validation.

## 7. Budget mémoire actuel observé

Estimations structurelles :

```text
CfgZone       ≈ 276 octets
ZoneSchedule  ≈ 256 octets
ActiveSlot    ≈ 16 octets
```

Coût minimal des tableaux historiques :

```text
16 CfgZone       = 4 416 octets
16 ZoneSchedule  = 4 096 octets
16 ActiveSlot    =   256 octets
Total minimal    = 8 768 octets
```

Le domaine V4 ne doit pas créer une troisième copie complète des plannings.

## 8. Cycle de configuration défini

Le document suivant devient la référence dédiée :

```text
docs/architecture/AQUALOOK_V4_CONFIGURATION_LIFECYCLE.md
```

Il définit :

- catalogue de modèles de cartes ;
- source de configuration ;
- builder ;
- candidate ;
- validations structurelle, matérielle, métier et mémoire ;
- activation atomique ;
- ajout, suppression, remplacement et upgrade ;
- diagnostics ;
- compatibilité avec le runtime actuel.

## 9. Documents produits ou corrigés

- `docs/architecture/adr/ADR-0001-stable-identifiers.md`
- `docs/architecture/adr/ADR-0002-domain-capacity-limits.md` — corrigée option C
- `docs/architecture/adr/ADR-0003-generic-hardware-inventory.md`
- `docs/architecture/AQUALOOK_V4_MEMORY_AND_CAPACITY_BUDGET.md` — corrigé
- `docs/architecture/AQUALOOK_V4_CONFIGURATION_LIFECYCLE.md` — nouveau
- `docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md` — corrigé
- présent checkpoint corrigé

## 10. Fichiers source modifiés

Aucun.

## 11. Fichiers volontairement non modifiés

- tout `src/` ;
- tout `include/` ;
- `platformio.ini` ;
- `data/` ;
- format NVS actuel ;
- Web ;
- LCD ;
- moteur d’arrosage ;
- `RelayTopology` runtime.

## 12. Compilation et tests

Aucune compilation requise : corrections documentaires uniquement.

Les valeurs `sizeof()` restent à confirmer sur la cible avant le premier code V4.

## 13. Invariants

1. La configuration active est immuable.
2. Toute modification construit une candidate complète.
3. Une candidate invalide ne remplace jamais l’active.
4. La mémoire nécessaire est évaluée avant activation.
5. Le nombre de ports vient du modèle de chaque carte.
6. Aucun objet n’est préalloué uniquement à cause d’un plafond fonctionnel.
7. Les gardes absolues ne décrivent pas une capacité commerciale.
8. Le runtime d’exécution est séparé de la configuration.
9. Le NVS actuel reste inchangé pendant la Phase 1.
10. Aucune troisième copie complète des plannings n’est autorisée.

## 14. Points ouverts immédiats

- ARCH-003 — paramètres spécifiques des équipements ;
- ARCH-004 — distinction type / capacités ;
- ARCH-039 — API interne minimale de l’arène bornée ;
- mesure réelle des budgets ;
- stratégie future de coexistence active/candidate ;
- politique de bascule pendant une exécution active.

## 15. Prochaine action unique

Démarrer **Phase 1 — Run 1.2 — Modèle Equipment minimal**.

Le run devra :

1. décider ARCH-003 et ARCH-004 ;
2. définir l’interface minimale d’allocation dans une arène sans implémenter la persistance ;
3. introduire les types Equipment isolés ;
4. vérifier leur taille ;
5. ne connecter ni planning, ni Web, ni NVS, ni matériel.

## 16. Message de reprise recommandé

```text
Projet AquaLook V4 — Phase 1 — Run 1.2 — Modèle Equipment minimal

Base de travail :
- Dépôt : cnuma/AquaLook
- Branche : feature/relay-board-mapping
- HEAD distant : utiliser le commit exact du checkpoint Run 1.1 corrigé
- Working tree local : à synchroniser ultérieurement

Documents de référence :
- docs/architecture/AQUALOOK_V4_TARGET_ARCHITECTURE.md
- docs/architecture/AQUALOOK_V4_CURRENT_TO_TARGET_MAPPING.md
- docs/architecture/AQUALOOK_V4_CONFIGURATION_LIFECYCLE.md
- docs/architecture/AQUALOOK_V4_MEMORY_AND_CAPACITY_BUDGET.md
- docs/architecture/adr/ADR-0001-stable-identifiers.md
- docs/architecture/adr/ADR-0002-domain-capacity-limits.md
- docs/architecture/adr/ADR-0003-generic-hardware-inventory.md
- docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.1-identifiers-capacities.md

État validé à préserver :
- identifiants 16 bits distincts des index ;
- inventaire générique carte/port/binding ;
- option C : construction dynamique dans une arène bornée ;
- aucun plafond fonctionnel fixe retenu ;
- configuration active immuable ;
- NVS et runtime inchangés.

Objectif unique :
Décider la représentation des types, capacités et paramètres spécifiques, puis créer le modèle Equipment minimal isolé et compatible avec une arène bornée.
```
