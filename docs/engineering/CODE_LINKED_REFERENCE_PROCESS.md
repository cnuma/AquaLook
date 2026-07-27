# Processus de référence directement reliée au code

## Objectif

Maintenir une correspondance vérifiable entre le comportement réel d’AquaLook et son Engineering Reference.

## Définition de terminé documentaire

Une évolution fonctionnelle n’est pas complètement documentée tant que :

- les documents impactés ne sont pas identifiés ;
- les fichiers et symboles modifiés ne sont pas référencés ;
- les interfaces exposées sont à jour ;
- les invariants concernés sont confirmés ;
- les tests et résultats sont reliés aux changements ;
- le registre `35_CODE_TRACEABILITY_REGISTER.md` est mis à jour ;
- un checkpoint indique les documents amendés.

## Informations à relever dans le code

Pour chaque composant :

1. chemins des fichiers ;
2. classes, structures et enums ;
3. fonctions publiques et callbacks ;
4. point d’entrée réellement exécuté ;
5. appels entrants et sortants ;
6. constantes, routes, IDs et clés persistées ;
7. erreurs et modes dégradés ;
8. tests, commandes et observations matérielles.

## Critères de blocage

La consolidation est bloquée si :

- le chemin du code n’est pas vérifié ;
- une signature est reconstruite depuis la mémoire ;
- une route, clé NVS ou structure JSON est supposée ;
- un test matériel est déclaré sans essai ;
- le document décrit une cible comme une fonction active ;
- le code et le document divergent sans écart explicitement enregistré.

## Sortie attendue

Chaque lot de consolidation fournit :

- liste des documents modifiés ;
- positions précises des ajouts ;
- ancrages code confirmés ;
- écarts ouverts ;
- niveau D et niveau de preuve atteints ;
- PR ou commit Git de référence.
