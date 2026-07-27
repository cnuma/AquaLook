# AquaLook Firmware — EquipmentManager et modèle V4

- Référence : FW-010
- Statut : relié au code
- Maturité : D4
- Sources : `src/EquipmentModel.*`, `src/EquipmentManager.*`, adaptateurs Runtime

## Mission

Transformer la configuration logique des zones et équipements en plans d’exécution, puis déléguer les actions aux backends autorisés.

## Modèle

- `EquipmentConfig` décrit un équipement ;
- `ZoneEquipmentLink` relie une zone à ses équipements ;
- `EquipmentConfigSet` regroupe la configuration ;
- huit types d’équipements sont définis ;
- les dépendances sont résolues avant construction du plan.

## Exécution actuelle

- les électrovannes de zones sont exécutables ;
- les plans sont limités à quatre étapes ;
- les actions pompe sont disponibles en planification/dry-run mais pas encore pilotées physiquement ;
- un fallback legacy reste maintenu pendant la migration.

## Invariants

- une zone ne connaît pas l’adresse matérielle ;
- les index, types, liens et rôles relais sont validés ;
- une configuration invalide ne déclenche aucune sortie ;
- les backends restent sélectionnés explicitement à la compilation.

## Validation

- environnement `test_execution_engine` ;
- compilation V4 et legacy ;
- tests des dépendances absentes et index invalides ;
- validation matérielle pour toute nouvelle action physique.

## Références

- `docs/engineering/16_V4_EQUIPMENT_MODEL_AND_WEATHER.md`
- `docs/engineering/36_DETAILED_EQUIPMENT_MODEL_SCHEMA.md`
- `docs/developer/DEV-006_Ajouter_un_equipement.md`
