# AquaLook Developer Guide — Partager un expander XL9535 sans écraser les sorties

- Référence : DEV-021
- Statut : actif
- Maturité : D4

## Problème

Deux chemins qui écrivent chacun un registre complet peuvent effacer les bits pilotés par l'autre. La coexistence legacy/V4 impose une image de sortie commune par adresse I2C.

## Étapes

1. identifier l'unique adresse et les propriétaires de canaux ;
2. amorcer `Xl9535SharedOutputState` avec l'état sûr réellement appliqué ;
3. injecter la même instance dans tous les chemins autorisés ;
4. modifier uniquement le canal ciblé via `updateChannel()` ;
5. appliquer ensuite l'image complète retournée au contrôleur ;
6. traiter adresse inconnue et canal hors plage comme erreurs ;
7. synchroniser le repli après perte ou réinitialisation I2C ;
8. tester des commandes alternées sur plusieurs canaux ;
9. mesurer les sorties voisines pendant les transitions ;
10. documenter l'arbitrage avant d'ajouter un nouveau propriétaire.

## Invariants

- une image partagée par adresse ;
- aucune écriture read-modify-write locale concurrente ;
- aucun succès déclaré si l'état partagé n'est pas disponible ;
- aucune sortie voisine modifiée.

## Références

- `docs/firmware/FW-018_Adaptateurs_backends_et_etat_partage_XL9535.md`
- `docs/firmware/FW-011_RelaisManager_et_RelayTopology.md`
