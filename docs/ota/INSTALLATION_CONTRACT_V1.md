# AquaLook — Contrat d’installation OTA v1

## 1. Statut

Ce document fixe le contrat préalable à toute écriture OTA sur un module AquaLook.

État au palier OTA-2.4 :

- `CHECK_VERSION` est fonctionnel et validé sur matériel V4 ;
- le manifeste GitHub est téléchargé, validé et comparé à la version installée ;
- aucune commande d’installation n’est active ;
- aucune écriture dans la partition OTA inactive n’est autorisée ;
- aucune modification de `otadata` ou de la partition de démarrage n’est autorisée ;
- le téléchargement complet du firmware et le calcul SHA-256 en flux restent à implémenter dans un palier séparé.

Le présent contrat est normatif pour les futurs paliers OTA-3.x et OTA-4.x.

## 2. Objectif

L’installation OTA doit permettre de transférer, vérifier, écrire et activer un firmware AquaLook publié dans une GitHub Release sans compromettre :

1. la sécurité des relais ;
2. la continuité du runtime actuel en cas d’échec ;
3. la distinction stricte entre Legacy et V4 ;
4. la compatibilité du schéma de partitions ;
5. la possibilité de retour automatique au firmware précédent ;
6. la traçabilité complète de chaque étape.

## 3. Séparation obligatoire des commandes

Les responsabilités restent séparées :

### `CHECK_VERSION`

Opération de lecture seule déjà autorisée :

- télécharger et valider le manifeste ;
- sélectionner la cible courante ;
- comparer les versions ;
- persister et afficher le résultat.

Interdictions : téléchargement du binaire, ouverture d’une partition OTA en écriture, modification de `otadata`, changement de partition de démarrage.

### `DOWNLOAD_UPDATE_TEST`

Palier OTA-3.0, sans écriture flash :

- télécharger le firmware en flux ;
- calculer SHA-256 pendant le flux ;
- vérifier la taille exacte ;
- comparer le hash calculé au manifeste ;
- mesurer durée, débit, heap minimale et erreurs ;
- détruire les données reçues après validation.

Cette commande doit conserver la garantie explicite `otaWrite=no`.

### `STAGE_UPDATE`

Palier OTA-3.1, écriture dans la partition inactive uniquement :

- revalider le manifeste ;
- identifier la partition inactive ;
- écrire le firmware par blocs bornés ;
- calculer et vérifier SHA-256 pendant l’écriture ;
- finaliser l’image sans modifier la partition de démarrage ;
- persister l’état `WRITTEN` ou `FAILED`.

### `INSTALL_UPDATE`

Palier OTA-4.x, activation contrôlée :

- exiger un firmware préalablement validé dans la partition inactive ;
- enregistrer l’état `PENDING_BOOT` ;
- sélectionner la nouvelle partition de démarrage ;
- redémarrer ;
- exiger une confirmation de démarrage sain ;
- déclencher un rollback automatique en absence de confirmation.

## 4. Machine d’état transactionnelle

Les états normatifs sont :

```text
IDLE
MANIFEST_VALID
DOWNLOADING
DOWNLOADED
HASH_VERIFIED
WRITING
WRITTEN
PENDING_BOOT
BOOT_TEST
CONFIRMED
ROLLED_BACK
FAILED
```

Règles :

- chaque transition est persistée avant l’étape irréversible suivante ;
- un redémarrage inattendu doit permettre de déterminer l’étape interrompue ;
- `FAILED` conserve un code d’erreur et l’étape d’origine ;
- `CONFIRMED` est le seul état autorisant l’effacement de la transaction précédente ;
- aucune transition ne peut contourner `HASH_VERIFIED` avant écriture ni `BOOT_TEST` avant confirmation.

## 5. Compatibilité obligatoire

Le manifeste d’installation doit permettre de vérifier au minimum :

- cible OTA : `legacy` ou `v4` ;
- environnement PlatformIO exact ;
- carte ou famille matérielle ;
- schéma de partitions attendu ;
- taille exacte du binaire ;
- SHA-256 du binaire ;
- version minimale installée autorisée ;
- version minimale du bootloader ou du mécanisme OTA si nécessaire ;
- canal de diffusion ;
- politique de downgrade.

Une incompatibilité sur un seul de ces critères doit provoquer un refus avant téléchargement ou écriture.

## 6. Politique de version et de canal

Politique initiale retenue :

