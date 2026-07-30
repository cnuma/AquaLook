# AquaLook Firmware — EquipmentExecutionEngine et ShadowRuntime

- Référence : FW-016
- Statut : implémenté en observation passive
- Maturité : D3
- Sources : `src/EquipmentExecutionEngine.*`, `src/EquipmentExecutionShadowRuntime.*`

## Mission

Le moteur d'exécution déroule un `ZoneExecutionPlan` par étapes bornées. `EquipmentExecutionShadowRuntime` maintient un moteur passif par zone pour comparer la logique V4 au fonctionnement actif sans commander le matériel.

## Fonctionnement

- `begin(nbZones)` initialise les slots de zone ;
- `submit(...)` transmet un plan de démarrage ou d'arrêt ;
- `update(nowMs)` fait progresser les moteurs sans attente bloquante ;
- l'arbitrage conserve une seule transition de pompe partagée ;
- `emergencyStopAll()` force l'arrêt logique des scénarios observés ;
- les compteurs et états permettent de détecter une incohérence.

## Frontière de sécurité

Le ShadowRuntime ne possède aucun backend matériel. Il observe les plans et journalise leur progression ; il ne constitue donc pas une preuve de fonctionnement électrique V4.

## Invariants

- un moteur distinct par zone ;
- capacités statiques bornées par `MAX_ZONES` ;
- IDs non nuls et renouvelés ;
- comptage cohérent des utilisateurs de pompe partagée ;
- aucune commande physique depuis le mode shadow.

## Validation

Tester démarrage, arrêt, recouvrement de zones, pompe partagée, arrêt d'urgence, plan invalide et réparation de cohérence. Comparer les traces shadow aux transitions legacy observées.

## Références

- `docs/engineering/16_V4_MODEL_AND_WEATHER.md`
- `docs/firmware/FW-010_EquipmentManager.md`
- `docs/developer/DEV-006_Ajouter_un_equipement.md`
