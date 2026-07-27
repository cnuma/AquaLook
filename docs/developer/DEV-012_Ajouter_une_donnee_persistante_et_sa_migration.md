# AquaLook Developer Guide — Ajouter une donnée persistante et sa migration

- Référence : DEV-012
- Statut : actif
- Maturité : D4

## Principe

`ConfigManager` reste propriétaire de la configuration persistante. Une nouvelle donnée NVS doit avoir une valeur par défaut sûre, une validation, une version de schéma et une stratégie de migration ou de repli.

## Étapes

1. identifier propriétaire, type, unité, bornes et valeur par défaut ;
2. ajouter le champ au modèle de configuration concerné ;
3. ajouter lecture et écriture NVS avec une clé stable et courte ;
4. valider avant application au runtime ;
5. incrémenter la version de schéma lorsque le format change ;
6. écrire une migration idempotente depuis les versions supportées ;
7. définir le comportement si la migration échoue ;
8. exposer la donnée sans révéler de secret ;
9. tester configuration absente, ancienne, invalide et courante ;
10. mettre à jour Firmware, Engineering et checkpoint.

## Interdictions

- monter LittleFS depuis un autre manager ;
- modifier silencieusement la signification d'une clé existante ;
- accepter une valeur hors bornes parce qu'elle vient de NVS ;
- supprimer une compatibilité sans décision documentée.

## Références

- `docs/firmware/FW-003_ConfigManager.md`
- `docs/engineering/05_CONFIGURATION_AND_PERSISTENCE.md`
- `docs/engineering/29_DATA_MODELS.md`
