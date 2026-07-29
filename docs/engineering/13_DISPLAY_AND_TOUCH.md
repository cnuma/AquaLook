# AquaLook Engineering Reference — Affichage et tactile

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/DisplayManager.h`, `src/DisplayManager.cpp`, `src/ScreenManager.*`, `src/main.cpp`, `platformio.ini`
- Composants : `DisplayManager`, TFT_eSPI, XPT2046, sprites, `ScreenManager`, `EventBus`
- Maturité : D4

## Mission

`DisplayManager` initialise l’écran et le tactile, rend les vues locales, gère la navigation et applique une politique de rafraîchissement bornée. Il consulte les managers métier mais ne décide pas des déclenchements d’arrosage.

## API publique confirmée

```cpp
void initTft();
void showSplash(uint8_t step, const char* label);
void begin(NTPManager* ntp, WeatherManager* weather,
           RelaisManager* relais, ScheduleManager* schedule,
           ConfigManager* config);
void update();
void requestDynamicRefresh();
void setOutputAdapter(EquipmentOutputRuntimeAdapter* outputs);
```

`SPLASH_STEPS` vaut `8`.

## Matériel et bus

- TFT : `TFT_eSPI`, rotation `1`, résolution logique 320 × 240 ;
- tactile : `XPT2046_Touchscreen` ;
- bus tactile : instance `SPIClass(VSPI)` séparée ;
- initialisation tactile : `_touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS)`, puis `_touch.begin(_touchSPI)` et rotation `1` ;
- calibration : valeurs `CfgTouch` fournies par `ConfigManager`, avec repli sur les constantes de compilation.

Les broches TFT et tactile du profil courant sont définies dans `platformio.ini`. `DisplayManager` ne monte pas LittleFS : le montage appartient à `ConfigManager`.

## Séquence d’initialisation réelle

Dans `src/main.cpp` :

1. `displayMgr.initTft()` ;
2. affichage progressif du splash pendant l’initialisation des managers ;
3. `displayMgr.setOutputAdapter(&outputAdapter)` ;
4. `displayMgr.begin(...)` après Web et météo.

`initTft()` initialise le TFT, configure TJpgDec et remplit l’écran. `showSplash()` charge `/splash.jpg` depuis LittleFS s’il existe, sinon utilise un rendu texte embarqué.

## Modes et vues

Écrans déclarés : `HOME`, `ZONE`, `STATUS`, `SYSTEM`, `ADMIN`.

Modes HOME déclarés :

- `LIST` pour 1 à 4 zones ;
- `GRID2` pour 5 à 8 zones ;
- `GRID4` est encore présent dans le code, mais `begin()` borne `_nbZones` à 8 et ne sélectionne actuellement que `LIST` ou `GRID2`.

Le mode `GRID4` constitue donc du code dormant, pas une capacité active du firmware courant.

Pages ADMIN : Wi-Fi, NTP, météo, zones, système et logs.

## Politique de rafraîchissement réelle

`update()` applique l’ordre suivant :

1. mise à jour de `ScreenManager` et détection d’une zone active ;
2. en veille, surveillance tactile uniquement pour réveil ;
3. polling tactile toutes les `80 ms` ;
4. application de la configuration d’affichage avant consommation de `EventBus::displayDirty` ;
5. redraw complet uniquement si `_needsFullRedraw` ;
6. sinon rafraîchissement dynamique à l’intervalle actif ou nominal.

Valeurs par défaut :

- nominal : `5000 ms` ;
- arrosage actif : `1000 ms` ;
- debounce tactile : `500 ms` ;
- polling tactile : `80 ms`.

Ces cadences peuvent être surchargées par `CfgDisplay`.

`requestDynamicRefresh()` avance `_lastUpdate` afin de provoquer un rafraîchissement partiel au prochain cycle, sans `fillScreen()`.

## Redraw complet et caches

`fillScreen()` est exécuté dans le chemin `_needsFullRedraw` de `update()`, en plus des phases explicites d’initialisation et de splash. Les transitions normales ON/OFF utilisent un refresh dynamique et ne forcent pas un redraw complet.

Des caches statiques suivent l’état actif et le temps restant des zones afin d’éviter le redessin complet des cartes chaque seconde. Les sprites principaux sont créés une seule fois dans `createSprites()`.

## Tactile

`getTouchPoint()` :

1. vérifie `tirqTouched()` et `touched()` ;
2. lit `TS_Point` ;
3. transforme les valeurs brutes selon la calibration ;
4. contraint les coordonnées à 320 × 240.

`handleTouch()` applique un debounce de `500 ms`, réveille l’écran puis délègue selon la vue active.

## État des sorties affiché

`OutputAwareRelayState` consulte en priorité `EquipmentOutputRuntimeAdapter::getZoneValveState()`. Il n’utilise `RelaisManager::getState()` qu’en repli. L’affichage privilégie ainsi l’état Runtime unifié lorsqu’il est valide.

## Invariants

- `INV-DSP-001` : le tactile utilise le bus VSPI séparé prévu.
- `INV-DSP-002` : les transitions normales de zone utilisent le rafraîchissement dynamique, pas un redraw complet.
- `INV-DSP-003` : `applyDisplayConfig()` précède la consommation de `displayDirty`.
- `INV-DSP-004` : en veille, aucun rendu n’est effectué ; le tactile reste surveillé pour le réveil.
- `INV-DSP-005` : la calibration provient de la configuration persistée avec repli contrôlé.
- `INV-DSP-006` : l’état des sorties fourni par l’adaptateur Runtime est prioritaire sur l’état legacy.
- `INV-DSP-007` : les polices GFXFF déjà incluses par TFT_eSPI ne sont pas réincluses séparément.

## Validation

- environnement `calibration` pour le tactile ;
- boot et progression des 8 étapes du splash ;
- présence et absence de `/splash.jpg` ;
- modes LIST et GRID2 ;
- veille, réveil et debounce ;
- rafraîchissement nominal et actif ;
- modification de thème via `/api/display` ;
- transitions de zone sans scintillement ;
- comparaison état OutputAdapter / fallback RelaisManager.

## Écarts ouverts

- décider de supprimer ou réactiver explicitement `GRID4` ;
- archiver une preuve de calibration matérielle pour chaque variante de dalle ;
- ajouter des tests automatisés sur la sélection des modes et les cadences ;
- vérifier l’utilité de `tftOutputCallback()` distinct du callback lambda réellement configuré.

## Références

- `src/DisplayManager.h` ;
- `src/DisplayManager.cpp` ;
- `src/ScreenManager.h` et `.cpp` ;
- `src/main.cpp` ;
- `src/ConfigManager.h` ;
- `platformio.ini` ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_MAIN_STEP6_CLOSED.md`.

## Historique

### 1.1

Consolidation D4 de l’API, des bus, des cadences, du tactile, des modes actifs et de la politique de redraw.