# AquaLook — Guide développeur pour prolonger le chantier OTA

## 1. Lecture obligatoire

Avant toute modification OTA, lire dans cet ordre :

1. `AGENTS.md` ;
2. le dernier checkpoint de `docs/checkpoints/` pour la branche ;
3. `docs/engineering/OTA_REMOTE_UPDATE_ARCHITECTURE.md` ;
4. `docs/firmware/OTA_MAINTENANCE_RUNTIME.md` ;
5. `docs/ota/GITHUB_MANIFEST_V1.md` ;
6. `docs/ota/INSTALLATION_CONTRACT_V1.md` ;
7. les fichiers `MaintenanceRequest.*`, `MaintenanceSetupWrapper.cpp`, `MaintenanceBoot.*`, `MaintenanceResult.*`, `OtaBuildIdentity.h` et `WebManager.*`.

Ne jamais reconstruire le mécanisme depuis un ancien extrait ou une conversation.

## 2. État actuel

Au palier OTA-2.4 :

- `PROBE_GITHUB` est fonctionnel ;
- `CHECK_VERSION` est fonctionnel et validé sur matériel V4 ;
- le manifeste est généré par le workflow de release ;
- aucune commande ne télécharge encore le firmware complet ;
- aucune primitive d’écriture OTA ne doit être appelée ;
- `INSTALL_UPDATE` reste désactivé.

Le prochain palier de code est OTA-3.0 : téléchargement complet et validation SHA-256 avec `otaWrite=no`.

## 3. Ajouter une commande de maintenance

Pour ajouter une commande :

1. déclarer la valeur dans `MaintenanceRequest` sans renuméroter les valeurs existantes ;
2. compléter `MaintenanceRequestStore::name()` et la validation des valeurs ;
3. ajouter une route Web distincte ;
4. vérifier l’absence d’arrosage actif avant l’enregistrement ;
5. répondre au client avant de programmer le redémarrage ;
6. traiter la commande dans `MaintenanceBoot::runIfRequested()` ;
7. effacer la commande avant son exécution ;
8. borner tous les délais, tailles et allocations ;
9. persister un résultat exploitable ;
10. garantir le redémarrage vers le runtime normal sur tous les chemins autorisés ;
11. journaliser explicitement `otaWrite=yes` ou `otaWrite=no` ;
12. documenter la transition de la machine d’état transactionnelle.

Une commande inconnue ou non implémentée doit être refusée sans lancer d’opération réseau ou OTA.

## 4. Étendre le résultat persistant

`MaintenanceResult` constitue le contrat entre la maintenance minimale et le runtime normal.

Lors d’une extension :

- ajouter uniquement les champs nécessaires ;
- conserver des tailles de chaînes bornées ;
- éviter les allocations dynamiques persistantes ;
- conserver la compatibilité avec un résultat ancien absent ou partiel ;
- documenter chaque nouvelle clé NVS ;
- vérifier le comportement lorsque `putString()` reçoit une chaîne vide ;
- ne pas transformer ce stockage en journal historique illimité ;
- prévoir un code d’erreur stable distinct du texte de diagnostic ;
- persister l’état avant toute étape irréversible.

Pour OTA-3.0, le résultat devra au minimum pouvoir représenter :

- taille attendue ;
- taille reçue ;
- SHA-256 attendu ;
- SHA-256 calculé ;
- durée ;
- débit ;
- heap minimale ;
- étape et code d’erreur.

## 5. `CHECK_VERSION`

`CHECK_VERSION` est une opération de lecture et de validation.

Séquence actuelle :

1. identifier explicitement la cible compilée ;
2. connecter le Wi-Fi en maintenance minimale ;
3. ouvrir TLS vers une URL GitHub autorisée ;
4. plafonner le manifeste à 8 Kio ;
5. suivre au maximum deux redirections autorisées ;
6. parser le JSON ;
7. vérifier le schéma et le canal ;
8. sélectionner `legacy` ou `v4` ;
9. vérifier `board`, `environment`, `firmwareUrl`, `size` et `sha256` ;
10. comparer la version disponible à la version installée ;
11. persister le résultat ;
12. fermer les ressources ;
13. redémarrer normalement.

