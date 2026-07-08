# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 3 — Run 3.6, 8 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose maintenant d’un contrat binaire, d’un driver simulé, d’un driver GPIO concret isolé, d’un driver XL9535 conditionnel isolé, d’un bootstrap non-runtime du registre de drivers et d’une validation PlatformIO complète réussie après ajout du XL9535.

## Dernière validation PlatformIO complète connue

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:02:54.294
```

```text
RAM:   20.6% — 67,384 / 327,680 octets
Flash: 62.6% — 1,271,997 / 2,031,616 octets
```

Cette mesure précède le bootstrap Run 3.6. Une nouvelle compilation PlatformIO est requise pour le clôturer définitivement.

## Décisions principales

| ID | Sujet | Statut |
|---|---|---|
| ARCH-001 à ARCH-012 | Domaine et inventaire matériel | **Clôturé architecturalement** |
| ARCH-013 | Actionneur binaire | **Contrat réalisé** |
| ARCH-014 | État sûr par port/actionneur | **Validé et appliqué par GPIO et XL9535** |
| ARCH-015 | Compatibilité `RelayAssignment` | Binding réalisé, intégration différée |
| ARCH-016 | Commandes idempotentes | **Validées** |
| ARCH-038 | CI et tests hôte | Workflow PlatformIO ajouté, tests hôte réalisés |
| ARCH-058 | Mesure PlatformIO | Validée Run 3.5, à refaire après bootstrap |
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
| ARCH-085 | Bootstrap non-runtime des drivers | **Réalisé et testé hôte** |

## Décisions du Run 3.6

- un plan `BinaryActuatorDriverBootstrapPlan` est ajouté ;
- le bootstrap enregistre les drivers demandés dans un registre fourni ;
- les contextes doivent être fournis explicitement par l’appelant ;
- aucun registre global n’est créé ;
- aucun driver n’est instancié automatiquement ;
- les drivers disponibles sont filtrés par le profil compilé ;
- les erreurs de registre sont propagées ;
- aucun raccord à `RelaisManager` ou au runtime actif n’est introduit.

## Validation hôte Run 3.6

```text
Compilation hôte OK
registered=3 requested=3 failures-ok
```

## Prochaine étape

Compiler localement :

```powershell
pio run -e ProgrammeArrosage
```

Après succès, clôturer Run 3.6 avec les mesures RAM/flash mises à jour.
