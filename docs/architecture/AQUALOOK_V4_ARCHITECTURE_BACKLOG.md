# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 3 — Run 3.4, 7 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose d’un contrat binaire, d’un driver simulé, d’un driver GPIO concret isolé et d’une première validation PlatformIO complète réussie.

## Validation d’intégration obtenue

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:02:41.876
```

```text
RAM:   20.6% — 67,384 / 327,680 octets
Flash: 62.6% — 1,271,749 / 2,031,616 octets
```

Capacité restante :

```text
RAM:   260,296 octets
Flash: 759,867 octets
```

Le warning SdFat `__has_include(FS.h)` reste présent, non bloquant et antérieur aux ajouts V4.

## Décisions principales

| ID | Sujet | Statut |
|---|---|---|
| ARCH-001 à ARCH-012 | Domaine et inventaire matériel | **Clôturé architecturalement** |
| ARCH-013 | Actionneur binaire | **Contrat réalisé** |
| ARCH-014 | État sûr par port/actionneur | **Validé et appliqué par GPIO** |
| ARCH-015 | Compatibilité `RelayAssignment` | Binding réalisé, intégration différée |
| ARCH-016 | Commandes idempotentes | **Validées** |
| ARCH-038 | CI et tests hôte | Workflow PlatformIO ajouté, tests hôte réalisés |
| ARCH-058 | Mesure PlatformIO | **Validée** |
| ARCH-059 | Mesure heap | Toujours requise avant intégration runtime |
| ARCH-070 | Drivers matériels conditionnels | **Premier driver concret réalisé et compilé** |
| ARCH-071 | Dépendances PlatformIO conditionnelles | Aucun ajout nécessaire pour GPIO |
| ARCH-072 | Mesure du gain flash par profil | Toujours ouverte |
| ARCH-075 | Registre borné de drivers | **Réalisé** |
| ARCH-076 | Driver binaire simulé | **Réalisé et validé** |
| ARCH-077 | Driver GPIO conditionnel | **Réalisé, testé hôte et compilé ESP32** |
| ARCH-078 | Driver XL9535 conditionnel | Prochaine étape possible |
| ARCH-079 | Synchronisation et concurrence | Ouvert |
| ARCH-080 | Politique de readback | Lecture explicite disponible |
| ARCH-082 | Adaptateur Arduino GPIO | **Isolé et compilé** |
| ARCH-083 | Collisions de macros Arduino | **Corrigées par préfixage des enums** |

## Décisions du Run 3.4

- un workflow GitHub Actions PlatformIO a été ajouté ;
- la compilation initiale a révélé des collisions entre macros Arduino et enums V4 ;
- les enums GPIO ont été renommés avec les préfixes `MODE_` et `LEVEL_` ;
- aucun comportement GPIO n’a changé ;
- la compilation complète de `ProgrammeArrosage` réussit ;
- tous les nouveaux fichiers V4 sont intégrés au build ;
- les drivers restent non instanciés par le runtime historique ;
- aucune commande matérielle V4 n’est activée.

## Prochaine étape

Démarrer **AquaLook V4 — Phase 3 — Run 3.5 — Driver XL9535 conditionnel et isolé**.
