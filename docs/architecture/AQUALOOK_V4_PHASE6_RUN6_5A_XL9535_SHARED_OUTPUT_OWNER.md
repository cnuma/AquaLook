# AquaLook V4 — Phase 6 — Run 6.5A

## Arbitrage de propriété XL9535 et registre de sortie partagé

**Date :** 10 juillet 2026  
**Statut :** premier palier réalisé, compilation à valider

## Problème traité

Le backend historique `RelaisManager` maintient une image `_regP0/_regP1` par carte tandis que le driver V4 `Xl9535BinaryActuatorDriver` maintient son propre `outputLatch`.

Deux images indépendantes du même registre physique rendent dangereuse une migration voie par voie : chaque écriture XL9535 réécrit le registre complet et peut écraser les autres sorties.

## Décision d’architecture

Une carte XL9535 doit disposer d’un propriétaire logique unique de son image de sortie.

Le composant ajouté est :

```text
AquaLook::Domain::Xl9535SharedOutputState
```

Il conserve une image 16 bits par adresse I2C et expose :

```text
seed(address, value)
read(address, value)
updateChannel(address, channel, high, value)
```

La modification d’une voie retourne toujours l’image complète résultante. Les futurs consommateurs écrivent ensuite cette image au composant physique.

## Fichiers ajoutés

```text
src/domain/Xl9535SharedOutputState.h
src/domain/Xl9535SharedOutputState.cpp
```

## Fichier modifié

```text
src/domain/Xl9535BinaryActuatorDriver.h
```

Le contexte `Xl9535BinaryActuatorContext` reçoit maintenant un pointeur optionnel :

```cpp
Xl9535SharedOutputState* sharedOutputState;
```

## Limites volontaires de ce premier palier

Pour limiter le risque de régression :

- aucune instance globale n’est encore créée dans `main.cpp` ;
- `RelaisManager` n’est pas encore raccordé au propriétaire partagé ;
- le driver V4 n’utilise pas encore activement le pointeur ;
- aucune zone V4 n’est activée ;
- le comportement matériel reste intégralement historique.

## Palier suivant du Run 6.5A

Après compilation réussie :

1. injecter une instance unique dans `RelaisManager` ;
2. initialiser l’image partagée avec les états sûrs de chaque carte ;
3. remplacer les modifications directes `_regP0/_regP1` par `updateChannel(...)` ;
4. faire utiliser la même image par `Xl9535BinaryActuatorDriver` ;
5. conserver le masque de migration à zéro ;
6. valider la non-régression avant toute activation de zone pilote.

## Invariants préservés

- aucun changement NVS ;
- aucun changement Web, LCD ou JSON ;
- aucune zone migrée ;
- aucun backend V4 actif ;
- fallback historique conservé ;
- état sûr par défaut inchangé.
