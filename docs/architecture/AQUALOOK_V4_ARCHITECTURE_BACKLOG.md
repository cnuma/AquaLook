# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 2 — Run 2.3, 7 juillet 2026

## État général

La Phase 1 est clôturée comme socle architectural isolé. La Phase 2 couvre désormais les bus, contrôleurs, cartes, ports, canaux et bindings génériques.

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
| ARCH-011 | Cartes et ports génériques | **Décidé et prototypé** |
| ARCH-012 | Adresses de bus dupliquées | **Règle initiale décidée** |
| ARCH-013 à ARCH-016 | Actionneurs et compatibilité historique | Binding réalisé, drivers en Phase 3 |
| ARCH-038 | CI et tests hôte | Ouvert |
| ARCH-040 | Versionnement des modèles de cartes | Principe engagé, catalogue différé |
| ARCH-041 | Suppression d’une carte liée | Phase 7 |
| ARCH-058 | Mesure PlatformIO | Bloquant avant intégration |
| ARCH-059 | Mesure heap | Bloquant avant intégration runtime |
| ARCH-060 | Catalogue de types de contrôleurs | Run 2.4 |
| ARCH-061 | Règles d’adresse spécialisées par contrôleur | Run 2.4 |
| ARCH-062 | Inventaire des ports et canaux | **Décidé et prototypé** |
| ARCH-063 | Binding Equipment vers port | **Décidé et prototypé** |
| ARCH-064 | Détection physique des contrôleurs | Différé |
| ARCH-065 | Profil de protocoles compilés | Run 2.4 |
| ARCH-066 | Catalogue de modèles de cartes | Run 2.4 |
| ARCH-067 | Politique des canaux partagés | Partage interdit par défaut |
| ARCH-068 | Migration effective de RelayTopology | Phase 7 ou run d’intégration dédié |
| ARCH-069 | Persistance des bindings | Phase 7 |

## Décisions du Run 2.3

- `EquipmentPortBinding` relie un `EquipmentId` à un `PortId` ;
- la structure occupe 16 octets ;
- quatre types de bindings sont définis ;
- les capacités requises sont validées contre le port et l’équipement ;
- un seul actionneur primaire est autorisé par équipement ;
- un port ne peut être partagé sans flag explicite ;
- la passerelle historique utilise une vue neutre de `RelayAssignment` ;
- `(role, targetIndex)` est traduit vers `EquipmentId` ;
- `(boardIndex, channelIndex)` est traduit vers `PortId` ;
- `RelayTopology.h` n’est jamais inclus dans le domaine V4 ;
- `RelayTopology`, `RelaisManager` et le runtime historique restent inchangés.

## Prochaine étape

Démarrer **AquaLook V4 — Phase 2 — Run 2.4 — Catalogues matériels et profil de protocoles compilés**.
