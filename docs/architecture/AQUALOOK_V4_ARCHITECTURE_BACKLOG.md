# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 3 — Run 3.1, 7 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 est engagée sur les actionneurs et drivers conditionnels.

Les validations différées restent obligatoires avant toute intégration runtime :

```text
compilation PlatformIO complète
mesure flash
mesure RAM statique
heap libre et minimum observé
```

## Décisions principales

| ID | Sujet | Statut |
|---|---|---|
| ARCH-001 à ARCH-012 | Domaine et inventaire matériel | **Clôturé architecturalement** |
| ARCH-013 | Actionneur binaire | **Contrat réalisé** |
| ARCH-014 | État sûr par port/actionneur | **Règle réalisée** |
| ARCH-015 | Compatibilité `RelayAssignment` | Binding réalisé, intégration différée |
| ARCH-016 | Commandes idempotentes | **Règle réalisée** |
| ARCH-038 | CI et tests hôte | Ouvert |
| ARCH-058 | Mesure PlatformIO | Bloquant avant intégration |
| ARCH-059 | Mesure heap | Bloquant avant intégration runtime |
| ARCH-068 | Migration effective de RelayTopology | Phase 7 ou intégration dédiée |
| ARCH-070 | Drivers matériels conditionnels | Contrat réalisé, drivers concrets à venir |
| ARCH-071 | Dépendances PlatformIO conditionnelles | Phase 3 |
| ARCH-072 | Mesure du gain flash par profil | Avant validation de Phase 3 |
| ARCH-075 | Registre borné de drivers | **Réalisé** |
| ARCH-076 | Driver binaire simulé | Run 3.2 |
| ARCH-077 | Driver GPIO conditionnel | Run ultérieur |
| ARCH-078 | Driver XL9535 conditionnel | Run ultérieur |
| ARCH-079 | Synchronisation et concurrence des drivers | Ouvert |
| ARCH-080 | Politique de readback | Ouvert, lecture explicite disponible |

## Décisions du Run 3.1

- `BinaryActuatorDriverOps` définit `configure`, `write`, `read`, `applySafeState` et `health` ;
- le contrat ne dépend d’aucun objet Arduino ;
- `BinaryActuatorSession` conserve le dernier état appliqué ;
- une commande répétée retourne `ALREADY_APPLIED` sans nouvelle écriture ;
- l’état sûr provient de `PortDefinition::safeState` ;
- les états sûrs binaires directement supportés sont `INACTIVE` et `ACTIVE` ;
- un registre borné associe un driver à un `ControllerTypeId` ;
- un driver incomplet, dupliqué ou hors capacité est refusé ;
- les erreurs sont traduisibles vers `OperationError` ;
- aucun driver concret ni raccord à `RelaisManager` n’est introduit.

## Tailles verrouillées

```text
BinaryActuatorDriverResult  8 octets
BinaryActuatorSession       6 octets
```

## Prochaine étape

Démarrer **AquaLook V4 — Phase 3 — Run 3.2 — Driver binaire simulé et validation complète du contrat**.
