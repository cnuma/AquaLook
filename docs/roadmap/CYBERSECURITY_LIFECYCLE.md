# AquaLook — Roadmap cybersécurité sur le cycle de vie

## 1. Principe

La cybersécurité n'est pas un lot final. Elle constitue une activité transverse qui accompagne chaque chantier AquaLook et reste active après la mise en service.

Chaque run fonctionnel doit contenir une section :

- impact cybersécurité ;
- risques introduits ou modifiés ;
- protections retenues ;
- tests de sécurité ;
- éléments de journalisation ;
- documentation et registre des risques mis à jour.

## 2. CYBER-0 — Inventaire et gouvernance

Objectif : connaître ce qui doit être protégé et imposer les règles de travail.

- inventorier appareils, interfaces, ports, protocoles, secrets et dépendances ;
- maintenir `CYBERSECURITY_ARCHITECTURE.md` ;
- maintenir `SECURITY_RISK_REGISTER.md` ;
- créer les ADR pour les choix structurants ;
- définir les responsables de rotation, révocation et traitement d'incident ;
- interdire les secrets dans Git, archives, logs et ressources publiques.

Critère de sortie : toute surface externe connue est décrite et associée à un risque ou à une protection.

## 3. CYBER-1 — Sécurité locale minimale

Objectif : protéger le module avant toute ouverture distante.

- remplacer le verrouillage administratif visuel par une authentification réelle ;
- définir des sessions et autorisations ;
- protéger le point d'accès de secours ;
- limiter les tentatives et tailles de requêtes ;
- valider toutes les entrées ;
- journaliser les actions sensibles ;
- définir une procédure locale de récupération qui ne crée pas de porte dérobée permanente.

Critère de sortie : aucune commande critique locale n'est possible sans authentification et autorisation explicites.

## 4. CYBER-2 — Identité des appareils et secrets

Objectif : permettre le déploiement de plusieurs AquaLook sans secret partagé global.

- créer un identifiant unique par module ;
- définir le provisionnement initial ;
- stocker les secrets dans l'emplacement adapté ;
- définir rotation et révocation ;
- éviter les clés communes à toute la flotte ;
- documenter la perte, le remplacement et la remise à zéro d'un appareil.

Critère de sortie : un appareil compromis peut être révoqué sans bloquer les autres.

## 5. CYBER-3 — OTA sécurisée

Objectif : garantir l'origine et l'intégrité des mises à jour.

- signer les firmwares ;
- vérifier intégrité et provenance ;
- conserver les partitions doubles et le rollback ;
- tracer chaque étape OTA ;
- protéger les secrets de publication ;
- étudier Secure Boot, Flash Encryption et la politique anti-downgrade ;
- tester firmware corrompu, signature invalide, coupure réseau et rollback.

Critère de sortie : un firmware non autorisé ne peut pas devenir la version active.

## 6. CYBER-4 — MQTT et commandes distantes

Objectif : ouvrir le pilotage distant sans exposer directement le module.

- TLS obligatoire ;
- ACL par appareil et par topic ;
- séparation télémétrie, état, commande et administration ;
- messages expirables avec identifiant et anti-rejeu ;
- accusé fondé sur l'état réellement appliqué ;
- limitation de fréquence et taille ;
- comportement sûr hors ligne.

Critère de sortie : une commande forgée, expirée, rejouée ou destinée à un autre appareil est refusée et tracée.

## 7. CYBER-5 — VPS et cloud

Objectif : durcir l'infrastructure distante avant usage réel.

- SSH par clé et comptes séparés ;
- pare-feu restrictif ;
- exposition minimale ;
- mises à jour de sécurité ;
- sauvegardes et restauration testée ;
- supervision, rotation des logs et alertes ;
- gestion centralisée des certificats et secrets ;
- procédure de reconstruction du serveur.

Critère de sortie : l'infrastructure peut être restaurée et les accès compromis peuvent être révoqués.

## 8. CYBER-6 — Application Flutter

Objectif : sécuriser les identités et commandes côté utilisateur.

- stockage sécurisé des jetons ;
- sessions expirables et révocables ;
- validation TLS stricte ;
- rôles et permissions ;
- confirmation des commandes critiques ;
- affichage clair du module ciblé ;
- aucune clé globale intégrée à l'application.

Critère de sortie : la perte d'un téléphone ne donne pas un accès permanent et incontrôlé aux installations.

## 9. CYBER-7 — Observabilité et incident

Objectif : détecter, comprendre et traiter les événements de sécurité.

- définir un journal d'événements de sécurité ;
- centraliser les événements utiles ;
- détecter répétitions d'échecs, certificats invalides, rollback et comportements anormaux ;
- définir niveaux de gravité et alertes ;
- préparer une procédure d'incident ;
- conserver les preuves sans exposer de secrets.

Critère de sortie : un incident laisse des traces exploitables et déclenche une action définie.

## 10. CYBER-8 — Maintenance continue

Objectif : maintenir le niveau de sécurité pendant toute la durée de vie.

- suivre les vulnérabilités des dépendances ;
- planifier les renouvellements de certificats ;
- tester régulièrement sauvegarde et restauration ;
- réviser les comptes et permissions ;
- retirer les accès et versions obsolètes ;
- publier les correctifs de sécurité ;
- documenter la fin de support.

Cette phase ne se termine pas tant qu'une installation AquaLook reste exploitée.

## 11. Dépendances avec les autres chantiers

| Chantier | Prérequis cybersécurité |
|---|---|
| OTA GitHub | CYBER-0, CYBER-2 et CYBER-3 |
| HiveMQ / MQTT | CYBER-0, CYBER-2 et CYBER-4 |
| VPS | CYBER-0 et CYBER-5 |
| Flutter | CYBER-2, CYBER-4, CYBER-5 et CYBER-6 |
| Notifications | Identité, TLS, secrets et limitation des données |
| Carte SD Web | Intégrité des ressources et séparation des secrets |
| Multi-site | Identités individuelles, rôles, révocation, audit et cloisonnement |

## 12. Revue périodique

La revue cybersécurité doit être déclenchée :

- avant chaque exposition d'un nouveau service ;
- avant une release OTA ;
- lors d'un changement de broker, VPS ou fournisseur ;
- après découverte d'une vulnérabilité ;
- après un incident ou une compromission présumée ;
- lors du renouvellement des certificats ;
- lors de l'ajout d'un nouveau type d'appareil ou d'utilisateur ;
- au minimum une fois par an en exploitation.