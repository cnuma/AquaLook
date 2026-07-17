# Roadmap — refactorisation de l’interface d’administration et de configuration

## Décision

La configuration ntfy est provisoirement intégrée à la page `/logs` uniquement pour accélérer la validation fonctionnelle du transport de notifications et de la file persistante.

Cette organisation n’est pas la cible définitive.

## Constat

L’interface d’administration AquaLook regroupe progressivement des paramètres de nature différente dans plusieurs pages et sections. Le risque est de rendre la configuration difficile à comprendre, à maintenir et à faire évoluer.

La page de journal doit rester centrée sur :

- le journal technique volatile ;
- les incidents persistants ;
- leur état et leur acquittement ;
- l’état de livraison des notifications ;
- le test manuel du canal de notification.

La page de journal ne doit pas devenir une page générale de configuration.

Les messages de confirmation, d’erreur et de progression ne doivent pas apparaître dans une fenêtre flottante au milieu du contenu. Cette implantation complique la gestion des zones, masque des informations et rompt la cohérence visuelle. Les retours opérateur doivent être intégrés dans le bandeau de la page ou dans une zone d’état fixe directement rattachée à celui-ci.

## Cible fonctionnelle

Réorganiser l’administration autour de catégories stables :

1. **Système**
   - paramètres généraux du module ;
   - nombre de zones et matériel ;
   - temps, veille et comportement global.

2. **Réseau**
   - Wi-Fi ;
   - NTP ;
   - services réseau sortants.

3. **Notifications**
   - activation ;
   - serveur ntfy ;
   - sujet privé ;
   - jeton facultatif ;
   - niveaux et types d’événements ;
   - politique de réessai ;
   - test manuel.

4. **Incidents et journal**
   - incidents actifs, récupérés et acquittés ;
   - historique persistant utile à l’opérateur ;
   - journal technique volatile ;
   - acquittement individuel ;
   - état de la file de notification.

5. **Affichage**
   - thème ;
   - couleurs ;
   - dispositions ;
   - fréquence de rafraîchissement.

6. **Arrosage**
   - zones ;
   - planning ;
   - pluie ;
   - durées manuelles.

## Principes d’interface

- une configuration ne doit avoir qu’un seul emplacement de référence ;
- les secrets ne doivent jamais être renvoyés en clair par l’API ;
- les pages de diagnostic ne doivent pas servir de stockage permanent de réglages hétérogènes ;
- les actions opérateur doivent être séparées des paramètres système ;
- les confirmations et erreurs doivent être intégrées au bandeau ou à une zone d’état fixe, jamais dans une fenêtre flottante couvrant le contenu ;
- les pages doivent rester utilisables avec la SD absente grâce aux fallbacks essentiels ;
- l’organisation doit rester cohérente entre les ressources `data/` et les routes du firmware.

## Ordre prévu

Cette refactorisation est différée jusqu’à la validation fonctionnelle et matérielle des notifications ntfy.

Ordre recommandé :

1. valider l’envoi ntfy et les réessais ;
2. créer un checkpoint stable du chantier SD/incidents/notifications ;
3. inventorier toutes les routes et tous les paramètres actuels ;
4. proposer une arborescence d’administration unifiée ;
5. déplacer la configuration ntfy hors de `/logs` ;
6. déplacer les retours flottants vers le bandeau commun des pages ;
7. consolider les autres configurations dispersées ;
8. valider les fallbacks SD/LittleFS et les migrations NVS ;
9. créer un checkpoint dédié avant toute reprise fonctionnelle importante.

## Invariant temporaire

Tant que cette refactorisation n’est pas réalisée, la présence de la configuration ntfy dans `/logs` est considérée comme une implantation provisoire de validation, et non comme une décision d’architecture d’interface définitive.