- `stable` : accepté par défaut ;
- `beta` : accepté uniquement si le module est explicitement configuré pour ce canal ;
- `dev` : interdit sur une installation normale ; réservé aux essais contrôlés ;
- downgrade : interdit par défaut ;
- même version : aucune installation ;
- version distante plus ancienne : information seulement, sans installation ;
- changement de cible Legacy/V4 : toujours interdit.

Toute exception doit être explicite, visible dans l’interface et journalisée.

## 7. Sécurité cryptographique

Le code HTTP, le nom du fichier et SHA-256 ne suffisent pas à authentifier une release.

Avant activation de `INSTALL_UPDATE`, la stratégie finale doit inclure :

1. validation TLS avec autorité de certification, sans `setInsecure()` sur le chemin d’installation ;
2. contrôle strict des hôtes et redirections ;
3. SHA-256 calculé localement sur le firmware reçu ;
4. signature cryptographique du manifeste ou du firmware ;
5. clé publique embarquée en lecture seule ;
6. identifiant et version de clé ;
7. procédure documentée de rotation et de révocation.

Algorithmes candidats : Ed25519 ou ECDSA. Le choix final devra faire l’objet d’une décision d’architecture dédiée avant implémentation.

## 8. Partition et écriture

Invariants :

- la partition active ne doit jamais être ouverte en écriture ;
- la partition cible doit provenir de l’API OTA ESP-IDF, pas d’une adresse écrite en dur ;
- la taille annoncée doit être inférieure ou égale à la taille utile de la partition inactive ;
- la taille réellement reçue doit être strictement égale à la taille du manifeste ;
- toute erreur d’écriture doit interrompre et invalider la transaction ;
- la partition de démarrage ne doit être modifiée qu’après validation complète de l’image écrite ;
- aucune écriture LittleFS ne doit être couplée au premier palier firmware OTA.

## 9. Confirmation de démarrage sain

Le nouveau firmware ne peut être confirmé qu’après validation d’un ensemble minimal de critères :

- démarrage sans boucle ni watchdog ;
- chargement ou migration valide de la configuration ;
- initialisation sûre des relais ;
- backend correspondant à la cible compilée réellement instancié ;
- serveur Web disponible ;
- EventLog opérationnel ;
- absence d’erreur critique pendant une fenêtre de surveillance bornée.

La confirmation doit utiliser le mécanisme OTA ESP-IDF prévu pour marquer l’application valide. Un simple délai ou l’arrivée dans `setup()` ne constitue pas une confirmation suffisante.

## 10. Rollback

Le rollback doit être testé sur matériel avec au minimum :

- crash avant `setup()` ;
- crash pendant `setup()` ;
- redémarrage en boucle ;
- watchdog ;
- absence de confirmation ;
- image invalide ;
- coupure d’alimentation avant et après changement de partition de démarrage.

Le résultat attendu est le retour automatique au firmware précédent, sans activation intempestive des relais et avec un diagnostic persistant exploitable.

## 11. LittleFS

La mise à jour LittleFS est exclue des premiers paliers d’installation firmware.

Avant ajout, il faudra définir :

- un artefact LittleFS distinct ;
- sa taille et son SHA-256 ;
- une matrice de compatibilité firmware / ressources ;
- une stratégie transactionnelle ou de repli ;
- le comportement en cas de coupure d’alimentation ;
- la conservation d’une interface minimale de récupération.

## 12. Observabilité minimale

Chaque transaction doit journaliser et persister :

- identifiant de transaction ;
- commande ;
- état courant ;
- version installée et disponible ;
- cible et environnement ;
- partition active et partition cible ;
- octets attendus, reçus et écrits ;
- SHA-256 attendu et calculé ;
- durée, débit, heap minimale ;
- code d’erreur stable ;
- motif détaillé ;
- résultat du premier boot et du rollback.

Les journaux ne doivent jamais contenir de mot de passe Wi-Fi, jeton ou secret de signature.

## 13. Conditions préalables au palier OTA-3.0

OTA-3.0 peut commencer uniquement lorsque :

- ce contrat est relu et accepté comme référence ;
- le manifeste v1 est aligné sur les champs de compatibilité retenus ;
- les limites de taille et de temps sont documentées ;
- la commande de test est distincte de toute commande d’écriture ;
- le résultat persistant peut représenter taille, hash, durée et étape ;
- les scénarios de test réseau sont définis.

OTA-3.0 doit se terminer avec une preuve matérielle de téléchargement et de vérification SHA-256, tout en conservant `otaWrite=no`.
