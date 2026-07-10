# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 6 — Run 6.2 V4RelayPhysicalBackend inactif, 10 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose d’un contrat binaire, d’un driver simulé, de drivers GPIO et XL9535 isolés, d’un registre de drivers et d’un bootstrap non-runtime validé.

La Phase 4 introduit progressivement la notion générique `EquipmentOutput` dans le runtime tout en gardant `RelaisManager` comme backend physique effectif et comme fallback.

Les Runs 4.11 à 4.13 ont ajouté et validé la chaîne suivante :

```text
EquipmentOutputRuntimeAdapter
  -> RelayPhysicalBackend
  -> RelaisManagerBackend
  -> RelaisManager
```

Fallback direct conservé :

```text
EquipmentOutputRuntimeAdapter
  -> RelaisManager
```

La Phase 5 de tests et validation automatisée est mise en pause sur décision utilisateur. Elle reste prévue et pourra être reprise avant ou pendant la stabilisation de la Phase 6.

La Phase 6 est ouverte par le Run 6.1 documentaire. Elle doit introduire progressivement un backend physique V4 sans basculement brutal, avec fallback, rollback et journalisation obligatoires.

Le Run 6.2 ajoute `V4RelayPhysicalBackend` derrière `RelayPhysicalBackend`. Le backend est strictement inactif : aucune zone migrée, aucune commande appliquée, aucune lecture V4 valide et aucun câblage dans `main.cpp`.

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

Validation utilisateur :

```text
page web et lcd semblent bon
```

Conclusion :

- `/api/status` opérationnel ;
- états actifs remontés correctement ;
- page Web fonctionnelle ;
- LCD fonctionnel ;
- non-régression principale validée.

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
| ARCH-101 | Stratégie de bascule backend physique V4 | **Documentée** |
| ARCH-102 | Squelette inactif `V4RelayPhysicalBackend` | **Ajouté, compilation à valider** |

## Décisions du Run 6.1

- Phase 5 mise en pause, non supprimée ;
- aucun driver V4 réel activé ;
- migration progressive par zone ;
- backend V4 jamais utilisé implicitement sur une zone non migrée ;
- priorité au backend V4 uniquement si toutes les conditions de disponibilité sont réunies ;
- fallback historique obligatoire en cas d’échec ;
- erreur primaire et fallback journalisés séparément ;
- rollback compile-time, runtime et Git obligatoire ;
- état sûr par défaut : `OFF` ;
- aucune suppression de fallback pendant les premiers runs de Phase 6 ;
- retrait éventuel des fallbacks soumis à validation longue durée et décision explicite.

## Décisions du Run 6.2

- ajout de `src/V4RelayPhysicalBackend.h` ;
- ajout de `src/V4RelayPhysicalBackend.cpp` ;
- `V4RelayPhysicalBackend` implémente `RelayPhysicalBackend` ;
- masque de zones migrées initialisé à zéro ;
- `setZoneValve(...)` renvoie toujours `false` ;
- `getZoneValveState(...)` force `active=false` puis renvoie `false` ;
- aucune instance créée dans `main.cpp` ;
- aucun appel au registre de drivers Phase 3 ;
- aucun backend V4 actif ;
- aucun changement NVS, Web, LCD ou JSON ;
- fallback historique inchangé.

## Séquence Phase 6 proposée

```text
Run 6.1  Stratégie de bascule du backend physique
Run 6.2  Backend V4 derrière RelayPhysicalBackend
Run 6.3  Résolution RelayAssignment vers driver/port
Run 6.4  Profils compile-time legacy / V4
Run 6.5  Activation sur une zone pilote
Run 6.6  Comparaison ancien / nouveau backend
Run 6.7  Migration progressive des zones
Run 6.8  Tests de panne et fallback
Run 6.9  Stabilisation du backend V4
Run 6.10 Décision de retrait des fallbacks
```

Documents de référence :

```text
docs/checkpoints/CHECKPOINT_2026-07-10_v4-phase4-run4-11-to-4-13-backend-bridge-validated.md
docs/architecture/AQUALOOK_V4_PHASE6_BACKEND_SWITCHOVER_STRATEGY.md
docs/architecture/AQUALOOK_V4_PHASE6_V4_RELAY_PHYSICAL_BACKEND.md
docs/architecture/AQUALOOK_V4_RELAY_PHYSICAL_BACKEND.md
docs/architecture/AQUALOOK_V4_OUTPUT_ADAPTER_PHYSICAL_BACKEND_INJECTION.md
docs/architecture/AQUALOOK_V4_RELAISMANAGER_BACKEND_WIRING.md
```

## Prochaine étape

Valider **Run 6.2 — compilation**.

Commande :

```powershell
pio run -e ProgrammeArrosage
```

Après compilation OK :

```text
AquaLook V4 — Phase 6 — Run 6.3
Résolution RelayAssignment vers registre de drivers et port physique,
sans activation de zone
```

Invariants Run 6.3 :

- inspection obligatoire des contrats Phase 3 réels ;
- aucune activation de zone ;
- aucun changement NVS ;
- aucun changement Web/LCD/JSON ;
- aucun câblage V4 dans `main.cpp` ;
- fallback historique conservé ;
- compilation PlatformIO obligatoire.
