# AquaLook — Architecture de mise à jour distante OTA

## Statut

- Niveau documentaire visé : D4
- Branche de consolidation actuelle : `agent/ota-2.4-install-contract`
- Jalons couverts : OTA-2.0 à OTA-2.4
- Firmware matériel de référence : `ProgrammeArrosage_v4`
- Dernier palier matériel validé : OTA-2.3 `CHECK_VERSION`

## 1. Finalité

Le sous-système OTA prépare puis exécutera une mise à jour distante sans compromettre la fonction critique d’arrosage.

L’architecture sépare strictement :

- le runtime complet, qui exécute l’arrosage, l’écran, le Web et les services courants ;
- le mode maintenance minimal, seul autorisé à établir une connexion TLS GitHub ;
- la découverte de version, actuellement validée ;
- le futur téléchargement de contrôle sans écriture ;
- la future écriture dans la partition inactive ;
- la future activation avec confirmation de démarrage sain et rollback.

Le contrat normatif d’installation est `docs/ota/INSTALLATION_CONTRACT_V1.md`.

## 2. Architecture actuellement validée

```text
Navigateur /ota
      |
      | POST commande maintenance
      v
WebManager
      |
      | contrôle arrosage inactif
      | persistance commande NVS
      v
Redémarrage différé
      |
      v
MaintenanceSetupWrapper
      |
      | tâche FreeRTOS dédiée, cœur 0, pile 16384 octets
      v
MaintenanceBoot
      |
      | Wi-Fi minimal
      | TLS GitHub
      | probe ou CHECK_VERSION
      | persistance du résultat NVS
      v
Redémarrage vers runtime normal
      |
      v
GET /api/maintenance/last-result
      |
      v
Affichage /ota
```

## 3. Composants

### `MaintenanceRequestStore`

Stocke la commande de maintenance dans le namespace NVS `aq_maint`. La commande est effacée avant exécution afin d’empêcher une boucle de redémarrage persistante.

### `MaintenanceSetupWrapper`

Intercepte le démarrage avant le `setup()` nominal. Lorsqu’une commande est présente, il lance la maintenance dans une tâche FreeRTOS dédiée, suspend le chemin nominal et prévoit un repli sûr si la tâche ne peut pas être créée.

### `MaintenanceBoot`

Exécute uniquement les opérations autorisées en mode minimal.

État actuel :

- `PROBE_GITHUB` : validé ;
- `CHECK_VERSION` : validé sur matériel V4 ;
- téléchargement du firmware : absent ;
- écriture OTA : absente ;
- activation d’une nouvelle partition : absente.

### `MaintenanceResultStore`

Stocke dans le namespace NVS `aq_maint_res` les informations de résultat, notamment : validité, succès, commande, ligne HTTP, durée TLS, uptime, heap minimale, détail d’erreur, versions, cible, environnement, URL, taille, SHA-256 et disponibilité d’une mise à jour.

Ce stockage devra être étendu de manière bornée pour représenter la future machine d’état transactionnelle.

### `OtaBuildIdentity`

Constitue la source compile-time de l’identité OTA : version, cible `legacy` ou `v4`, environnement PlatformIO, carte et emplacement du manifeste.

Legacy et V4 ciblant le même matériel physique, la cible OTA ne doit jamais être déduite uniquement de la carte.

### `WebManager`

Expose actuellement :

- `GET /ota` ;
- `POST /api/maintenance/probe-github` ;
- la commande de vérification de version ;
- `GET /api/maintenance/last-result`.

Toute commande est refusée avec une zone active. Le redémarrage est exécuté hors callback AsyncTCP.

## 4. Invariants de sécurité

