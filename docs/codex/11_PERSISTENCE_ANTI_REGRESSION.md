# AquaLook — Barrière anti-régression de persistance

## Objectif

Toute évolution ajoutant une fonctionnalité, une option, un champ de configuration, une route d’administration ou un comportement de redémarrage doit démontrer qu’elle ne dégrade pas la persistance existante.

Une compilation réussie ne valide jamais la persistance.

## Périmètre critique

Les éléments suivants forment un seul contrat fonctionnel indivisible :

- portail captif ;
- scan Wi-Fi ;
- saisie SSID et mot de passe ;
- écriture NVS ;
- relecture immédiate ;
- redémarrage ;
- rechargement de la configuration ;
- reconnexion Wi-Fi ;
- accès à l’interface Web ;
- conservation des réglages de toutes les zones.

## Règle de livraison

Tout changement touchant directement ou indirectement l’un des éléments suivants impose la campagne complète décrite ci-dessous :

- `ConfigManager.*` ;
- structures `Cfg*` persistées ;
- `CFG_NVS_SCHEMA`, `CFG_NVS_KEY` ou namespace NVS ;
- `WiFiManager.*` ;
- portail captif et routes de sauvegarde Wi-Fi ;
- table de partitions ;
- LittleFS ou ressources Web de secours ;
- ajout d’une option ou d’un champ utilisateur ;
- appel à `ESP.restart()` après une sauvegarde.

Aucun commit concerné ne peut être déclaré validé, fusionné, tagué ou utilisé comme checkpoint sans preuve de cette campagne.

## Invariants obligatoires

1. Un redémarrage ne doit jamais être déclenché avant confirmation de l’écriture et de la relecture de la configuration.
2. Une écriture NVS partielle ou refusée doit laisser la configuration précédente lisible.
3. L’ajout d’un champ persistant impose une évolution explicite du schéma ou la preuve documentée de compatibilité binaire.
4. La migration doit conserver au minimum : Wi-Fi, système, affichage, zones, notifications et ancres d’intervalle.
5. Une valeur sensible telle que le mot de passe ne doit jamais être imprimée dans les logs ; seule sa présence et sa longueur peuvent être journalisées.
6. Le portail captif doit rester fonctionnel même lorsque la configuration principale est absente ou rejetée.
7. La page Web principale doit rester accessible après chaque redémarrage de qualification.

## Campagne minimale obligatoire

### A. Démarrage avec configuration existante

- démarrage complet ;
- `Config: charge depuis NVS` ;
- SSID présent ;
- connexion Wi-Fi obtenue ;
- page principale accessible ;
- zones et réglages historiques présents.

### B. Portail captif

- effacer ou rendre indisponible uniquement la configuration Wi-Fi selon la procédure de test prévue ;
- vérifier l’apparition du portail ;
- exécuter le scan ;
- choisir un SSID ;
- saisir le mot de passe ;
- valider ;
- vérifier que l’écriture est confirmée avant le reboot ;
- vérifier après reboot que le même SSID est relu et que le mot de passe est déclaré présent ;
- vérifier la connexion puis l’accès à la page principale.

### C. Persistance fonctionnelle

- modifier un réglage de zone 1 ;
- modifier un réglage différent de zone 2 ;
- modifier une option système ou d’affichage ;
- redémarrer ;
- vérifier la conservation des trois modifications.

### D. Non-régression Web et stockage

- ouvrir `/` et `/ota` ;
- effectuer plusieurs rafraîchissements ;
- vérifier l’absence de crash AsyncTCP, de faux incident SD et de fallback Web permanent ;
- vérifier que la page reste accessible après redémarrage.

### E. Critères d’échec immédiat

La campagne échoue dès qu’une occurrence apparaît :

- `NOT_ENOUGH_SPACE` ;
- `Config: ecriture NVS incomplete` ;
- configuration absente après un reboot consécutif à une sauvegarde ;
- SSID vide après validation du portail captif ;
- boucle de reboot ;
- page Web inaccessible ;
- perte d’un réglage historique ;
- structure ou CRC rejeté sans migration documentée.

## Preuves à consigner

Le compte rendu de validation doit indiquer :

- commit et SHA embarqué ;
- profils Legacy et V4 compilés ;
- profil réellement flashé ;
- port COM ;
- taille du bloc persistant ;
- schéma NVS ;
- résultat d’écriture et longueur relue ;
- SSID relu et présence du mot de passe, sans afficher le secret ;
- résultat du portail captif ;
- résultat après redémarrage ;
- résultat d’accès Web ;
- réglages modifiés et relus ;
- tests non effectués.

## Règle pour les agents

Avant de proposer une évolution fonctionnelle, rechercher si elle ajoute ou agrandit un champ persistant. Si oui, inclure la migration, les diagnostics et la campagne de non-régression dans le même périmètre. Ne jamais reporter ces éléments à une étape ultérieure implicite.
