# AquaLook Firmware — DisplayManager

- Référence : FW-007
- Statut : relié au code
- Maturité : D4
- Sources : `src/DisplayManager.*`, `src/ScreenManager.*`

## Mission

Piloter l’écran TFT, le tactile XPT2046, les vues locales et les rafraîchissements sans introduire de logique métier.

## Fonctionnement

- initialise TFT et tactile ;
- affiche le splash et la progression de démarrage ;
- choisit la vue selon la configuration et le nombre de zones ;
- lit l’état réel depuis les adaptateurs Runtime ;
- applique les cadences nominales et actives ;
- consomme `EventBus::displayDirty` pour forcer un redraw.

## Contraintes

- bus tactile séparé du TFT ;
- polling tactile borné et debounce ;
- aucun redraw complet inutile dans la boucle ;
- la configuration graphique appartient à `ConfigManager` ;
- l’affichage ne commande jamais directement un relais.

## Écarts connus

- mode `GRID4` encore présent mais dormant ;
- validation matérielle requise pour rotation, calibration et cadence ;
- consommation mémoire des ressources graphiques à surveiller.

## Validation

- compilation du firmware concerné ;
- environnement `calibration` si les bornes tactiles changent ;
- test écran/tactile sur cible ;
- test des modes sans SD et sans réseau.

## Références

- `docs/engineering/13_DISPLAY_AND_TOUCH.md`
- `docs/developer/DEV-007_Ajouter_une_vue_LCD.md`
