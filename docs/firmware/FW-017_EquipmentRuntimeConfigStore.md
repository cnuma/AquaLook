# AquaLook Firmware — EquipmentRuntimeConfigStore

- Référence : FW-017
- Statut : relié au code
- Maturité : D3
- Sources : `src/EquipmentRuntimeConfig.*`, `src/EquipmentRuntimeConfigStore.*`

## Mission

`EquipmentRuntimeConfigStore` possède le chargement, la sauvegarde et la remise à zéro de la configuration runtime des équipements V4. Il fournit toujours une configuration exploitable ou des valeurs sûres explicites.

## API

- `begin()` initialise le stockage ;
- `load()` charge et valide la configuration persistée ;
- `save(config)` refuse une configuration invalide avant écriture ;
- `reset()` supprime la configuration active et restaure les valeurs sûres ;
- `config()`, `isLoaded()`, `usedSafeDefaults()` et `lastStatus()` exposent l'état.

## Politique de repli

`applySafeDefaults(status)` est utilisé lorsque le stockage est absent, illisible ou incompatible. Le repli doit désactiver les comportements non prouvés plutôt que reconstruire une configuration ambiguë.

## Invariants

- format versionné et validation avant usage ;
- aucune activation matérielle induite par une donnée corrompue ;
- statut de chargement observable ;
- valeurs sûres distinctes d'une configuration validée ;
- persistance V4 séparée des décisions métier du Scheduler.

## Validation

Tester première installation, lecture valide, version inconnue, données tronquées, valeurs hors bornes, sauvegarde puis redémarrage et reset. Vérifier que le repli ne commande aucun équipement.

## Références

- `docs/engineering/06_CONFIGURATION_AND_PERSISTENCE.md`
- `docs/engineering/16_V4_MODEL_AND_WEATHER.md`
- `docs/developer/DEV-012_Ajouter_une_donnee_persistante_et_sa_migration.md`