Il est interdit d’ajouter une écriture OTA dans ce chemin.

## 6. Implémenter OTA-3.0 `DOWNLOAD_UPDATE_TEST`

Objectif : valider le transfert complet d’un firmware publié sans écrire en flash.

Séquence imposée :

1. charger un résultat `CHECK_VERSION` valide et récent ;
2. revalider la cible, l’environnement, l’URL, la taille et le SHA-256 ;
3. refuser si aucune mise à jour compatible n’est disponible ;
4. ouvrir une connexion HTTPS vers un hôte autorisé ;
5. suivre les redirections dans la limite documentée ;
6. refuser une taille HTTP incompatible lorsqu’elle est connue ;
7. initialiser un contexte SHA-256 avant lecture du corps ;
8. lire le firmware par blocs bornés ;
9. alimenter SHA-256 pour chaque bloc reçu ;
10. compter strictement les octets ;
11. appliquer un timeout d’inactivité borné ;
12. calculer le hash final ;
13. comparer taille et SHA-256 ;
14. persister mesures et résultat ;
15. fermer toutes les ressources ;
16. redémarrer normalement.

Interdictions absolues pendant OTA-3.0 :

```text
Update.begin()
Update.write()
Update.end()
esp_ota_begin()
esp_ota_write()
esp_ota_end()
esp_ota_set_boot_partition()
```

La sortie série et le résultat Web doivent afficher explicitement :

```text
otaWrite=no
```

## 7. Préparation d’OTA-3.1

OTA-3.1 ne peut commencer qu’après validation matérielle d’OTA-3.0 avec :

- téléchargement complet ;
- taille exacte ;
- SHA-256 identique ;
- heap suffisante ;
- comportement borné en cas de coupure ;
- retour nominal au runtime ;
- absence vérifiée d’écriture dans la partition inactive.

L’écriture future devra utiliser la partition retournée par l’API OTA ESP-IDF et ne jamais coder `app0` ou `app1` en dur.

## 8. Identité de build

La cible courante ne doit pas être déduite uniquement du matériel, car Legacy et V4 ciblent la même carte.

`OtaBuildIdentity` fournit au minimum :

- version installée ;
- cible manifeste : `legacy` ou `v4` ;
- environnement PlatformIO exact ;
- identifiant matériel attendu ;
- emplacement du manifeste.

Cette identité doit être réutilisée dans les logs, `/ota`, le manifeste et les checkpoints.

## 9. Sécurité

`setInsecure()` est toléré uniquement pour les paliers actuels de lecture et de validation. Il ne doit pas être considéré comme acceptable pour une installation.

Avant `INSTALL_UPDATE`, implémenter et valider :

- validation TLS ;
- signature cryptographique ;
- clé publique embarquée ;
- rotation et révocation ;
- premier démarrage surveillé ;
- rollback matériel.

Ne jamais journaliser de secret, mot de passe Wi-Fi ou clé privée.

## 10. Validation minimale

Avant de déclarer un palier OTA validé :

```powershell
git diff --check
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

Pour upload et test matériel, demander le port courant et remplacer `<PORT_COM>` :

```powershell
pio run -e ProgrammeArrosage_v4 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200
```

Vérifier au minimum :

- fonctionnement nominal après upload ;
- refus avec arrosage actif ;
- succès réseau ;
- échec réseau borné ;
- données persistées cohérentes avec le série ;
- redémarrage normal après succès et échec ;
- partition active ;
- absence d’écriture non autorisée ;
- cible Legacy/V4 correcte.

Pour OTA-3.0, ajouter :

- firmware correct ;
- firmware tronqué ;
- mauvais SHA-256 ;
- taille incorrecte ;
- coupure Wi-Fi ;
- timeout de corps ;
- redirection refusée ;
- mesure de heap et du débit.

## 11. Documentation à mettre à jour à chaque palier

Chaque palier OTA doit mettre à jour, selon l’impact :

- le document d’architecture ;
- le document firmware ;
- ce guide développeur ;
- le contrat de manifeste ;
- le contrat d’installation ;
- les risques ou ADR si une décision difficile à inverser est prise ;
- le checkpoint exact de branche.

Aucune décision structurante ne doit rester uniquement dans le chat ou dans le checkpoint.
