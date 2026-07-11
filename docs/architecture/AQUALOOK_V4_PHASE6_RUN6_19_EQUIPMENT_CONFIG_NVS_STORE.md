# AquaLook V4 — Phase 6 — Run 6.19

## Objet

Rendre la configuration des équipements persistante sans modifier le bloc NVS historique du planning et de l’interface.

## Choix d’architecture

La configuration d’équipements utilise une clé NVS dédiée dans le namespace `aqualook` :

```text
equipCfg
```

Le bloc possède :

- un magic ;
- un numéro de schema ;
- une taille de payload ;
- la structure `EquipmentRuntimeConfig` ;
- un CRC32.

Ce découplage évite une migration risquée du bloc `config` existant et permet de faire évoluer indépendamment les pompes, éclairages, contacts auxiliaires, ventilations et futurs équipements.

## Comportement sûr

En cas de clé absente, taille incorrecte, CRC invalide, schema inconnu ou contenu incohérent :

- la pompe est désactivée ;
- le mode devient `DISABLED` ;
- aucune affectation relais n’est retenue ;
- les valeurs sûres sont réécrites dans `equipCfg` ;
- aucune activation physique n’est possible implicitement.

## API

`EquipmentRuntimeConfigStore` expose :

- `begin()` ;
- `load()` ;
- `save()` ;
- `reset()` ;
- `config()` ;
- `isLoaded()` ;
- `usedSafeDefaults()` ;
- `lastStatus()`.

## Périmètre volontaire

Ce run ajoute la persistance mais ne branche pas encore la configuration sur le moteur shadow, le Web ou les relais physiques. Le comportement embarqué courant reste inchangé jusqu’au Run suivant.
