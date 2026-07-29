# AquaLook Developer Guide — Ajouter un test ou un banc matériel

- Référence : DEV-011
- Statut : actif
- Maturité : D4

## Choisir le niveau

Utiliser un test hôte pour une règle déterministe sans dépendance Arduino, un test de contrat pour une propriété statique du dépôt, et un banc matériel pour une sortie électrique, un bus, un écran, une SD ou un comportement temporel réel.

## Étapes

1. définir le risque et le résultat observable ;
2. isoler le composant et ses entrées ;
3. fixer les cas nominaux, limites et erreurs ;
4. créer le test dans l'environnement PlatformIO adapté ou dans `tests/contracts/` ;
5. éviter les temporisations arbitraires ;
6. pour un banc, limiter la première activation à une zone et une durée courte ;
7. consigner carte, câblage, profil, port, stimulus et observation ;
8. exécuter legacy et V4 lorsque les deux chemins sont concernés ;
9. documenter ce qui n'a pas été testé ;
10. relier le test à la matrice anti-régression.

## Preuves

Une compilation réussie ne prouve pas une action matérielle. Un test sur matériel doit décrire le point d'entrée exécuté et l'effet mesuré ou observé.

## Références

- `docs/engineering/17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md`
- `docs/engineering/33_ANTI_REGRESSION_MATRIX.md`
- `AGENTS.md`
