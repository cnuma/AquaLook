# AquaLook V4 — Driver binaire simulé

**Date :** 7 juillet 2026  
**Run :** Phase 3 — Run 3.2

## Périmètre

Le driver simulé valide toute la chaîne du contrat binaire sans matériel :

```text
registre
configuration
commande
idempotence
lecture
état sûr
santé
pannes injectées
```

## Contexte

`SimulatedBinaryActuatorContext` occupe 24 octets et mémorise l’état, la santé, les compteurs d’appels et le masque de pannes.

## Pannes disponibles

```text
CONFIGURE
WRITE
READ
SAFE_STATE
UNAVAILABLE
READBACK_MISMATCH
```

## Résultat du test hôte

```text
Compilation hôte OK
ok 2 2 3 1
```

Interprétation :

```text
2 configurations
2 écritures tentées
3 lectures
1 application d’état sûr
```

Le scénario valide :

- configuration nominale ;
- commande ACTIVE ;
- répétition idempotente sans seconde écriture ;
- lecture ACTIVE ;
- retour à INACTIVE par état sûr ;
- erreur d’écriture sans modification de session ;
- erreur de lecture ;
- readback incohérent avec santé DEGRADED ;
- indisponibilité simulée.

## Hors périmètre

Aucun GPIO, I²C, driver XL9535, `RelaisManager`, NVS, Web ou LCD n’est modifié.

## Suite

Le Run 3.3 pourra introduire un premier driver concret conditionnel, idéalement le GPIO local, tout en conservant le driver simulé comme référence de test.
