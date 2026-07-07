# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 3 — Run 3.3, 7 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés. La Phase 3 dispose désormais d’un contrat binaire, d’un driver simulé et d’un premier driver concret GPIO isolé.

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
| ARCH-014 | État sûr par port/actionneur | **Validé et appliqué par GPIO** |
| ARCH-015 | Compatibilité `RelayAssignment` | Binding réalisé, intégration différée |
| ARCH-016 | Commandes idempotentes | **Validées** |
| ARCH-038 | CI et tests hôte | Ouvert |
| ARCH-058 | Mesure PlatformIO | Bloquant avant intégration |
| ARCH-059 | Mesure heap | Bloquant avant intégration runtime |
| ARCH-070 | Drivers matériels conditionnels | **Premier driver concret réalisé** |
| ARCH-071 | Dépendances PlatformIO conditionnelles | Aucun ajout nécessaire pour GPIO |
| ARCH-072 | Mesure du gain flash par profil | Avant validation de Phase 3 |
| ARCH-075 | Registre borné de drivers | **Réalisé** |
| ARCH-076 | Driver binaire simulé | **Réalisé et validé** |
| ARCH-077 | Driver GPIO conditionnel | **Réalisé et validé sur hôte** |
| ARCH-078 | Driver XL9535 conditionnel | Run ultérieur |
| ARCH-079 | Synchronisation et concurrence | Ouvert |
| ARCH-080 | Politique de readback | Lecture explicite disponible |
| ARCH-082 | Adaptateur Arduino GPIO | **Isolé sous src/drivers** |

## Décisions du Run 3.3

- le driver GPIO logique reste indépendant d’Arduino ;
- l’adaptateur ESP32 est le seul à inclure `Arduino.h` ;
- la compilation est conditionnée par `AQUALOOK_V4_ENABLE_GPIO` ;
- `PORT_FLAG_INVERTED` inverse uniquement le niveau électrique ;
- la configuration applique immédiatement l’état sûr ;
- les broches non valides en sortie sont refusées ;
- le contrôleur requis est `LOCAL_GPIO` ;
- aucun objet n’est instancié dans le firmware actuel ;
- `RelaisManager` reste inchangé.

## Validation hôte

```text
Compilation hôte OK
normalWrites=2 invertedWrites=2 normalReads=1 invertedReads=1
GPIO driver enabled
GPIO driver excluded
Profils conditionnels OK
```

## Prochaine étape

Avant le driver XL9535, exécuter idéalement une compilation PlatformIO complète du dépôt. À défaut, le prochain run sera **Phase 3 — Run 3.4 — Driver XL9535 conditionnel et isolé**.
