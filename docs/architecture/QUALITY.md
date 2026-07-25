# AquaLook — Pilier Qualité

## Objectif

Prévenir les régressions et démontrer qu'une modification agit réellement sur le besoin demandé, tout en préservant les invariants matériels, fonctionnels et documentaires.

## Domaines couverts

- compilation des environnements concernés ;
- tests unitaires et tests de composants ;
- tests d'intégration ;
- tests Web et stockage ;
- tests sur matériel réel ;
- bancs de tests orchestrateur et relais ;
- validation OTA et restauration ;
- analyse statique et revue de dépendances ;
- contrôle documentaire et reproductibilité des checkpoints.

## Niveaux de validation

1. **Statique** : cohérence du diff, format, absence de duplication et analyse des appels.
2. **Compilation** : tous les environnements impactés compilent.
3. **Fonctionnelle locale** : le chemin d'exécution est réellement relié et observable.
4. **Intégration** : les modules concernés interagissent sans régression.
5. **Matérielle** : validation sur la carte et les périphériques réels.
6. **Endurance** : observation prolongée pour les changements réseau, stockage, mémoire ou concurrence.

Une validation de niveau inférieur ne doit jamais être présentée comme une validation matérielle ou d'endurance.

## Matrice obligatoire par évolution

Chaque chantier significatif doit documenter :

- exigences et résultat attendu ;
- fichiers, fonctions et points d'appel concernés ;
- invariants à préserver ;
- tests exécutés et résultats ;
- tests non exécutés et raison ;
- comportement dégradé ;
- impacts mémoire, flash, stockage et temps d'exécution ;
- risques cybersécurité ;
- conditions de rollback.

## Critères de sortie

Une évolution est prête à être proposée pour fusion lorsque :

- le diff est limité au périmètre ;
- les contrôles disponibles sont réussis ;
- les limitations sont explicites ;
- la documentation durable est à jour ;
- le registre des risques est ajusté si nécessaire ;
- un état de reprise précis est disponible pour les paliers importants.

## Tests spécifiques par domaine

### Relais et moteur

- démarrage sûr ;
- commande d'une zone courte sous surveillance ;
- durée maximale ;
- arrêt après erreur ou redémarrage ;
- aucun pilotage direct hors abstraction prévue.

### Web et stockage

- routes et IDs conservés ;
- mode SD et repli ;
- portail captif ;
- build des ressources embarquées ;
- absence de blocage du planificateur.

### Réseau, MQTT et cloud

- perte et reprise de connexion ;
- authentification refusée ;
- commandes dupliquées ou anciennes ;
- fonctionnement local sans service distant.

### OTA

- intégrité et authenticité ;
- partitions et taille ;
- échec contrôlé ;
- rollback ;
- conservation de la configuration compatible.

## Dette qualité

Toute exception temporaire doit être inscrite dans le document de risques ou la roadmap avec propriétaire logique, impact, condition de résolution et interdiction éventuelle de mise en production.
