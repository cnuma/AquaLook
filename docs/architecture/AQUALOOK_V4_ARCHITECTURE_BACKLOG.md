# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 3 — Run 3.5, 8 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose maintenant d’un contrat binaire, d’un driver simulé, d’un driver GPIO concret isolé, d’un driver XL9535 conditionnel isolé et d’une première validation PlatformIO complète réussie au Run 3.4.

## Dernière validation PlatformIO complète connue

```text
Environment        Status    Duration
ProgrammeArrosage  SUCCESS   00:02:41.876
```

```text
RAM:   20.6% — 67,384 / 327,680 octets
Flash: 62.6% — 1,271,749 / 2,031,616 octets
```

Cette mesure précède l’ajout du driver XL9535. Une nouvelle compilation PlatformIO est requise pour clôturer définitivement Run 3.5.

## Décisions principales

| ID | Sujet | Statut |
|---|---|---|
| ARCH-001 à ARCH-012 | Domaine et inventaire matériel | **Clôturé architecturalement** |
| ARCH-013 | Actionneur binaire | **Contrat réalisé** |
| ARCH-014 | État sûr par port/actionneur | **Validé et appliqué par GPIO et XL9535** |
| ARCH-015 | Compatibilité `RelayAssignment` | Binding réalisé, intégration différée |
| ARCH-016 | Commandes idempotentes | **Validées** |
| ARCH-038 | CI et tests hôte | Workflow PlatformIO ajouté, tests hôte réalisés |
| ARCH-058 | Mesure PlatformIO | Validée au Run 3.4, à refaire après XL9535 |
| ARCH-059 | Mesure heap | Toujours requise avant intégration runtime |
| ARCH-070 | Drivers matériels conditionnels | GPIO et XL9535 réalisés isolément |
| ARCH-071 | Dépendances PlatformIO conditionnelles | Aucun ajout nécessaire pour GPIO/XL9535 |
| ARCH-072 | Mesure du gain flash par profil | Toujours ouverte |
| ARCH-075 | Registre borné de drivers | **Réalisé** |
| ARCH-076 | Driver binaire simulé | **Réalisé et validé** |
| ARCH-077 | Driver GPIO conditionnel | **Réalisé, testé hôte et compilé ESP32 au Run 3.4** |
| ARCH-078 | Driver XL9535 conditionnel | **Réalisé et testé hôte** |
| ARCH-079 | Synchronisation et concurrence | Ouvert |
| ARCH-080 | Politique de readback | Lecture explicite disponible |
| ARCH-082 | Adaptateur Arduino GPIO | **Isolé et compilé** |
| ARCH-083 | Collisions de macros Arduino | **Corrigées** |
| ARCH-084 | Adaptateur Arduino I²C/Wire | **Isolé sous src/drivers** |

## Décisions du Run 3.5

- un driver `Xl9535BinaryActuatorDriver` est ajouté ;
- il est conditionné par `AQUALOOK_V4_ENABLE_I2C` ;
- le domaine manipule `Xl9535I2cOps`, pas `Wire` ;
- `ArduinoI2cPlatform` est le seul adaptateur Wire ajouté ;
- le driver accepte les canaux 0 à 15 ;
- l’état sûr est écrit dans le latch avant de configurer le canal en sortie ;
- `PORT_FLAG_INVERTED` reste géré logiquement par le driver ;
- aucun raccord à `RelaisManager` ou au runtime actif n’est introduit.

## Validation hôte Run 3.5

```text
Compilation hôte OK
normalWrites=2 invertedWrites=2 reads=2 probes=3
```

## Prochaine étape

Compiler localement :

```powershell
pio run -e ProgrammeArrosage
```

Après succès, clôturer Run 3.5 avec les mesures RAM/flash mises à jour.
