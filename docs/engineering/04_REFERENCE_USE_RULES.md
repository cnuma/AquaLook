# AquaLook — Règles d'utilisation de la référence Git

## Avant toute proposition

1. identifier le dépôt, la branche et le commit ciblés ;
2. lire `docs/START_HERE.md` ;
3. lire le checkpoint le plus récent applicable ;
4. consulter le manuel d'ingénierie et les documents spécialisés du domaine ;
5. inspecter le code réel avant de conclure.

## Interdictions

- ne pas reconstruire un fichier depuis une conversation ancienne ;
- ne pas réintroduire une solution abandonnée sans relire la cause de son abandon ;
- ne pas présenter une roadmap comme un comportement déjà implémenté ;
- ne pas fusionner des informations provenant de branches différentes sans le signaler ;
- ne pas remplacer un invariant validé par une préférence implicite.

## Après une évolution

Mettre à jour selon le cas :

- `01_PROJECT_STATUS.md` pour la situation globale ;
- `02_SYSTEM_OVERVIEW.md` pour l'architecture générale ;
- le chapitre technique concerné ;
- un ADR pour une décision structurante ;
- le registre des risques ;
- la stratégie de tests ;
- le checkpoint de reprise.

## Résolution des divergences

Si le manuel diffère du code ou d'un checkpoint plus récent :

1. ne pas masquer l'écart ;
2. déterminer quelle source correspond au commit ciblé ;
3. corriger le manuel dans la même branche ou ouvrir un chantier documentaire dédié ;
4. conserver la justification du changement.

## Principe de continuité

Le but n'est pas de figer l'architecture. Le dépôt conserve les décisions afin de pouvoir les faire évoluer consciemment, avec leurs raisons, conséquences, migrations et validations.
