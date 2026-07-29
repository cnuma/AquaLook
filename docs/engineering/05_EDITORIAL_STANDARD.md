# AquaLook Engineering Reference — Standard éditorial

- Version documentaire : 1.0
- Statut : référence
- Dernière consolidation : 2026-07-27

## Objet

Ce document fixe le format des tomes du référentiel d’ingénierie.

## Sources

L’ordre de confiance est le suivant :

1. code et configuration du commit ciblé ;
2. checkpoint exact de la branche ;
3. invariants et décisions versionnés ;
4. documentation d’ingénierie ;
5. architectures spécialisées ;
6. roadmaps ;
7. conversations et notes historiques.

Une information issue uniquement d’une conversation ne doit pas être présentée comme un fait implémenté.

## Style

Le référentiel utilise un style factuel et impersonnel. Les formulations conversationnelles, appréciations de l’auteur, propositions de suite et commentaires de fin de chapitre sont interdits.

Les éléments futurs sont uniquement référencés vers la roadmap correspondante. Ils ne sont pas décrits comme existants.

## En-tête obligatoire

Chaque tome comporte : titre, version, statut, date de consolidation, sources, composants concernés et niveau de maturité documentaire.

## Sections communes

Selon le composant : objet, mission, responsabilités, responsabilités exclues, architecture, interfaces, données, séquences, états, modes dégradés, invariants, tests, risques et références.

## Interfaces exposées

Toute interface réellement présente est recensée avec son usage :

- URL et méthode HTTP ;
- topic MQTT ;
- port et protocole ;
- fichier ou répertoire ;
- bus, adresse et broches matérielles ;
- interface interne stable.

Aucune URL, route, topic ou signature ne doit être inventé. Une interface non confirmée est indiquée comme « à extraire du code ».

## Historique

### 1.0

Création du standard éditorial et consolidation des règles établies pendant la préparation du référentiel v1.0.
