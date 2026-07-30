# AquaLook — Guide développeur pour prolonger le chantier OTA

## 1. Lecture obligatoire

Avant toute modification OTA, lire dans cet ordre :

1. `AGENTS.md` ;
2. le dernier checkpoint de `docs/checkpoints/` pour la branche ;
3. `docs/engineering/OTA_REMOTE_UPDATE_ARCHITECTURE.md` ;
4. `docs/firmware/OTA_MAINTENANCE_RUNTIME.md` ;
5. `docs/ota/GITHUB_MANIFEST_V1.md` ;
6. les fichiers `MaintenanceRequest.*`, `MaintenanceSetupWrapper.cpp`, `MaintenanceBoot.*`, `MaintenanceResult.*` et `WebManager.h`.

Ne jamais reconstruire le mécanisme depuis un ancien extrait ou une conversation.

## 2. Ajouter une commande de maintenance

Pour ajouter une commande :

1. déclarer la valeur dans `MaintenanceRequest` sans renuméroter les valeurs existantes ;
2. compléter `MaintenanceRequestStore::name()` et la validation des valeurs ;
3. ajouter une route Web distincte ;
4. vérifier l'absence d'arrosage actif avant l'enregistrement ;
5. répondre au client avant de programmer le redémarrage ;
6. traiter la commande dans `MaintenanceBoot::runIfRequested()` ;
7. effacer la commande avant son exécution ;
8. borner tous les délais, tailles et allocations ;
9. persister un résultat exploitable ;
10. garantir le redémarrage vers le runtime normal sur tous les chemins.

Une commande inconnue ou non implémentée doit être refusée sans lancer d'opération réseau ou OTA.

## 3. Étendre le résultat persistant

`MaintenanceResult` constitue le contrat entre la maintenance minimale et le runtime normal.

Lors d'une extension :

- ajouter uniquement les champs nécessaires ;
- conserver des tailles de chaînes bornées ;
- éviter les allocations dynamiques persistantes ;
- conserver la compatibilité avec un résultat ancien absent ou partiel ;
- documenter chaque nouvelle clé NVS ;
- vérifier le comportement lorsque `putString()` reçoit une chaîne vide ;
- ne pas transformer ce stockage en journal historique illimité.

Pour OTA-2.3, il faudra probablement ajouter un type de résultat ou des champs dédiés au manifeste plutôt que détourner les champs du probe.

## 4. Implémenter CHECK_VERSION

`CHECK_VERSION` doit rester une opération de lecture et de validation.

Séquence recommandée :

1. identifier explicitement la cible compilée ;
2. connecter le Wi-Fi en maintenance minimale ;
3. ouvrir TLS vers une URL GitHub autorisée ;
4. vérifier la ligne HTTP et les en-têtes utiles ;
5. plafonner la taille reçue à 8 Kio ;
6. lire dans un tampon borné ou en flux contrôlé ;
7. parser avec un document JSON dimensionné ;
8. vérifier `schema` ;
9. sélectionner `legacy` ou `v4` ;
10. vérifier `board`, `environment`, `firmwareUrl`, `size` et `sha256` ;
11. comparer la version disponible à la version installée ;
12. persister le résultat ;
13. fermer les ressources ;
14. redémarrer normalement.

## 5. Identité de build

La cible courante ne doit pas être déduite uniquement du matériel, car Legacy et V4 ciblent la même carte.

Créer une source unique compile-time fournissant au minimum :

- version installée ;
- cible manifeste : `legacy` ou `v4` ;
- environnement PlatformIO exact ;
- identifiant matériel attendu.

Cette identité doit être réutilisée dans les logs, `/ota`, le manifeste et les checkpoints.

## 6. Interdictions jusqu'à validation INSTALL_UPDATE

Pendant `CHECK_VERSION`, il est interdit d'introduire ou d'appeler :

```text
Update.begin()
Update.write()
esp_ota_begin()
esp_ota_write()
```

Il est également interdit de :

- télécharger le binaire firmware ;
- ouvrir `app1` en écriture ;
- modifier `otadata` ;
- sélectionner une nouvelle partition de démarrage ;
- considérer le seul code HTTP comme une validation suffisante.

## 7. Validation minimale

Avant de déclarer un palier OTA validé :

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage_v4
```

Upload V4 sur le module de référence :

```powershell
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
```

Connexion série :

```powershell
pio device monitor -p COM3 -b 115200
```

Upload puis moniteur :

```powershell
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3 ; pio device monitor -p COM3 -b 115200
```

Vérifier au minimum :

- fonctionnement nominal après upload ;
- refus avec arrosage actif ;
- succès réseau ;
- échec réseau borné ;
- données persistées cohérentes avec le série ;
- redémarrage normal après succès et échec ;
- partition active et absence d'écriture dans `app1`.

## 8. Documentation à mettre à jour à chaque palier

Chaque palier OTA doit mettre à jour, selon l'impact :

- le document d'architecture ;
- le document firmware ;
- ce guide développeur ;
- le contrat de manifeste ;
- les risques ou ADR si une décision difficile à inverser est prise ;
- le checkpoint exact de branche.

Aucune décision structurante ne doit rester uniquement dans le chat ou dans le checkpoint.
