# AquaLook Firmware — RelaisManager et RelayTopology

- Référence : FW-011
- Statut : relié au code
- Maturité : D4
- Sources : `src/RelaisManager.h`, `src/RelaisManager.cpp`, `src/RelayTopology.*`

## Mission

`RelaisManager` traduit une demande logique de zone ou d'affectation en écriture I²C. `RelayTopology` décrit les cartes, contrôleurs, canaux, rôles et correspondances entre zones et sorties physiques.

## Initialisation

`begin(ConfigManager*)` construit une topologie compatible avec la configuration historique, initialise les états logiques et registres miroirs, initialise chaque carte valide puis publie le défaut `FaultId::RELAY_I2C` selon le résultat.

La topologie courante conserve les contrôleurs XL9535 et MCP23017 ainsi que les logiques directe et inversée. Une configuration invalide ou un doublon de mapping est journalisé.

## Commandes

- `setRelay(zone, state)` conserve l'API historique par index de zone ;
- `findZoneAssignment()` cherche l'affectation `ROLE_ZONE_VALVE` correspondante ;
- `setAssignment()` résout le mapping, calcule le niveau physique, met à jour les registres miroirs puis appelle `applyBoard()` ;
- `getState()` et `getAssignmentState()` exposent les états mémorisés ;
- `topology()` expose la topologie runtime en lecture.

Pour XL9535, `Xl9535SharedOutputState` évite d'écraser des sorties partagées avec d'autres fonctions.

## Sécurités et défauts

`update()` coupe une zone restée active au-delà de la durée maximale configurée. Une carte absente, une écriture I²C impossible ou un état partagé indisponible active `RELAY_I2C` et rend l'erreur observable.

## Limites de validation

La compilation ne valide ni le câblage, ni l'adresse I²C, ni la logique électrique. Chaque contrôleur et chaque variante directe/inversée doivent être testés sur matériel avec une seule zone et une durée courte.

## Références

- `docs/engineering/08_RELAY_AND_EQUIPMENT_CONTROL.md`
- `docs/engineering/17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md`
- `docs/firmware/FW-010_EquipmentManager.md`
- `docs/developer/DEV-010_Ajouter_un_backend_materiel.md`
