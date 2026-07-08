# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 3 — Run 3.5 clôturé, 8 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose maintenant d’un contrat binaire, d’un driver simulé, d’un driver GPIO concret isolé, d’un driver XL9535 conditionnel isolé et d’une validation PlatformIO complète réussie après ajout du XL9535.

## Validation PlatformIO complète Run 3.5

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:02:54.294
```

```text
RAM:   20.6% — 67,384 / 327,680 octets
Flash: 62.6% — 1,271,997 / 2,031,616 octets
```

Capacité restante :

```text
RAM:   260,296 octets
Flash: 759,619 octets
```

Delta depuis Run 3.4 :

```text
RAM:   +0 octet
Flash: +248 octets
```

Le warning SdFat `__has_include(FS.h)` reste présent, non bloquant et sans lien avec les drivers V4.

## Décisions principales

| ID | Sujet | Statut |
|---|---|---|
| ARCH-001 à ARCH-012 | Domaine et inventaire matériel | **Clôturé architecturalement** |
| ARCH-013 | Actionneur binaire | **Contrat réalisé** |
| ARCH-014 | État sûr par port/actionneur | **Validé et appliqué par GPIO et XL9535** |
| ARCH-015 | Compatibilité `RelayAssignment` | Binding réalisé, intégration différée |
| ARCH-016 | Commandes idempotentes | **Validées** |
| ARCH-038 | CI et tests hôte | Workflow PlatformIO ajouté, tests hôte réalisés |
| ARCH-058 | Mesure PlatformIO | **Validée après XL9535** |
| ARCH-059 | Mesure heap | Toujours requise avant intégration runtime |
| ARCH-070 | Drivers matériels conditionnels | GPIO et XL9535 réalisés isolément et compilés |
| ARCH-071 | Dépendances PlatformIO conditionnelles | Aucun ajout nécessaire pour GPIO/XL9535 |
| ARCH-072 | Mesure du gain flash par profil | Toujours ouverte |
| ARCH-075 | Registre borné de drivers | **Réalisé** |
| ARCH-076 | Driver binaire simulé | **Réalisé et validé** |
| ARCH-077 | Driver GPIO conditionnel | **Réalisé, testé hôte et compilé ESP32** |
| ARCH-078 | Driver XL9535 conditionnel | **Réalisé, testé hôte et compilé ESP32** |
| ARCH-079 | Synchronisation et concurrence | Ouvert |
| ARCH-080 | Politique de readback | Lecture explicite disponible |
| ARCH-082 | Adaptateur Arduino GPIO | **Isolé et compilé** |
| ARCH-083 | Collisions de macros Arduino | **Corrigées** |
| ARCH-084 | Adaptateur Arduino I²C/Wire | **Isolé et compilé** |

## Décisions du Run 3.5

- un driver `Xl9535BinaryActuatorDriver` est ajouté ;
- il est conditionné par `AQUALOOK_V4_ENABLE_I2C` ;
- le domaine manipule `Xl9535I2cOps`, pas `Wire` ;
- `ArduinoI2cPlatform` est le seul adaptateur Wire ajouté ;
- le driver accepte les canaux 0 à 15 ;
- l’état sûr est écrit dans le latch avant de configurer le canal en sortie ;
- `PORT_FLAG_INVERTED` reste géré logiquement par le driver ;
- la compilation PlatformIO complète réussit ;
- aucun raccord à `RelaisManager` ou au runtime actif n’est introduit.

## Validation hôte Run 3.5

```text
Compilation hôte OK
normalWrites=2 invertedWrites=2 reads=2 probes=3
```

## Prochaine étape

Démarrer **AquaLook V4 — Phase 3 — Run 3.6 — Consolidation registre de drivers et stratégie d’instanciation non-runtime**.
