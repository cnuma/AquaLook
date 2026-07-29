# AquaLook — Gouvernance d’architecture

## Objet

Cette règle s’applique à toute évolution significative d’AquaLook pendant toute la vie du produit.

## Piliers permanents

Chaque chantier doit être analysé selon les piliers suivants :

1. Core ;
2. Hardware ;
3. User Experience ;
4. Connectivity ;
5. Cloud ;
6. Cybersecurity ;
7. Observability ;
8. Quality ;
9. Documentation ;
10. Evolution.

Tous les piliers n’exigent pas une modification, mais chacun doit être déclaré `concerné`, `sans impact` ou `à étudier`.

## Fiche obligatoire d’un chantier

Avant développement :

- besoin et résultat attendu ;
- source de vérité Git exacte ;
- piliers concernés ;
- invariants à préserver ;
- risques, notamment cybersécurité ;
- comportement nominal et dégradé ;
- stratégie de test et de rollback ;
- documents à mettre à jour.

À la livraison :

- fichiers et positions modifiés ;
- point d’appel et effet observable ;
- validations réellement exécutées ;
- validations restantes ;
- impacts mémoire, stockage, réseau et matériel ;
- risques nouveaux, réduits ou acceptés ;
- documentation durable mise à jour ;
- commit ou checkpoint de reprise.

## Règles de cybersécurité

La cybersécurité est transverse et ne peut pas être reportée à une phase finale. Tout chantier OTA, MQTT, cloud, VPS, Flutter, authentification, stockage de secrets ou commande distante doit mettre à jour, si nécessaire :

- `docs/security/CYBERSECURITY_ARCHITECTURE.md` ;
- `docs/security/SECURITY_RISK_REGISTER.md` ;
- `docs/roadmap/CYBERSECURITY_LIFECYCLE.md`.

Une fonctionnalité distante ne peut pas être déclarée prête pour production si son authentification, ses autorisations, son chiffrement, sa révocation et sa traçabilité ne sont pas définis.

## Règles documentaires

- `docs/architecture/ARCHITECTURE_OVERVIEW.md` reste la porte d’entrée.
- `docs/codex/` reste le socle détaillé historique.
- Les documents spécialisés complètent le socle sans le recopier intégralement.
- Aucun déplacement ou archivage n’est réalisé sans vérification des liens et remplacement explicite.
- Les checkpoints décrivent un état daté ; ils ne remplacent pas la documentation durable.

## Revue périodique

À chaque palier majeur et au minimum avant une mise en service distante :

- revoir le registre des risques ;
- revoir les dépendances et versions supportées ;
- vérifier les secrets et certificats ;
- vérifier les procédures de sauvegarde, restauration et rollback ;
- vérifier l’observabilité disponible en mode dégradé ;
- retirer ou documenter les composants obsolètes.

## Critère de clôture

Un chantier n’est clos que lorsque le code éventuel, les tests, les risques, la documentation et l’état Git sont cohérents. Une proposition uniquement écrite dans une conversation ne constitue pas une mise à jour du projet.
