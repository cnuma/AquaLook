# AquaLook V4 — Phase 6 — Run 6.18

## Objet

Poser un contrat de configuration générique pour les équipements pilotés par AquaLook, en commençant par la pompe partagée.

La pompe n'est pas traitée comme une exception figée : elle devient un équipement configurable avec un mode de contrôle, une affectation logique, des temporisations et des contraintes de fonctionnement.

## Modes de contrôle

- `DISABLED` : équipement déclaré inactif ; aucune dépendance ajoutée aux zones ; aucune commande physique.
- `SHADOW` : plans, temporisations et arbitrage actifs uniquement dans le moteur passif ; aucune commande physique.
- `PHYSICAL` : autorisable uniquement si une affectation relais valide de rôle `ROLE_PUMP` existe et correspond à l'index cible.

## Configuration pompe

`PumpRuntimeConfig` contient :

- `enabled` ;
- `mode` ;
- `targetIndex` ;
- `relayAssignmentIndex` ;
- `startupDelayMs` ;
- `shutdownDelayMs` ;
- `minOnSec` ;
- `minOffSec`.

## Valeurs sûres par défaut

La configuration produite par `makeSafeDefaultEquipmentRuntimeConfig()` est :

```text
pump.enabled = false
pump.mode = DISABLED
pump.relayAssignmentIndex = INVALID_INDEX
startupDelayMs = 500
shutdownDelayMs = 500
```

Un démarrage sans configuration persistée ne peut donc jamais activer une pompe.

## Validation

`validateEquipmentRuntimeConfig()` refuse notamment :

- une version de schéma inconnue ;
- un mode invalide ;
- des délais supérieurs à 30 secondes ;
- des durées minimales supérieures à 1 heure ;
- un mode physique sans affectation ;
- une affectation relais invalide ;
- une affectation dont le rôle n'est pas `ROLE_PUMP` ;
- une discordance d'index cible.

Le résultat sépare explicitement :

- `valid` : configuration structurellement acceptable ;
- `physicalActivationAllowed` : autorisation effective de commander le matériel.

## Application au modèle

`applyPumpConfigToModel()` applique la configuration au modèle d'équipements sans accéder au backend. En mode désactivé, il retire la dépendance pompe de toutes les zones. En mode actif, il peuple l'équipement pompe et relie les zones concernées.

## Périmètre volontaire du Run 6.18

Ce run ne modifie pas encore :

- la persistance NVS ;
- l'interface Web de paramétrage ;
- `ConfigManager` ;
- le chemin de commande physique ;
- le second relais de la carte connectée.

Il établit le contrat stable que la NVS et l'interface alimenteront dans les runs suivants.

## Orientation générale

La même approche doit ensuite couvrir les autres équipements : auxiliaires, éclairage, ventilation de serre, brumisation et futurs actionneurs. Les zones, les équipements, leurs dépendances et leurs affectations physiques doivent rester configurables séparément.
