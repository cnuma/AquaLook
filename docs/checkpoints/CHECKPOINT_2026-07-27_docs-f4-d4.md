# Checkpoint documentaire — Firmware F4 + Developer D4

- Dépôt : `cnuma/AquaLook`
- Branche de travail : `agent/docs-firmware-f4-developer-d4`
- Base : `main` au commit `817bfb6e1a98e0ef14ae017f0f858402e8c2c8eb`
- Nature : documentation uniquement

## Contenu

Le lot F4 décrit le chemin d'exécution V4 : `V4PilotRuntime`, moteur et runtime shadow, configuration runtime persistée, adaptateurs/backends et état partagé XL9535.

Le lot D4 explique comment qualifier une fonction legacy en V4, ajouter un scénario shadow, étendre la configuration runtime, intégrer un driver binaire et partager un expander sans écraser les sorties.

## Validation

- aucun fichier firmware modifié ;
- aucune dette de sécurité déclarée corrigée ;
- aucune validation matérielle revendiquée ;
- variantes legacy et V4 conservées ;
- index Firmware et Developer mis à jour.

## Reprise

Repartir de la PR associée à cette branche, vérifier la CI, puis fusionner dans `main` si le diff reste strictement documentaire et sans conflit.
