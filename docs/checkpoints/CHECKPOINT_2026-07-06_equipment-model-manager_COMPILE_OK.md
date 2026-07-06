# AquaLook — Checkpoint EquipmentModel / EquipmentManager

Date : 2026-07-06  
Branche : `feature/relay-board-mapping`  
Commit validé : `75fc817ad18971938627ab50dfc58112a97c57bc`

## État validé

- compilation PlatformIO réussie par l'utilisateur ;
- modèle `EquipmentModel` ajouté de façon isolée ;
- squelette `EquipmentManager` ajouté ;
- résolution logique disponible : zone -> équipement -> affectation relais -> carte/voie ;
- aucune commande matérielle encore effectuée par `EquipmentManager` ;
- comportement runtime existant inchangé ;
- NVS, `ConfigManager`, API Web et moteur d'arrosage non modifiés.

## Fichiers ajoutés

- `src/EquipmentModel.h`
- `src/EquipmentModel.cpp`
- `src/EquipmentManager.h`
- `src/EquipmentManager.cpp`

## Invariants

1. Une zone ne doit pas connaître directement l'adresse I2C ou la voie physique.
2. `RelaisManager` reste la couche d'exécution matérielle.
3. `EquipmentManager` porte l'orchestration métier future.
4. La persistance NVS reste inchangée tant que le modèle n'est pas stabilisé.
5. La compatibilité courante reste `Zone N -> carte 0 -> voie N`.

## Suite retenue

Raccorder proprement `EquipmentManager` à un exécuteur générique d'affectations relais, sans encore modifier `ScheduleManager` ni ajouter la dépendance pompe.
