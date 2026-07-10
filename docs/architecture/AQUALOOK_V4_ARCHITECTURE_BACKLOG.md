# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 4 — Runs 4.11 à 4.13 figés après validation fonctionnelle, 10 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose d’un contrat binaire, d’un driver simulé, de drivers GPIO et XL9535 isolés, d’un registre de drivers et d’un bootstrap non-runtime validé.

La Phase 4 introduit progressivement la notion générique `EquipmentOutput` dans le runtime tout en gardant `RelaisManager` comme backend physique effectif et comme fallback.

Le Run 4.9 fige la migration des lectures d’état Web/LCD via `EquipmentOutputRuntimeAdapter`.

Le Run 4.10 documente la frontière backend recommandée.

Le Run 4.11 ajoute `RelayPhysicalBackend` et `RelaisManagerBackend`.

Le Run 4.12 injecte `RelayPhysicalBackend` dans `EquipmentOutputRuntimeAdapter`, avec fallback direct `RelaisManager`.

Le Run 4.13 câble `RelaisManagerBackend` dans `main.cpp`. La compilation et le test fonctionnel utilisateur sont validés.

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

## Validation fonctionnelle Runs 4.11 à 4.13

Données `/api/status` observées le 10 juillet 2026 à 16:35:39 :

```text
synced: true
uptime: 30 s
heap: 87,352 octets
weather.fetched: true
zones remontées: 4
zones actives: 3
manualDurationMin: 1
```

États observés :

```text
Jardin   active=false
Terrasse active=true
Zone 3   active=true
Zone 4   active=true
```

Le motif `Manuel 1min` est remonté pour les zones testées.

Validation utilisateur :

```text
page web et lcd semblent bon
```

Conclusion :

- `/api/status` opérationnel ;
- états actifs remontés correctement ;
- page Web fonctionnelle ;
- LCD fonctionnel ;
- non-régression principale validée par l’utilisateur.

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
| ARCH-098 | Interface passive `RelayPhysicalBackend` | **Ajoutée, compilée et validée** |
| ARCH-099 | Injection `RelayPhysicalBackend` dans `EquipmentOutputRuntimeAdapter` | **Ajoutée, compilée et validée** |
| ARCH-100 | Câblage `RelaisManagerBackend` dans `main.cpp` | **Ajouté, compilé et validé** |

## Décisions Runs 4.11 à 4.13

- ajout de `RelayPhysicalBackend` ;
- ajout de `RelaisManagerBackend` ;
- backend optionnel essayé avant le fallback direct ;
- `RelaisManagerBackend` est câblé dans `main.cpp` ;
- `outputAdapter.bind(&relaisMgr)` reste conservé ;
- aucun driver V4 réel n’est activé ;
- aucun changement NVS, Web, LCD ou JSON ;
- compilation et test de non-régression validés.

Documents de référence :

```text
docs/checkpoints/CHECKPOINT_2026-07-09_v4-phase4-run4-9-web-lcd-state-read-validated.md
docs/checkpoints/CHECKPOINT_2026-07-10_v4-phase4-run4-11-to-4-13-backend-bridge-validated.md
docs/architecture/AQUALOOK_V4_RUNTIME_BRIDGE_NEXT_STEP_PLAN.md
docs/architecture/AQUALOOK_V4_RELAY_PHYSICAL_BACKEND.md
docs/architecture/AQUALOOK_V4_OUTPUT_ADAPTER_PHYSICAL_BACKEND_INJECTION.md
docs/architecture/AQUALOOK_V4_RELAISMANAGER_BACKEND_WIRING.md
```

## Prochaine étape

```text
AquaLook V4 — Phase 5 — Tests et validation automatisée
Run 5.1 — Stratégie et matrice de tests
```

Objectif : couvrir systématiquement les branches backend, fallback, erreurs, zones invalides et produire un rapport automatique avant l’activation des drivers physiques V4.
