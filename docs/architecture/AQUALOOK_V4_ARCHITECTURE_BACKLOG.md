# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 2 — Run 2.4, 7 juillet 2026

## État général

La Phase 1 est clôturée comme socle architectural isolé. La Phase 2 couvre les bus, contrôleurs, cartes, ports, bindings, catalogues matériels et profils de protocoles compilés.

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
| ARCH-040 | Versionnement des modèles de cartes | **Version initiale intégrée aux catalogues** |
| ARCH-041 | Suppression d’une carte liée | Phase 7 |
| ARCH-058 | Mesure PlatformIO | Bloquant avant intégration |
| ARCH-059 | Mesure heap | Bloquant avant intégration runtime |
| ARCH-060 | Catalogue de types de contrôleurs | **Catalogue minimal réalisé** |
| ARCH-061 | Règles d’adresse spécialisées par contrôleur | **Règles initiales réalisées** |
| ARCH-062 | Inventaire des ports et canaux | **Décidé et prototypé** |
| ARCH-063 | Binding Equipment vers port | **Décidé et prototypé** |
| ARCH-064 | Détection physique des contrôleurs | Différé |
| ARCH-065 | Profil de protocoles compilés | **Décidé et prototypé** |
| ARCH-066 | Catalogue de modèles de cartes | **Catalogue minimal réalisé** |
| ARCH-067 | Politique des canaux partagés | Partage interdit par défaut |
| ARCH-068 | Migration effective de RelayTopology | Phase 7 ou intégration dédiée |
| ARCH-069 | Persistance des bindings | Phase 7 |
| ARCH-070 | Drivers matériels conditionnels | Phase 3 |
| ARCH-071 | Dépendances PlatformIO conditionnelles par protocole | Phase 3 |
| ARCH-072 | Mesure du gain flash par profil | Avant validation de Phase 3 |

## Décisions du Run 2.4

- neuf macros de build sélectionnent les protocoles V4 ;
- le profil initial active GPIO, I2C et VIRTUAL ;
- SPI, UART, ONEWIRE, CAN, RS485 et REMOTE sont désactivés ;
- les catalogues connaissent cinq contrôleurs et cinq modèles de cartes ;
- un type matériel est disponible seulement si son protocole requis est compilé ;
- l’inventaire candidat est validé contre le profil du build ;
- les drivers futurs devront utiliser les mêmes macros ;
- la présence d’un enum ou d’un catalogue n’inclut aucune bibliothèque matérielle ;
- `platformio.ini` expose explicitement le profil actuel ;
- le runtime historique reste inchangé.

## Prochaine étape

Démarrer **AquaLook V4 — Phase 2 — Run 2.5 — Consolidation de l’inventaire matériel et budget de Phase 2**.
