# AquaLook — Manifeste GitHub OTA v1

## Statut

Contrat préparatoire pour la future commande `CHECK_VERSION`.

À ce stade :

- le manifeste est défini et versionné ;
- `CHECK_VERSION` reste non exécutable ;
- `INSTALL_UPDATE` reste désactivé ;
- aucune écriture dans `app1` n'est autorisée.

## Emplacement prévu

Le manifeste stable devra être publié dans une GitHub Release AquaLook et téléchargé en HTTPS par le mode maintenance minimal.

Nom recommandé :

```text
aqualook-manifest.json
```

## Format JSON v1

```json
{
  "schema": "aqualook-ota-manifest-v1",
  "release": {
    "version": "4.0.0",
    "channel": "stable",
    "publishedAt": "2026-07-23T10:00:00Z",
    "notesUrl": "https://github.com/cnuma/AquaLook/releases/tag/v4.0.0"
  },
  "targets": {
    "legacy": {
      "board": "esp32-2432S028",
      "environment": "ProgrammeArrosage",
      "firmwareUrl": "https://github.com/cnuma/AquaLook/releases/download/v4.0.0/ProgrammeArrosage.bin",
      "size": 1331200,
      "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    },
    "v4": {
      "board": "esp32-2432S028",
      "environment": "ProgrammeArrosage_v4",
      "firmwareUrl": "https://github.com/cnuma/AquaLook/releases/download/v4.0.0/ProgrammeArrosage_v4.bin",
      "size": 1343488,
      "sha256": "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789"
    }
  }
}
```

Les valeurs ci-dessus sont des exemples de structure, pas des binaires publiables.

## Champs obligatoires

### Racine

- `schema` : doit être exactement `aqualook-ota-manifest-v1` ;
- `release` : métadonnées communes de la version ;
- `targets` : description des firmwares autorisés.

### `release`

- `version` : version sémantique sans préfixe `v` ;
- `channel` : `stable`, `beta` ou `dev` ;
- `publishedAt` : date ISO 8601 UTC ;
- `notesUrl` : URL informative de la release.

### Chaque cible

- `board` : identifiant matériel attendu ;
- `environment` : environnement PlatformIO exact ;
- `firmwareUrl` : URL HTTPS GitHub du binaire ;
- `size` : taille exacte du binaire en octets ;
- `sha256` : empreinte SHA-256 en 64 caractères hexadécimaux minuscules.

## Règles de validation futures pour `CHECK_VERSION`

La commande devra uniquement lire et valider le manifeste. Elle ne devra jamais ouvrir la partition OTA de destination.

Le manifeste devra être rejeté si :

1. `schema` est absent ou différent ;
2. la version n'est pas exploitable ;
3. la cible du firmware courant est absente ;
4. `board` ou `environment` ne correspondent pas au firmware courant ;
5. l'URL n'est pas HTTPS ou ne pointe pas vers GitHub ;
6. `size` vaut zéro ou dépasse la taille utile de la partition OTA ;
7. `sha256` n'est pas une chaîne hexadécimale de 64 caractères ;
8. le JSON est incomplet, trop volumineux ou contient des types inattendus.

## Séparation des responsabilités

### `CHECK_VERSION`

Autorisé ultérieurement à :

- connecter le Wi-Fi en mode maintenance minimal ;
- télécharger le manifeste avec une taille plafonnée ;
- parser le JSON ;
- comparer la version publiée avec la version installée ;
- persister et afficher le résultat.

Interdit à :

- télécharger le firmware ;
- appeler `Update.begin()` ;
- sélectionner ou écrire `app1` ;
- modifier la partition de démarrage.

### `INSTALL_UPDATE`

Doit rester désactivé jusqu'à validation indépendante de :

- la sélection fiable de la cible ;
- la taille du binaire ;
- son téléchargement en flux ;
- son SHA-256 calculé pendant le flux ;
- l'écriture contrôlée de la partition inactive ;
- la stratégie de rollback matériel.

## Limites de sécurité proposées

- taille maximale du manifeste : 8 KiB ;
- délai TLS : 10 s ;
- délai de réponse : 10 s ;
- profondeur JSON limitée par le document statique choisi ;
- aucune redirection vers un domaine non GitHub ;
- aucune confiance accordée au seul code HTTP ;
- validation complète avant toute future écriture OTA.
