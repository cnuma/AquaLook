# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 2 — Run 2.2, 7 juillet 2026

## État général

La Phase 1 est clôturée comme socle architectural isolé. La Phase 2 couvre désormais les bus, contrôleurs, cartes, ports et canaux génériques.

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
| ARCH-013 à ARCH-016 | Actionneurs et compatibilité historique | Phase 3 |
| ARCH-038 | CI et tests hôte | Ouvert |
| ARCH-040 | Versionnement des modèles de cartes | Principe engagé, catalogue différé |
| ARCH-041 | Suppression d’une carte liée | Phase 7 |
| ARCH-058 | Mesure PlatformIO | Bloquant avant intégration |
| ARCH-059 | Mesure heap | Bloquant avant intégration runtime |
| ARCH-060 | Catalogue de types de contrôleurs | Run 2.4 |
| ARCH-061 | Règles d’adresse spécialisées par contrôleur | Ouvert |
| ARCH-062 | Inventaire des ports et canaux | **Décidé et prototypé** |
| ARCH-063 | Binding Equipment vers port | Run 2.3 |
| ARCH-064 | Détection physique des contrôleurs | Différé |
| ARCH-065 | Profil de protocoles compilés | Run dédié avant drivers concrets |
| ARCH-066 | Catalogue de modèles de cartes | Ouvert |
| ARCH-067 | Politique des canaux partagés | Ouvert, partage interdit par défaut |

## Décisions du Run 2.2

- `BoardTypeId` et `PortId` sont des identifiants forts sur 16 bits ;
- `BoardDefinition` occupe 16 octets ;
- `PortDefinition` occupe 16 octets ;
- les ports d’une carte forment une plage contiguë ;
- un port référence sa carte, son contrôleur et son canal ;
- les capacités du port doivent être supportées par le contrôleur ;
- les collisions de canaux sont refusées par défaut ;
- l’état sûr matériel reste distinct de l’état sûr métier ;
- les protocoles connus du modèle ne sont pas automatiquement compilés ;
- aucun runtime historique n’est modifié.

## Prochaine étape

Démarrer **AquaLook V4 — Phase 2 — Run 2.3 — Binding Equipment vers ports et compatibilité relais**.
