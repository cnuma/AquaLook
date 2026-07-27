# AquaLook Developer Guide — Ajouter une vue LCD

- Référence : DEV-007
- Statut : actif
- Maturité : D4

## Principe

Une vue transforme un état déjà calculé en rendu. Elle ne décide pas d’un arrosage et ne commande aucun équipement.

## Étapes

1. identifier les données nécessaires et leur propriétaire ;
2. ajouter le mode ou la vue sans dupliquer les constantes graphiques ;
3. utiliser les tokens de `CfgDisplay` ;
4. séparer rendu statique et données dynamiques ;
5. définir les causes d’invalidation ;
6. consommer `displayDirty` lorsque pertinent ;
7. respecter les cadences nominales et actives ;
8. vérifier le tactile et les zones de contact ;
9. mesurer le coût dans la boucle ;
10. tester avec réseau et SD absents.

## Interdictions

- appel direct au relais ;
- accès concurrent non maîtrisé à une ressource SPI ;
- redraw complet permanent ;
- couleurs, marges ou délais dispersés dans le code ;
- allocation dynamique répétée pendant le rendu.

## Validation

Compiler le firmware, utiliser `calibration` si nécessaire, puis tester orientation, rafraîchissement, transitions, appuis limites et fonctionnement dégradé sur la dalle réelle.

## Références

- `docs/firmware/FW-007_DisplayManager.md`
- `docs/engineering/13_DISPLAY_AND_TOUCH.md`
