# AquaLook Engineering Reference — Modèles de données et JSON

- Version documentaire : 1.0
- Statut : référence initiale
- Dernière consolidation : 2026-07-27
- Sources : checkpoints, interfaces Web, persistance et modèle V4
- Maturité : D2

## Objet

Ce document centralise les familles de données utilisées par AquaLook sans inventer de schéma absent du code.

## Familles de données

| Domaine | Propriétaire | Persistance | Exposition |
|---|---|---|---|
| configuration générale | ConfigManager | NVS | Web / diagnostic |
| programmes | Scheduler / ConfigManager | NVS | Web |
| équipements | backend V4 | NVS ou configuration validée | Runtime / Web |
| état d’exécution | Runtime | RAM | LCD / Web / EventLog |
| événements | EventLog | RAM, stockage selon disponibilité | diagnostic |
| ressources statiques | StorageManager | SD / LittleFS / firmware | HTTP |

## Règles de schéma

- chaque structure persistée possède une version ;
- les champs obligatoires, optionnels et valeurs par défaut sont documentés ;
- toute évolution incompatible possède une migration ou un repli ;
- les tailles et bornes sont validées avant application ;
- les secrets ne sont jamais inclus dans les réponses publiques ;
- un JSON reçu n’est jamais appliqué directement au matériel.

## Cycle d’une modification

```text
Réception -> parsing -> validation syntaxique -> validation métier -> autorisation -> persistance -> publication -> journalisation
```

## Formats JSON

Les schémas exacts doivent être extraits des sérialisations et désérialisations du commit ciblé. Les exemples historiques ne deviennent pas des contrats tant qu’ils ne correspondent pas au code courant.

Pour chaque format confirmé, documenter :

- nom et version ;
- producteur et consommateurs ;
- champs, types et unités ;
- limites de taille ;
- valeurs par défaut ;
- erreurs possibles ;
- compatibilité ascendante et descendante.

## Invariants

### INV-DATA-001

Une donnée persistée ou reçue du réseau est validée avant utilisation.

### INV-DATA-002

L’état Runtime volatil ne remplace pas la configuration persistante.

### INV-DATA-003

Une évolution de schéma est traçable et testée.

### INV-DATA-004

Les secrets sont exclus des exports, journaux et réponses non autorisées.

## Tests

- JSON valide et incomplet ;
- type incorrect ;
- taille excessive ;
- valeur hors plage ;
- champ inconnu ;
- migration depuis la version précédente ;
- redémarrage après sauvegarde ;
- rejet sans effet matériel.

## Références

- `07_CONFIGURATION_AND_PERSISTENCE.md` ;
- `09_WEB_AND_HTTP_INTERFACES.md` ;
- `16_V4_EQUIPMENT_MODEL_AND_WEATHER.md` ;
- `20_MQTT.md`.
