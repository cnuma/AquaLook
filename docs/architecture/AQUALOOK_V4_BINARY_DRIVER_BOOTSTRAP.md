# AquaLook V4 — Bootstrap du registre de drivers binaires

**Run :** Phase 3 — Run 3.6  
**Date :** 8 juillet 2026

## Objectif

Consolider les drivers disponibles sans les raccorder au firmware actif.

## Fichiers

```text
src/domain/BinaryActuatorDriverBootstrap.h
src/domain/BinaryActuatorDriverBootstrap.cpp
```

## Chaîne préparée

```text
contexts fournis explicitement
-> BinaryActuatorDriverBootstrapPlan
-> bootstrapBinaryActuatorDrivers()
-> BinaryActuatorDriverRegistry
```

## Drivers supportés par le plan

```text
simulated
gpio
xl9535
```

Les drivers GPIO et XL9535 sont conditionnés par les macros de profil existantes.

## Capacité par défaut

Le helper expose :

```text
defaultBinaryActuatorDriverCapacity()
compiledBinaryActuatorDriverMask()
```

Ces fonctions permettent de dimensionner un stockage externe sans allocation dynamique.

## Refus explicites

Le bootstrap refuse :

```text
contexte manquant
driver incomplet
doublon
capacité insuffisante
```

## Non-runtime

Aucun appel n’est ajouté au firmware actif. Les drivers restent compilables et disponibles, mais non utilisés.

## Validation hôte

```text
Compilation hôte OK
registered=3 requested=3 failures-ok
```

## Suite

La suite logique consiste à compiler PlatformIO, puis à décider si l’on introduit un point d’instanciation expérimental désactivé par défaut ou si l’on prépare d’abord la stratégie NVS/runtime.
