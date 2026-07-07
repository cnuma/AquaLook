# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 2 — Run 2.1, 7 juillet 2026

## État général

La Phase 1 est clôturée comme socle architectural isolé. La Phase 2 est engagée sur l’inventaire matériel générique.

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
| ARCH-001 à ARCH-009 | Domaine Phase 1 | **Décidé et prototypé** |
| ARCH-010 | Inventaire générique des bus | **Décidé et prototypé** |
| ARCH-011 | Cartes et ports génériques | Run 2.2 |
| ARCH-012 | Adresses de bus dupliquées | **Règle initiale décidée** |
| ARCH-013 à ARCH-016 | Actionneurs et compatibilité historique | Phase 3 |
| ARCH-017 à ARCH-019 | Reprise, durées et compensation | Phase 4 |
| ARCH-020 à ARCH-022 | Ressources partagées et simultanéité | Phase 5 |
| ARCH-023 à ARCH-025 | Observation et débit | Phase 6 |
| ARCH-026 à ARCH-029 | Persistance et migration | Phase 7 |
| ARCH-030 à ARCH-032 | API et sécurité | Phase 8 |
| ARCH-038 | CI et tests hôte | Ouvert |
| ARCH-040 | Versionnement des modèles de cartes | Run 2.2/Phase 7 |
| ARCH-041 | Suppression d’une carte liée | Run 2.2/Phase 7 |
| ARCH-058 | Mesure PlatformIO | Bloquant avant intégration |
| ARCH-059 | Mesure heap | Bloquant avant intégration runtime |
| ARCH-060 | Catalogue de types de contrôleurs | Run 2.2 ou 2.3 |
| ARCH-061 | Règles d’adresse spécialisées par contrôleur | Ouvert |
| ARCH-062 | Inventaire des ports et canaux | Run 2.2 |
| ARCH-063 | Binding Equipment vers port | Run 2.3 |
| ARCH-064 | Détection physique des contrôleurs | Différé |

## Décisions du Run 2.1

- `BusId`, `ControllerId` et `ControllerTypeId` sont des identifiants forts sur 16 bits ;
- `BusDefinition` occupe 16 octets ;
- `ControllerDefinition` occupe 24 octets ;
- neuf types de bus sont reconnus ;
- une adresse générique sur 64 bits couvre plusieurs protocoles ;
- les collisions d’endpoints sont refusées sur un même bus ;
- deux bus ne peuvent pas partager le même couple type/instance ;
- aucun objet Arduino ni driver concret n’est utilisé ;
- `RelayTopology` et le runtime historique restent inchangés.

## Prochaine étape

Démarrer **AquaLook V4 — Phase 2 — Run 2.2 — Cartes, ports et canaux génériques**.