1. Aucune opération OTA pendant un arrosage actif.
2. Aucun TLS GitHub dans le runtime complet.
3. Aucune modification du moteur d’arrosage.
4. Aucune libération de sprites pour récupérer de la mémoire.
5. Aucun usage de `initVariant()`.
6. Maintenance dans une tâche dédiée.
7. Commande NVS effacée avant exécution.
8. Retour automatique au runtime normal après succès ou échec des opérations de lecture.
9. Aucune écriture dans la partition active.
10. Aucune écriture dans la partition inactive avant le palier OTA-3.1 explicitement validé.
11. Aucun changement de partition de démarrage avant validation de l’image écrite.
12. `INSTALL_UPDATE` reste désactivé jusqu’à validation du premier boot et du rollback.
13. Aucune installation croisée Legacy vers V4 ou V4 vers Legacy.
14. Toute étape irréversible doit être précédée par la persistance de l’état transactionnel.

## 5. État des partitions

Le matériel réel utilise un layout dual OTA :

- `app0` / `ota_0` : 2 031 616 octets ;
- `app1` / `ota_1` : 2 031 616 octets ;
- `otadata` : 8 192 octets.

Les validations OTA-2.x ont été réalisées avec `app0` active et `app1` non écrite.

La future partition cible devra être obtenue par l’API OTA ESP-IDF. Aucune adresse ni hypothèse `app0`/`app1` ne doit être codée en dur.

## 6. Séquence cible industrialisable

```text
CHECK_VERSION
  -> MANIFEST_VALID

DOWNLOAD_UPDATE_TEST
  -> DOWNLOADING
  -> DOWNLOADED
  -> HASH_VERIFIED
  -> retour runtime, otaWrite=no

STAGE_UPDATE
  -> WRITING
  -> WRITTEN
  -> retour runtime sans changement de boot

INSTALL_UPDATE
  -> PENDING_BOOT
  -> changement de partition de démarrage
  -> BOOT_TEST
  -> CONFIRMED
       ou
     ROLLED_BACK
```

Les responsabilités et conditions exactes sont fixées par `INSTALLATION_CONTRACT_V1.md`.

## 7. Modes dégradés

Déjà couverts :

- SSID absent ;
- Wi-Fi indisponible ;
- TLS impossible ;
- réponse HTTP absente ou invalide ;
- manifeste vide, invalide ou trop volumineux ;
- cible absente ou incompatible ;
- création de tâche impossible ;
- arrosage actif.

À couvrir avant installation :

- firmware tronqué ;
- taille réelle différente du manifeste ;
- SHA-256 incorrect ;
- redirection interdite ;
- perte Wi-Fi pendant transfert ;
- coupure secteur pendant écriture ;
- écriture flash partielle ;
- image non amorçable ;
- absence de confirmation de démarrage sain ;
- rollback échoué.

## 8. Sécurité cryptographique

Le code actuel utilise `setInsecure()` pour les probes et la lecture du manifeste. Cette tolérance ne peut pas être conservée pour l’installation.

Le futur chemin d’installation devra cumuler :

- validation TLS par autorité de certification ;
- restriction stricte des hôtes et redirections ;
- SHA-256 calculé localement ;
- signature cryptographique du manifeste ou du firmware ;
- clé publique embarquée ;
- rotation et révocation documentées.

Le choix Ed25519 ou ECDSA reste à formaliser par une décision d’architecture dédiée.

## 9. LittleFS

La mise à jour LittleFS n’est pas couplée aux premiers paliers firmware OTA.

Elle nécessitera un artefact distinct, une matrice de compatibilité, une validation d’intégrité et une stratégie transactionnelle garantissant qu’une interface minimale de récupération reste disponible.

## 10. Observabilité

Les logs `Maintenance:` fournissent actuellement : commande, entrée en mode minimal, mémoire, durée TLS, ligne HTTP, résultat et garantie `otaWrite=no`.

Les futurs paliers devront ajouter :

- identifiant de transaction ;
- état courant ;
- partition active et cible ;
- octets attendus, reçus et écrits ;
- SHA-256 attendu et calculé ;
- débit ;
- résultat du premier boot ;
- rollback ;
- code d’erreur stable.

## 11. Étape suivante

OTA-3.0 introduira `DOWNLOAD_UPDATE_TEST`.

Cette étape devra télécharger le firmware complet en flux et valider sa taille et son SHA-256 sans appeler de primitive d’écriture OTA. La preuve matérielle devra conserver explicitement :

```text
otaWrite=no
```
