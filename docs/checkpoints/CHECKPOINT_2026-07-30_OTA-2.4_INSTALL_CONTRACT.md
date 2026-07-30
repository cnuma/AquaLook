# CHECKPOINT — AquaLook — OTA-2.4 — Contrat d’installation

Date : 2026-07-30

## 1. Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche : `agent/ota-2.4-install-contract`
- Base : `main` après fusion de la PR #23
- Commit de base : `7164a822dd9929bc96a95695205e8f7adeccd34f`
- Dernier palier matériel validé : OTA-2.3 `CHECK_VERSION`
- Firmware matériel de référence : `ProgrammeArrosage_v4`

## 2. Objet du palier

OTA-2.4 fige le contrat préalable à toute installation OTA réelle.

Ce palier est exclusivement documentaire. Il ne modifie :

- aucun fichier source ;
- aucune ressource Web ;
- aucun workflow ;
- aucune configuration PlatformIO ;
- aucune partition ;
- aucun comportement runtime.

## 3. Décisions fixées

1. Séparation entre `CHECK_VERSION`, `DOWNLOAD_UPDATE_TEST`, `STAGE_UPDATE` et `INSTALL_UPDATE`.
2. OTA-3.0 téléchargera et vérifiera le firmware sans écriture flash.
3. OTA-3.1 pourra écrire uniquement la partition inactive, sans modifier la partition de démarrage.
4. L’activation nécessite une machine d’état persistante, un premier boot surveillé et un rollback.
5. Legacy et V4 restent des cibles distinctes et non interchangeables.
6. Le downgrade est interdit par défaut.
7. Le canal `stable` est accepté par défaut ; `beta` doit être explicitement activé ; `dev` reste réservé aux tests.
8. `setInsecure()` ne sera pas admis sur le chemin d’installation.
9. Une signature cryptographique est requise avant `INSTALL_UPDATE`.
10. LittleFS reste séparé des premiers paliers firmware OTA.

## 4. Machine d’état cible

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

## 5. Fichiers modifiés

### Nouveau

- `docs/ota/INSTALLATION_CONTRACT_V1.md`

### Mis à jour

- `docs/ota/GITHUB_MANIFEST_V1.md`
- `docs/engineering/OTA_REMOTE_UPDATE_ARCHITECTURE.md`
- `docs/developer/OTA_EXTENSION_GUIDE.md`

### Checkpoint

- `docs/checkpoints/CHECKPOINT_2026-07-30_OTA-2.4_INSTALL_CONTRACT.md`

## 6. Invariants préservés

- aucun changement Legacy ou V4 ;
- aucun changement EventLog ;
- aucun changement relais ;
- aucun changement de persistance active ;
- aucune écriture OTA ;
- aucune modification LittleFS ;
- aucun buildfs requis ;
- aucun test matériel revendiqué pour OTA-2.4.

## 7. Validation applicable

Le palier étant documentaire :

- vérifier l’absence de modification hors documentation ;
- vérifier la cohérence des liens et noms de commandes ;
- vérifier que le diff ne présente aucune altération du firmware ;
- aucune compilation ne peut être revendiquée comme exécutée par ce checkpoint.

## 8. Suite exacte

Le prochain palier est OTA-3.0 : `DOWNLOAD_UPDATE_TEST`.

Objectif :

- télécharger le firmware complet depuis la GitHub Release ;
- calculer SHA-256 en flux ;
- vérifier la taille exacte ;
- persister le résultat ;
- revenir au runtime normal ;
- garantir et afficher `otaWrite=no`.

Toujours interdit pendant OTA-3.0 :

```text
Update.begin()
Update.write()
Update.end()
esp_ota_begin()
esp_ota_write()
esp_ota_end()
esp_ota_set_boot_partition()
```

## 9. Reprise

Lire dans cet ordre :

1. `AGENTS.md` ;
2. le présent checkpoint ;
3. `docs/ota/INSTALLATION_CONTRACT_V1.md` ;
4. `docs/engineering/OTA_REMOTE_UPDATE_ARCHITECTURE.md` ;
5. `docs/ota/GITHUB_MANIFEST_V1.md` ;
6. `docs/developer/OTA_EXTENSION_GUIDE.md` ;
7. le code réel de maintenance sur la branche de développement OTA-3.0.
