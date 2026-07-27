# AquaLook Firmware — Adaptateurs, backends et état partagé XL9535

- Référence : FW-018
- Statut : chemin hybride implémenté, validation matérielle partielle
- Maturité : D3
- Sources : `src/EquipmentOutputRuntimeAdapter.*`, `src/RelaisManagerBackend.*`, `src/V4RelayPhysicalBackend.*`, `src/domain/Xl9535SharedOutputState.*`

## Mission

Cette chaîne traduit une action d'équipement validée en commande de sortie tout en permettant la coexistence des chemins legacy et V4 sur le même expander XL9535.

## Responsabilités

- `EquipmentOutputRuntimeAdapter` relie les plans d'équipements aux backends disponibles ;
- `RelaisManagerBackend` adapte l'API historique de `RelaisManager` ;
- `V4RelayPhysicalBackend` résout inventaire, port et driver V4 ;
- `Xl9535SharedOutputState` conserve une image commune des seize sorties par adresse I2C afin d'éviter qu'un chemin écrase les bits pilotés par l'autre.

## Invariants

- un seul état logique partagé par adresse XL9535 ;
- mise à jour d'un canal sans altération des autres bits ;
- rôle, cible, carte et port validés avant accès matériel ;
- erreur de backend remontée sans simuler un succès ;
- aucune suppression du backend legacy pendant la qualification V4.

## Risques

Une initialisation concurrente, une adresse dupliquée ou une image de registre non amorcée peut provoquer une perte d'état. La preuve requiert l'observation des sorties et des registres sur la carte réelle.

## Validation

Tester commandes alternées legacy/V4 sur canaux distincts, ON/OFF répétés, adresse inconnue, canal hors plage, perte I2C et redémarrage. Vérifier qu'aucune sortie voisine ne change.

## Références

- `docs/engineering/08_RELAY_AND_EQUIPMENT_CONTROL.md`
- `docs/engineering/16_V4_MODEL_AND_WEATHER.md`
- `docs/firmware/FW-011_RelaisManager_et_RelayTopology.md`
