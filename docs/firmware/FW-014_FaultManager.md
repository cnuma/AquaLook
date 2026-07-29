# AquaLook Firmware — FaultManager

- Référence : FW-014
- Statut : relié au code
- Maturité : D4
- Sources : `src/FaultManager.h`, `src/FaultManager.cpp`

## Mission

`FaultManager` centralise les défauts actifs et l'état d'acquittement des erreurs afin de fournir une signalisation cohérente au Web, à l'affichage et aux diagnostics.

## Modèle

`FaultId` définit actuellement `RELAY_I2C`, `WIFI`, `FILESYSTEM`, `SOFTWARE` et `STORAGE_SD`. Chaque défaut occupe un bit de `_activeMask`.

## API

- `begin()` initialise l'état global ;
- `setActive(id, active)` active ou efface un bit ;
- `notifyError()` signale une erreur non acquittée ;
- `acknowledge()` acquitte la signalisation sans masquer les défauts encore actifs ;
- `hasActiveFaults()`, `hasUnacknowledgedErrors()`, `isAcknowledged()` et `activeMask()` exposent l'état ;
- `resolveColor()` adapte la couleur de signalisation selon l'état courant.

## Invariants

Un acquittement n'est pas une résolution. Le composant propriétaire de la panne reste responsable d'appeler `setActive(..., false)` lorsque la condition disparaît. Un nouveau défaut doit disposer d'un producteur réel, d'une condition de retour à la normale et d'une exposition diagnostique.

## Validation

Tester activation, répétition, coexistence de plusieurs bits, acquittement, résolution partielle et retour à l'état nominal. La validation matérielle dépend du producteur du défaut.

## Références

- `docs/engineering/24_DIAGNOSTICS_AND_OBSERVABILITY.md`
- `docs/engineering/25_MAINTENANCE_AND_RECOVERY.md`
- `docs/developer/DEV-014_Ajouter_un_defaut_FaultManager.md`
