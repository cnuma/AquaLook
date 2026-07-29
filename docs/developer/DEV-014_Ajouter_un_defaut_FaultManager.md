# AquaLook Developer Guide — Ajouter un défaut FaultManager

- Référence : DEV-014
- Statut : actif
- Maturité : D4

## Étapes

1. définir précisément la condition de panne et son propriétaire ;
2. ajouter un `FaultId` sans réutiliser un bit existant ;
3. activer le défaut au point où l'échec est prouvé ;
4. définir la condition mesurable qui l'efface ;
5. conserver la différence entre défaut actif et erreur non acquittée ;
6. exposer le bit dans les diagnostics pertinents ;
7. adapter la signalisation sans masquer les défauts plus critiques ;
8. tester répétition, coexistence, acquittement et résolution ;
9. vérifier le mode dégradé ;
10. documenter la preuve matérielle requise.

## Règles

Un acquittement ne doit jamais appeler implicitement `setActive(id, false)`. Un défaut ne doit pas être activé sur une simple hypothèse ni rester permanent faute de chemin de retour à la normale.

## Références

- `docs/firmware/FW-014_FaultManager.md`
- `docs/engineering/24_DIAGNOSTICS_AND_OBSERVABILITY.md`
- `docs/engineering/25_MAINTENANCE_AND_RECOVERY.md`
