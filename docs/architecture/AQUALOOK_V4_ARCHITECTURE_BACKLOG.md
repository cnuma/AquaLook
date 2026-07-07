# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Dernière mise à jour :** Phase 2 — Run 2.5, 7 juillet 2026

## État général

Les Phases 1 et 2 sont clôturées comme socles architecturaux isolés.

La Phase 2 couvre désormais :

```text
bus
contrôleurs
cartes
ports
bindings
catalogues matériels
profils de protocoles compilés
budget de capacité
passerelle relais historique
```

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
| ARCH-001 à ARCH-009 | Domaine Phase 1 | **Clôturé architecturalement** |
| ARCH-010 à ARCH-012 | Bus, contrôleurs et adresses | **Décidé et prototypé** |
| ARCH-011 / ARCH-062 | Cartes, ports et canaux | **Décidé et prototypé** |
| ARCH-013 à ARCH-016 | Actionneurs et compatibilité historique | Binding réalisé, drivers en Phase 3 |
| ARCH-038 | CI et tests hôte | Ouvert |
| ARCH-040 | Versionnement des modèles de cartes | **Version initiale intégrée** |
| ARCH-041 | Suppression d’une carte liée | Phase 7 |
| ARCH-058 | Mesure PlatformIO | Bloquant avant intégration |
| ARCH-059 | Mesure heap | Bloquant avant intégration runtime |
| ARCH-060 / ARCH-066 | Catalogues matériels | **Catalogues minimaux réalisés** |
| ARCH-061 | Règles spécialisées d’adresse | **Règles initiales réalisées** |
| ARCH-063 | Binding Equipment vers Port | **Décidé et prototypé** |
| ARCH-064 | Détection physique des contrôleurs | Différé |
| ARCH-065 | Profil de protocoles compilés | **Décidé et prototypé** |
| ARCH-067 | Politique des canaux partagés | Partage interdit par défaut |
| ARCH-068 | Migration effective de RelayTopology | Phase 7 ou intégration dédiée |
| ARCH-069 | Persistance des bindings | Phase 7 |
| ARCH-070 | Drivers matériels conditionnels | Phase 3 |
| ARCH-071 | Dépendances PlatformIO conditionnelles | Phase 3 |
| ARCH-072 | Mesure du gain flash par profil | Avant validation de Phase 3 |
| ARCH-073 | Budget Phase 2 | **Décidé et verrouillé** |
| ARCH-074 | Double buffering actif/candidat | **Budgété, activation différée** |

## Budgets de la Phase 2

```text
SMALL       832 octets
STANDARD  2 816 octets
EXTENDED  5 632 octets
```

Avec inventaire actif et candidat simultanés :

```text
SMALL      1 664 octets
STANDARD   5 632 octets
EXTENDED  11 264 octets
```

Le profil `STANDARD` est la référence.

## Budget indicatif Phase 1 + Phase 2

```text
Phase 1 STANDARD             10 592 octets
Phase 2 STANDARD actif        2 816 octets
Total isolé                  13 408 octets

Phase 1 STANDARD
+ Phase 2 actif/candidat     16 224 octets
```

Les drivers, buffers de protocoles, Wi-Fi, Web, écran et stacks restent exclus.

## Décision de clôture

La **Phase 2 est clôturée sur le plan architectural et des modèles isolés**.

Le passage à la Phase 3 est autorisé sous réserve de :

- conserver les drivers conditionnels ;
- ne pas raccorder immédiatement le moteur historique ;
- compiler et mesurer avant toute intégration runtime.

## Prochaine étape

Démarrer **AquaLook V4 — Phase 3 — Run 3.1 — Contrat générique des actionneurs binaires et registre de drivers**.
