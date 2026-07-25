# AquaLook — Architecture cybersécurité

## 1. Statut

La cybersécurité est un pilier permanent d'AquaLook. Elle s'applique dès la conception, pendant le développement, au déploiement, en exploitation, lors des mises à jour et jusqu'à la fin de vie des composants.

Ce document définit les principes directeurs. Les choix techniques précis doivent être confirmés par des ADR et validés sur le matériel et l'infrastructure réellement retenus.

## 2. Objectifs

- empêcher une activation non autorisée des équipements ;
- protéger les secrets, identités et données de configuration ;
- garantir l'intégrité et l'origine des firmwares ;
- limiter l'exposition réseau ;
- conserver un fonctionnement local sûr en cas d'attaque ou de panne distante ;
- détecter, tracer et traiter les événements anormaux ;
- permettre la révocation d'un équipement, d'un utilisateur ou d'un certificat compromis ;
- maintenir la sécurité malgré l'évolution des dépendances.

## 3. Modèle de confiance

### 3.1 Principe général

Aucun composant n'est implicitement digne de confiance parce qu'il se trouve sur le réseau local.

Chaque action sensible doit répondre à quatre questions :

1. quelle identité demande l'action ?
2. cette identité est-elle authentifiée ?
3. possède-t-elle l'autorisation exacte ?
4. l'action et son résultat sont-ils traçables ?

### 3.2 Domaines

- domaine embarqué : ESP32 principal et extensions ;
- domaine local : navigateur, Wi-Fi, point d'accès, réseau domestique ;
- domaine distant : VPS, broker MQTT, notifications, GitHub OTA ;
- domaine utilisateur : application Flutter, navigateur, comptes ;
- domaine développement : dépôt Git, CI, postes de développement, secrets de publication.

## 4. Exigences par composant

### 4.1 Firmware ESP32

- désactiver les services inutiles ;
- limiter les routes et commandes administratives ;
- séparer les commandes de consultation et de modification ;
- vérifier les entrées, tailles, bornes et états avant action ;
- refuser en sécurité toute commande invalide ;
- ne jamais journaliser un secret en clair ;
- évaluer Secure Boot et Flash Encryption selon la carte, la chaîne OTA et les contraintes de maintenance ;
- conserver les limites de durée et les sécurités relais indépendamment du réseau.

### 4.2 Wi-Fi et interface locale

- aucun mot de passe universel par défaut ;
- identifiants propres à chaque appareil ;
- point d'accès de secours protégé et signalé ;
- délai, limitation et journalisation des échecs d'authentification ;
- session administrative explicite ;
- protection CSRF et validation d'origine à prévoir pour les actions sensibles ;
- HTTPS à privilégier dès que la mémoire, le certificat et l'architecture le permettent.

Le verrouillage visuel actuel côté navigateur ne constitue pas une authentification forte.

### 4.3 MQTT

- TLS obligatoire hors laboratoire isolé ;
- identité distincte par appareil ;
- ACL par appareil et par topic ;
- commandes séparées des télémétries ;
- messages de commande avec identifiant, horodatage, expiration et anti-rejeu ;
- accusé de réception fondé sur l'état réellement appliqué ;
- révocation individuelle d'un appareil compromis ;
- limitation de débit et de taille des messages.

### 4.4 OTA

- firmware signé ;
- vérification d'intégrité avant activation ;
- provenance de la version vérifiée ;
- partitions doubles et rollback conservés ;
- version minimale autorisée pour empêcher le retour vers une version vulnérable, après étude de la procédure de secours ;
- journalisation du téléchargement, de la validation, du basculement et du rollback ;
- aucun secret de publication stocké dans le firmware ou la carte SD.

### 4.5 VPS et services cloud

- SSH par clé ;
- authentification par mot de passe désactivée lorsque possible ;
- pare-feu restrictif ;
- comptes de service séparés ;
- mises à jour de sécurité suivies ;
- sauvegardes testées ;
- journaux centralisés et rotation ;
- supervision des ressources, connexions et certificats ;
- secrets hors dépôt Git ;
- exposition publique limitée aux ports nécessaires.

### 4.6 Application Flutter

- jetons stockés dans le stockage sécurisé du système ;
- sessions expirables et révocables ;
- aucune clé globale embarquée dans l'application ;
- validation TLS stricte ;
- distinction des rôles utilisateur ;
- confirmation renforcée pour les actions critiques ;
- affichage de l'identité de l'équipement ciblé et du résultat réel.

### 4.7 Développement et chaîne logicielle

- secrets interdits dans Git, les ZIP, logs et checkpoints ;
- dépendances inventoriées et versions maîtrisées ;
- revue des alertes de vulnérabilité ;
- commits et releases traçables ;
- artefacts de publication identifiés par SHA-256 et version ;
- permissions GitHub minimales ;
- procédure de rotation immédiate après fuite présumée.

## 5. Secrets et identités

Catégories minimales :

- identifiants Wi-Fi ;
- identifiants MQTT ;
- certificats et clés privées ;
- jetons API météo et notifications ;
- secrets VPS et CI ;
- jetons utilisateurs mobiles.

Pour chaque secret, documenter : propriétaire, emplacement, méthode de provisionnement, durée de vie, rotation, révocation, sauvegarde éventuelle et comportement après perte.

## 6. Journalisation de sécurité

Événements à prévoir :

- authentification réussie ou refusée ;
- commande distante acceptée ou refusée ;
- modification de configuration critique ;
- changement d'identité ou de certificat ;
- échec TLS, certificat expiré ou non valide ;
- firmware invalide, OTA échouée ou rollback ;
- démarrage en mode de secours ;
- répétition anormale de requêtes ;
- perte ou restauration d'un service de sécurité.

Les journaux ne doivent contenir ni mots de passe, ni clés privées, ni jetons complets.

## 7. Niveaux de déploiement

Les niveaux ci-dessous sont des profils de déploiement, pas des excuses pour désactiver les protections fondamentales.

- laboratoire : accès isolé, diagnostic facilité, aucune exposition Internet ;
- habitation locale : authentification locale, réseau protégé, mises à jour maîtrisées ;
- habitation connectée : MQTT TLS, identités individuelles, commandes distantes tracées ;
- installation professionnelle : supervision, sauvegarde, rotation et procédure d'incident ;
- multi-site : séparation des locataires, gestion centralisée des identités, révocation et audit.

## 8. Cycle de vie sécurité

Chaque évolution suit :

1. identification des actifs et surfaces d'attaque ;
2. analyse des menaces ;
3. définition des protections et du repli ;
4. implémentation minimale ;
5. tests positifs et négatifs ;
6. journalisation vérifiée ;
7. documentation et mise à jour du registre des risques ;
8. suivi des vulnérabilités après livraison ;
9. correction, rotation ou révocation en cas d'incident ;
10. plan de retrait ou fin de support.

## 9. Invariants

- Une perte de cloud ne doit pas désactiver les sécurités locales.
- Une commande non authentifiée ou expirée ne doit jamais activer un équipement.
- Une erreur de sécurité doit conduire à un état sûr et visible.
- Les secrets ne doivent jamais être intégrés aux ressources Web publiques ou à la carte SD.
- Une fonction de récupération ne doit pas devenir une porte dérobée permanente.
- Toute protection dépendant de l'heure doit définir son comportement lorsque NTP est indisponible.
- Toute nouvelle interface externe doit être ajoutée au registre des risques avant mise en service.