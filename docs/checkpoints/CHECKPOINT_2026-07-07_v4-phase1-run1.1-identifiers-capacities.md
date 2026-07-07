# AquaLook V4 — Checkpoint Phase 1 Run 1.1 — Identités et capacités maximales

**Date :** 7 juillet 2026  
**Dépôt :** `cnuma/AquaLook`  
**Branche :** `feature/relay-board-mapping`  
**Base de départ :** `1ba0256fafab0242b254241ac9957f0a21e5f2c9`  
**HEAD de clôture :** commit contenant ce checkpoint

## 1. Objet

Décider les identifiants stables, les limites embarquées initiales et le modèle supérieur de l’inventaire matériel, sans modifier le runtime.

## 2. Décisions prises

### Identifiants

- identifiants stables sur `uint16_t` ;
- `0x0000` invalide, `0xFFFF` réservé ;
- types forts distincts : ZoneId, EquipmentId, SensorId, BoardId, AutomationId, ExecutionId ;
- index runtime sur `uint8_t` ;
- identifiant et index ne sont jamais supposés égaux ;
- un port est identifié par `BoardId + portIndex`.

### Inventaire matériel

Le modèle supérieur devient :

```text
HardwareInventory
├── HardwareBoard
│   └── HardwarePort
└── PortBinding
```

Les cartes relais deviennent un cas spécialisé et transitoire.

Les ports décrivent :

- direction INPUT, OUTPUT ou BIDIRECTIONAL ;
- nature DIGITAL ou ANALOG ;
- capacités complémentaires : compteur, interruption, PWM, impulsion, fréquence, etc.

### Capacités initiales

```text
MAX_ZONES_V4            = 16
MAX_EQUIPMENTS_V4       = 32
MAX_SENSORS_V4          = 32
MAX_AUTOMATIONS_V4      = 32
MAX_DEPENDENCIES_V4     = 64
MAX_ACTIVE_EXECUTIONS   = 16
MAX_HARDWARE_BOARDS     = 8
MAX_PORTS_PER_BOARD     = 16
MAX_PORT_BINDINGS       = 64
```

Ces plafonds sont des capacités de représentation, pas des garanties de simultanéité.

## 3. Budget mémoire

Tailles structurelles estimées avec alignement 32 bits :

```text
CfgSlot       ≈ 6 octets
CfgDaySchedule≈ 30 octets
CfgRain       ≈ 8 octets
CfgZone       ≈ 276 octets
ZoneSchedule  ≈ 256 octets
ActiveSlot    ≈ 16 octets
```

Coût minimal des tableaux existants :

```text
16 CfgZone       = 4 416 octets
16 ZoneSchedule  = 4 096 octets
16 ActiveSlot    =   256 octets
Total minimal    = 8 768 octets
```

Ce total exclut les `String`, autres managers et buffers.

Budget fixe retenu pour le nouveau domaine V4 :

```text
<= 12 Kio de RAM fixe
```

## 4. Règles mémoire

- aucune troisième copie complète des plannings ;
- aucun `String` durable dans les registres V4 ;
- descripteurs de modèles de cartes partagés et placés en flash lorsque possible ;
- identifiants et relations compacts ;
- aucune allocation dynamique régulière ;
- mesure `sizeof()` obligatoire lors de l’introduction du code.

## 5. Documents produits

- `docs/architecture/adr/ADR-0001-stable-identifiers.md`
- `docs/architecture/adr/ADR-0002-domain-capacity-limits.md`
- `docs/architecture/adr/ADR-0003-generic-hardware-inventory.md`
- `docs/architecture/AQUALOOK_V4_MEMORY_AND_CAPACITY_BUDGET.md`
- mise à jour de `docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md`
- présent checkpoint

## 6. Fichiers source modifiés

Aucun.

## 7. Fichiers volontairement non modifiés

- tout `src/` ;
- tout `include/` ;
- `platformio.ini` ;
- `data/` ;
- NVS ;
- Web ;
- LCD ;
- moteur d’arrosage ;
- `RelayTopology` runtime.

## 8. Compilation et tests

Aucune compilation requise : run documentaire.

Les tailles sont des estimations structurelles à confirmer par compilation PlatformIO sur ESP32 avant acceptation définitive du premier code V4.

## 9. Points ouverts suivants

- ARCH-003 — paramètres spécifiques des équipements ;
- ARCH-004 — distinction type d’équipement / capacités ;
- ARCH-005 — états demandé, autorisé, appliqué et observé.

## 10. Prochaine action unique

Démarrer **Phase 1 — Run 1.2 — Modèle Equipment minimal**.

Avant le code, ce run doit décider ARCH-003 et ARCH-004, puis introduire uniquement les types isolés nécessaires :

- identité ;
- type ;
- capacités ;
- activation ;
- mode ;
- état sûr ;
- paramètres spécifiques compacts.

Aucune liaison au planning, au NVS, au Web ou au matériel ne doit être ajoutée.

## 11. Message de reprise recommandé

```text
Projet AquaLook V4 — Phase 1 — Run 1.2 — Modèle Equipment minimal

Base de travail :
- Dépôt : cnuma/AquaLook
- Branche : feature/relay-board-mapping
- HEAD distant : utiliser le commit exact du checkpoint Run 1.1
- Working tree local : à synchroniser ultérieurement

Documents de référence :
- docs/architecture/AQUALOOK_V4_TARGET_ARCHITECTURE.md
- docs/architecture/AQUALOOK_V4_CURRENT_TO_TARGET_MAPPING.md
- docs/architecture/AQUALOOK_V4_MEMORY_AND_CAPACITY_BUDGET.md
- docs/architecture/adr/ADR-0001-stable-identifiers.md
- docs/architecture/adr/ADR-0002-domain-capacity-limits.md
- docs/architecture/adr/ADR-0003-generic-hardware-inventory.md
- docs/checkpoints/CHECKPOINT_2026-07-07_v4-phase1-run1.1-identifiers-capacities.md

État validé à préserver :
- identifiants 16 bits distincts des index ;
- inventaire générique carte/port/binding ;
- budget fixe V4 <= 12 Kio ;
- aucune troisième copie des plannings ;
- NVS et runtime inchangés.

Objectif unique :
Décider la représentation des types, capacités et paramètres spécifiques, puis créer le modèle Equipment minimal isolé.
```
