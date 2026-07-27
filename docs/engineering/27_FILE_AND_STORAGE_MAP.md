# AquaLook Engineering Reference — Cartographie des fichiers et stockages

- Version documentaire : 1.0
- Statut : référence initiale
- Dernière consolidation : 2026-07-27
- Sources : AGENTS.md, checkpoints NVS/LittleFS/SD, dépôt Git
- Maturité : D3

## Objet

Ce document indique où résident les différentes catégories de données et quelles règles s’appliquent à chaque support.

## Cartographie

| Emplacement | Contenu | Autorité | Contraintes |
|---|---|---|---|
| Flash firmware | code et fallbacks embarqués | build publié | modifié uniquement par mise à jour firmware |
| NVS | configuration active et paramètres critiques | ConfigManager | schéma versionné, migration obligatoire |
| LittleFS | ressources Web et splash historiques | ConfigManager pour le montage | lecture seule en fonctionnement normal |
| microSD | ressources volumineuses, exports, récupération | StorageManager | support optionnel et non fiable par défaut |
| RAM | état Runtime, caches, files temporaires | composant propriétaire | volatile, bornée, non persistante |
| `data/` dans Git | image LittleFS à construire | dépôt Git | aucune sauvegarde, documentation ou fichier parasite |
| `docs/` dans Git | référentiel, checkpoints et décisions | dépôt Git | aucune donnée secrète |

## Ordre de résolution des ressources Web

```text
microSD -> LittleFS -> fallback firmware lorsqu’il existe
```

L’ordre réel de chaque ressource doit être confirmé dans le gestionnaire correspondant.

## Propriété des opérations

- `ConfigManager` possède le montage LittleFS et la configuration NVS ;
- `StorageManager` possède les opérations sur la microSD ;
- le serveur Web consomme les ressources via les gestionnaires prévus ;
- aucun composant métier ne manipule directement les fichiers de ressources.

## Règles d’écriture

- limiter les écritures répétitives en Flash ;
- écrire de façon atomique ou avec repli lorsque possible ;
- valider avant remplacement ;
- journaliser les échecs sans exposer les secrets ;
- ne jamais rendre la carte SD obligatoire pour l’arrosage local essentiel.

## Construction LittleFS

Après toute modification de `data/` :

```powershell
pio run -e ProgrammeArrosage -t buildfs
```

L’environnement exact applicable doit être vérifié dans `platformio.ini` et le checkpoint courant.

## Modes dégradés

- SD absente : fallback LittleFS ou firmware ;
- LittleFS indisponible : service minimal et diagnostic ;
- NVS invalide : valeurs sûres, migration ou mode de récupération ;
- stockage plein : rejet contrôlé, rotation ou purge documentée.

## Invariants

### INV-STO-001

Chaque donnée possède un propriétaire unique.

### INV-STO-002

Une ressource Web n’est pas une donnée métier critique.

### INV-STO-003

Une absence de SD ne bloque pas le fonctionnement essentiel.

### INV-STO-004

Aucun secret n’est stocké dans une ressource Web publique ou un checkpoint.

## Références

- `07_CONFIGURATION_AND_PERSISTENCE.md` ;
- `14_SD_AND_STATIC_RESOURCES.md` ;
- `25_BACKUP_RESTORE_AND_MAINTENANCE.md` ;
- `AGENTS.md`.
