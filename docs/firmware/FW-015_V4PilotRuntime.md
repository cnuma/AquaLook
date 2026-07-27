# AquaLook Firmware — V4PilotRuntime

- Référence : FW-015
- Statut : relié au code, qualification matérielle incomplète
- Maturité : D3
- Sources : `src/V4PilotRuntime.*`, `src/V4RelayPhysicalBackend.*`, `src/domain/`

## Mission

`V4PilotRuntime` assemble le chemin physique V4 à partir de la topologie relais validée. Il construit l'inventaire contrôleur/carte/ports, enregistre le driver binaire XL9535 et configure `V4RelayPhysicalBackend`.

## Cycle de vie

1. `begin(topology, sharedOutputState)` reçoit la topologie réelle et l'état partagé XL9535 ;
2. les définitions contrôleur, carte et ports sont construites dans des tableaux statiques ;
3. le driver XL9535 est lié au registre de drivers ;
4. le backend physique est initialisé ;
5. `isReady()` indique si l'assemblage est exploitable.

## Limites actuelles

Les capacités sont bornées à un contrôleur, une carte, seize ports et un binding de driver. Une compilation V4 réussie ne prouve ni le câblage complet du chemin d'exécution, ni son effet matériel.

## Invariants

- aucune allocation dynamique requise pour l'inventaire ;
- topologie validée avant commande ;
- état XL9535 partagé avec le chemin legacy ;
- aucune activation implicite au démarrage ;
- legacy reste disponible comme référence et repli.

## Validation

Tester la construction avec topologie valide/invalide, port hors plage, driver absent et état partagé indisponible. La validation finale exige une commande observable sur carte avec le profil V4 réellement flashé.

## Références

- `docs/engineering/08_RELAY_AND_EQUIPMENT_CONTROL.md`
- `docs/engineering/16_V4_MODEL_AND_WEATHER.md`
- `docs/firmware/FW-010_EquipmentManager.md`
- `docs/firmware/FW-011_RelaisManager_et_RelayTopology.md`
