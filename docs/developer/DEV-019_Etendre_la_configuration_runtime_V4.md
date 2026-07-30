# AquaLook Developer Guide — Étendre la configuration runtime V4

- Référence : DEV-019
- Statut : actif
- Maturité : D4

## Étapes

1. définir le propriétaire, le type, les bornes et la valeur sûre ;
2. ajouter le champ dans `EquipmentRuntimeConfig` ;
3. incrémenter la version de format lorsque la représentation persistée change ;
4. compléter validation, égalité et valeurs par défaut ;
5. adapter `EquipmentRuntimeConfigStore::load()` et `save()` ;
6. prévoir migration, rejet ou reset explicite des anciennes versions ;
7. exposer le statut sans publier de secret ;
8. tester données absentes, anciennes, tronquées et hors bornes ;
9. vérifier qu'un repli sûr n'active aucune sortie ;
10. documenter le contrat et la procédure de retour arrière.

## Règles

La configuration persistée ne doit pas contenir de pointeur, d'état transitoire ou de décision Scheduler. Une donnée inconnue ou invalide entraîne un repli explicite, jamais une interprétation opportuniste.

## Références

- `docs/firmware/FW-017_EquipmentRuntimeConfigStore.md`
- `docs/developer/DEV-012_Ajouter_une_donnee_persistante_et_sa_migration.md`
- `docs/engineering/06_CONFIGURATION_AND_PERSISTENCE.md`
