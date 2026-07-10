# AquaLook V4 — Phase 6 — Run 6.5

## Blocage de sécurité : propriété partagée du registre XL9535

**Date :** 10 juillet 2026  
**Statut :** activation matérielle suspendue avant câblage runtime

## Constat

Le profil V4 vise une activation contrôlée d'une seule zone pilote tandis que les autres zones restent pilotées par `RelaisManager`.

Sur une même carte XL9535, les deux chemins partageraient cependant le même registre matériel de sortie 16 bits :

```text
zone pilote       -> V4RelayPhysicalBackend -> Xl9535BinaryActuatorDriver
autres zones      -> RelaisManager          -> XL9535
```

Le driver V4 conserve une image locale `outputLatch` puis écrit l'intégralité du registre 16 bits. Il ne relit pas le registre de sortie avant chaque modification de bit.

Conséquence possible :

```text
1. RelaisManager active une zone non migrée ;
2. son image de registre devient la plus récente ;
3. le backend V4 commande la zone pilote avec une image locale différente ;
4. l'écriture 16 bits V4 peut écraser l'état des autres zones.
```

## Décision

Aucune activation physique de zone pilote n'est réalisée dans cet état.

Le Run 6.5 est scindé :

```text
Run 6.5A  rendre l'accès XL9535 compatible avec une propriété partagée
Run 6.5B  activer une zone pilote derrière le profil ProgrammeArrosage_v4
```

## Précondition 6.5A

Avant toute coexistence, le chemin V4 doit garantir l'une des stratégies suivantes :

1. **propriétaire unique par carte** — migration complète de toutes les voies d'une carte ;
2. **registre partagé centralisé** — une seule image de sortie utilisée par les deux chemins ;
3. **read-modify-write matériel** — lecture du registre de sortie avant modification d'un seul bit, avec protection contre les accès concurrents.

La stratégie recommandée pour la transition est le registre partagé centralisé ou, à défaut, la migration par carte complète. Le read-modify-write seul ne supprime pas toutes les courses concurrentes sans synchronisation.

## Invariants conservés

- aucune modification de `main.cpp` ;
- aucune instance V4 activée ;
- aucune zone migrée ;
- fallback historique conservé ;
- aucune modification NVS, Web, LCD ou JSON ;
- état matériel actuel inchangé.

## Suite proposée

```text
AquaLook V4 — Phase 6 — Run 6.5A
Arbitrage de propriété XL9535 et registre de sortie partagé
```

Cette étape doit être validée avant l'activation réelle d'une zone pilote.
