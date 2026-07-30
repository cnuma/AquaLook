# AquaLook — Manifeste GitHub OTA v1

## Statut

Contrat actif pour `CHECK_VERSION`, préparatoire aux futurs paliers de téléchargement et d’installation.

État au palier OTA-2.4 :

- le manifeste est généré et publié dans les GitHub Releases ;
- `CHECK_VERSION` est exécutable et validé sur matériel V4 ;
- la cible courante, la taille, l’URL et le SHA-256 sont validés ;
- `INSTALL_UPDATE` reste désactivé ;
- aucune écriture dans la partition OTA inactive n’est autorisée ;
- les extensions nécessaires à une installation industrialisable sont définies dans `docs/ota/INSTALLATION_CONTRACT_V1.md`.

## Emplacement

Le manifeste stable est publié dans une GitHub Release AquaLook et téléchargé en HTTPS par le mode maintenance minimal.

Nom :

```text
aqualook-manifest.json
```

## Format JSON v1 actuellement produit

```json
{
  "schema": "aqualook-ota-manifest-v1",
  "release": {
    "version": "5.9.0",
    "channel": "stable",
    "publishedAt": "2026-07-29T10:00:00Z",
    "notesUrl": "https://github.com/cnuma/AquaLook/releases/tag/v5.9.0"
  },
  "targets": {
    "legacy": {
      "board": "esp32-2432S028",
      "environment": "ProgrammeArrosage",
      "firmwareUrl": "https://github.com/cnuma/AquaLook/releases/download/v5.9.0/AquaLook-legacy-5.9.0.bin",
      "size": 1331200,
      "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    },
    "v4": {
      "board": "esp32-2432S028",
      "environment": "ProgrammeArrosage_v4",
      "firmwareUrl": "https://github.com/cnuma/AquaLook/releases/download/v5.9.0/AquaLook-v4-5.9.0.bin",
      "size": 1343488,
      "sha256": "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"
    }
  }
}
```

Les tailles et empreintes ci-dessus sont des exemples de structure.

## Champs obligatoires actuels

### Racine

- `schema` : exactement `aqualook-ota-manifest-v1` ;
- `release` : métadonnées communes ;
- `targets` : firmwares autorisés par cible.

### `release`

- `version` : version `MAJEUR.MINEUR.CORRECTIF`, sans préfixe `v` ;
- `channel` : `stable`, `beta` ou `dev` ;
- `publishedAt` : date ISO 8601 UTC ;
- `notesUrl` : URL informative de la release.

### Chaque cible

- `board` : identifiant matériel attendu ;
- `environment` : environnement PlatformIO exact ;
- `firmwareUrl` : URL HTTPS GitHub du binaire ;
- `size` : taille exacte du binaire en octets ;
- `sha256` : empreinte SHA-256 en 64 caractères hexadécimaux minuscules.

## Validation actuellement implémentée par `CHECK_VERSION`

Le manifeste est rejeté si :

1. `schema` est absent ou différent ;
2. la version n’est pas au format attendu ;
3. le canal n’est pas reconnu ;
4. la cible du firmware courant est absente ;
5. `board` ou `environment` ne correspondent pas au firmware courant ;
6. l’URL n’est pas HTTPS ou ne pointe pas vers un hôte GitHub autorisé ;
7. `size` vaut zéro ou dépasse la taille de la partition OTA disponible ;
8. `sha256` n’est pas une chaîne hexadécimale minuscule de 64 caractères ;
9. le JSON est invalide, vide ou dépasse 8 Kio ;
10. les redirections dépassent la limite ou quittent les hôtes autorisés.

`CHECK_VERSION` compare ensuite la version distante à la version installée et persiste :

- `update-available` ;
- `version-identical` ;
- `remote-version-older`.

## Champs d’extension requis avant `INSTALL_UPDATE`

Le schéma v1 doit être étendu de manière rétrocompatible, ou remplacé par un schéma v2, avant toute activation de l’installation. Les informations suivantes deviennent obligatoires pour le chemin d’installation :

### Compatibilité de la release

- `partitionScheme` : identifiant exact du layout de partitions attendu ;
- `minimumInstalledVersion` : version minimale autorisée pour une installation directe ;
- `minimumOtaContractVersion` : version minimale du mécanisme OTA ;
- `downgradePolicy` : `forbid` par défaut ;
- `deployable` : booléen explicite autorisant le déploiement ;
- `keyId` : identifiant de la clé de signature utilisée.

### Authenticité

- `signatureAlgorithm` : algorithme retenu ;
- `manifestSignature` ou signature détachée équivalente ;
- définition canonique des octets signés.

### LittleFS, uniquement lorsqu’il sera activé

- artefact distinct ;
- taille ;
- SHA-256 ;
- version ou schéma de ressources ;
- matrice de compatibilité avec le firmware.

L’ajout de ces champs doit être accompagné d’une mise à jour coordonnée du générateur de manifeste, du workflow de release, du parseur embarqué et de la documentation.

## Séparation des responsabilités

### `CHECK_VERSION`

Autorisé à :

- connecter le Wi-Fi en maintenance minimale ;
- télécharger le manifeste avec une taille plafonnée ;
- parser le JSON ;
- sélectionner `legacy` ou `v4` ;
- valider les métadonnées actuelles ;
- comparer les versions ;
- persister et afficher le résultat.

Interdit à :

- télécharger le firmware ;
- appeler `Update.begin()` ou une primitive ESP-IDF d’écriture OTA ;
- ouvrir une partition OTA en écriture ;
- modifier `otadata` ;
- changer la partition de démarrage.

### `DOWNLOAD_UPDATE_TEST`

Futur palier OTA-3.0 défini par `INSTALLATION_CONTRACT_V1.md`. Cette commande devra télécharger et vérifier le firmware sans aucune écriture flash et conserver `otaWrite=no`.

### `INSTALL_UPDATE`

Reste désactivé jusqu’à validation indépendante de :

- compatibilité complète ;
- authentification du manifeste ;
- téléchargement en flux ;
- SHA-256 calculé localement ;
- écriture contrôlée de la partition inactive ;
- validation de l’image écrite ;
- premier démarrage surveillé ;
- confirmation de démarrage sain ;
- rollback matériel.

## Limites de sécurité actuelles

- taille maximale du manifeste : 8 Kio ;
- délai TLS : 10 s ;
- délai de réponse : 10 s ;
- deux redirections maximum ;
- aucune redirection vers un domaine non GitHub ;
- aucune confiance accordée au seul code HTTP ;
- `setInsecure()` reste toléré uniquement pour les paliers de lecture actuels et doit être supprimé du futur chemin d’installation ;
- aucune écriture OTA avant validation complète du contrat d’installation.
