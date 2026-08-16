# Analyse d'impact — portage vers Guition JC4827W543C (ESP32-S3)

Document d'analyse préalable, rédigé le 16 août 2026 sur la branche `hw/jc4827w543-esp32s3-port`, avant réception du matériel. Aucune modification de code n'accompagne cette analyse : elle sert à dimensionner le travail et à identifier ce qui doit être vérifié sur la carte réelle.

Décision d'architecture applicable (actée le 16 août 2026) : **le projet ne comportera jamais de code gérant deux cartes**. Ce portage est donc une bascule de socle, pas l'ajout d'une variante.

## 1. Matériel confirmé

| Élément | Actuel (ESP32-2432S028) | Cible (JC4827W543C) |
|---|---|---|
| MCU | ESP32 (Xtensa LX6) | **ESP32-S3-WROOM-1-N4R8** (LX7) |
| RAM interne | 520 Ko (~284 Ko de tas) | 520 Ko |
| PSRAM | aucune | **8 Mo (OSPI)** |
| Flash | 4 Mo | **4 Mo** (inchangé) |
| Dalle | 320×240 | **480×272** IPS |
| Contrôleur | ILI9341, SPI | **NV3041A, QSPI** (SPI 4 bits) |
| Tactile | XPT2046 résistif, SPI | **GT911 capacitif, I2C** |

La fiche vendeur mentionne « MCU LX6 » : c'est une erreur de copier-coller, l'ESP32-S3 est en LX7. À ne pas prendre pour argent comptant.

Point notable : cette carte utilise le QSPI là où les cartes 4,3" équivalentes (Sunton ESP32-4827S043) utilisent une interface RGB parallèle. Le QSPI **libère des GPIO**, ce qui est favorable pour raccorder l'expandeur de relais.

## 2. Ce qui n'est PAS impacté

À vérifier par recompilation, mais sans travail de conception attendu : arrosage et planification, météo, notifications, WiFi et portail captif, NTP, OTA firmware, NVS et configuration, serveur Web et l'ensemble des pages, canal de mise à jour des ressources Web, carte SD (sous réserve du brochage, voir §7).

La table de partitions reste valable telle quelle : la flash fait toujours 4 Mo. Les 84 Kio de NVS et les slots applicatifs de `0x1E0000` sont conservés. À revérifier uniquement si le binaire ESP32-S3 s'avérait sensiblement plus gros que les 72 % actuels.

## 3. Le cœur du sujet — la couche d'affichage

`TFT_eSPI` ne pilote pas d'écran QSPI. La bibliothèque de référence pour cette carte est `Arduino_GFX` (GFX Library for Arduino).

Surface concernée, mesurée sur le code réel :

- **3 625 lignes** d'affichage (`DisplayManager`, `DisplayPlanningDecor`, `DisplaySplashWrap`, `ScreenManager`) ;
- **431 appels** à l'API TFT_eSPI ;
- **4 sprites** effectivement créés (`_sprTime` 110×20, `_sprSignal` 20×16, `_sprPlan` 320×90, `_sprBtn0` 154×120).

### Répartition des appels — ce qui dicte la stratégie

| Catégorie | Appels | Portabilité vers Arduino_GFX |
|---|---|---|
| Texte (`drawString`, `setTextColor`, `setTextDatum`, `setTextSize`, `print`/`println`, `setFreeFont`) | ~473 | **difficile** — pas d'équivalent direct de `drawString` avec alignement par datum |
| Primitives (`fillRect`, `fillRoundRect`, `drawFastVLine/HLine`, `fillCircle`, `drawLine`, `drawRect`, `fillScreen`) | ~130 | directe, mêmes noms et sémantique |
| Sprites (`createSprite`, `fillSprite`, `pushSprite`, `getPointer`) | 13 | conceptuellement équivalent via `Arduino_Canvas` |
| Divers (`init`, `setRotation`, `pushImage`, `setSwapBytes`) | ~15 | ponctuel |

**Conclusion opérationnelle : le texte représente l'essentiel du coût, pas le graphisme.** `Arduino_GFX` suit le modèle Adafruit-GFX (`setCursor` puis `print`), sans notion de datum d'alignement (`TL_DATUM`, `TC_DATUM`, `MC_DATUM`…) dont le code fait un usage intensif.

### Stratégie retenue : adaptateur, pas réécriture

Réécrire 431 sites d'appel serait long, risqué et sans valeur ajoutée. La voie raisonnable est un **adaptateur mince** exposant l'API TFT_eSPI utilisée par le projet, implémenté au-dessus d'`Arduino_GFX` :

- `drawString(texte, x, y)` combiné au datum courant → calcul du décalage via `getTextBounds()`, puis `setCursor()` + `print()` ;
- `setTextDatum()` → mémorisation d'un état dans l'adaptateur ;
- `setFreeFont(GFXfont*)` → `setFont(GFXfont*)`. **Les polices se portent telles quelles** : les FreeFonts de TFT_eSPI sont au format Adafruit GFXfont, celui qu'attend `Arduino_GFX` ;
- `TFT_eSprite` → `Arduino_Canvas` (surface hors écran, `pushSprite` → recopie vers l'écran) ;
- primitives graphiques → délégation directe.

Bénéfice : la logique de mise en page, de rendu et d'interaction — l'essentiel des 3 625 lignes — reste inchangée. Le portage se concentre dans un fichier dédié, testable isolément, et réversible.

### Opportunité : abandonner les sprites partiels

Avec 8 Mo de PSRAM, un **framebuffer plein écran** coûte 480 × 272 × 2 = **261 Ko**, soit 3 % de la PSRAM disponible. Il devient donc envisageable de remplacer les quatre sprites partiels par une surface unique.

Gains attendus :

- suppression de la gymnastique de placement des sprites et de leurs contraintes de coordonnées ;
- **suppression du correctif de libération des sprites en veille** (ajouté le 16 août 2026 pour cause de saturation mémoire) et, avec lui, de toute la classe de bugs « RAM interne saturée » — page Web qui ne se charge pas, poignée de main TLS en échec ;
- levée du plafond de connexions HTTP simultanées, aujourd'hui de deux écran allumé.

À arbitrer après mesure : un framebuffer plein écran en PSRAM impose de transférer 261 Ko à chaque rafraîchissement complet. C'est précisément le point de performance à mesurer en premier (voir §8).

## 4. Mise en page — 320×240 vers 480×272

Ce n'est pas un changement d'échelle mécanique : le rapport d'aspect passe de 4:3 à ~16:9, et la surface augmente de 70 %.

Constantes concernées, toutes dans `DisplayManager.h` : `PL_PLAN_Y`, `PL_PLAN_H`, `PL_HDR_H`, `PL_ZONE_H`, `PL_Z0_ROW_Y`, `PL_Z1_ROW_Y`, `PL_BTN_W`, `PL_BTN_H`, ainsi que les trois dispositions `HomeMode` (LIST 1–4 zones, GRID2 5–8, GRID4 9–16) et les écrans HOME / ZONE / STATUS / SYSTEM / ADMIN.

Il s'agit d'un travail de conception d'interface, pas de transposition : la largeur supplémentaire permet d'afficher davantage sans réduire la lisibilité, ce qui mérite d'être exploité plutôt que subi. À traiter comme une amélioration de l'affichage, l'occasion étant donnée.

## 5. Tactile — XPT2046 vers GT911

Variante commandée : **capacitive**, donc GT911 en I2C.

- remplacement de `XPT2046_Touchscreen` par un pilote GT911 ;
- `getTouchPoint()` est le seul point d'entrée du reste du code : l'impact est confiné ;
- **simplification** : le tactile capacitif ne nécessite pas d'étalonnage. Les réglages `TOUCH_X_MIN/MAX`, `TOUCH_Y_MIN/MAX`, la route `/api/touch` et l'écran d'étalonnage deviennent sans objet — à retirer plutôt qu'à porter ;
- attention au partage du bus I2C avec l'expandeur de relais XL9535 (adresse 0x20) : adresses distinctes, mais brochage et fréquence à vérifier ensemble.

## 6. Configuration de compilation

| Élément | Changement |
|---|---|
| `board` | `esp32dev` → carte ESP32-S3 (`esp32-s3-devkitc-1` ou définition dédiée) |
| PSRAM | `-DBOARD_HAS_PSRAM`, type mémoire **OSPI** (octal) et non quad |
| Bibliothèque écran | `bodmer/TFT_eSPI` → `moononournation/GFX Library for Arduino` |
| Drapeaux TFT_eSPI | les ~15 `-DTFT_*` / `-DILI9341_2_DRIVER` deviennent sans objet |
| Brochage QSPI | à renseigner d'après la documentation constructeur |
| Tactile | drapeaux XPT2046 remplacés par l'I2C du GT911 |
| Console série | l'ESP32-S3 dispose d'un USB natif : le flux de supervision série peut différer |

## 7. À vérifier sur la carte, avant tout développement

Ces deux points conditionnent la faisabilité même, indépendamment de l'affichage :

1. **GPIO disponibles pour l'I2C du bloc relais.** C'est la fonction cœur du produit. Le connecteur JST 1.25 documenté est un UART ; il faut identifier les broches réellement exposées et libres. Le QSPI en libère davantage qu'une interface RGB, mais cela reste à constater.
2. **Lecteur de carte SD : présence et brochage.** Tout le service des pages Web et le canal de mise à jour en dépendent.

Points secondaires à confirmer : luminosité du rétroéclairage et sa commande, alimentation et consommation avec le bloc relais, comportement de l'USB natif pour le flashage et la supervision.

## 8. Validation par sous-système avant tout portage

Principe retenu : **ne rien porter avant d'avoir validé chaque sous-système isolément**, au moyen de programmes minimaux et jetables. Sur une carte dont le brochage n'est pas encore constaté, un défaut sur un firmware complet est très coûteux à diagnostiquer — on ne sait pas si le fautif est la configuration de l'écran, le brochage, la bibliothèque ou le code métier. Isolé, chaque test répond à une seule question.

Le projet pratique déjà cette approche : `platformio.ini` comporte les environnements `test_relais`, `calibration`, `debug_boot`, `test_ota_https` et `test_execution_engine`, chacun avec son propre `build_src_filter`. Les validations ci-dessous suivent le même modèle et n'introduisent donc aucune méthode nouvelle.

| # | Sous-système | Question à laquelle le test répond | Critère de réussite |
|---|---|---|---|
| 1 | Démarrage et PSRAM | La carte démarre-t-elle, la PSRAM octale est-elle vue ? | 8 Mo rapportés, console série lisible |
| 2 | Rétroéclairage | Quelle broche le commande, est-il réglable ? | allumage, extinction, variation |
| 3 | Écran QSPI | Le NV3041A s'initialise-t-il, couleurs et orientation correctes ? | mire stable, rouge/vert/bleu justes, pas d'inversion |
| 4 | **Performance d'affichage** | Combien coûte un rafraîchissement plein écran depuis la PSRAM ? | **mesure chiffrée** — décide de la stratégie du §3 |
| 5 | Tactile GT911 | Adresse I2C, coordonnées, orientation cohérente avec l'écran ? | appui restitué au bon endroit, sans étalonnage |
| 6 | **Bus I2C et bloc relais** | Quels GPIO sont libres, le XL9535 répond-il en 0x20 ? | **commutation réelle d'une voie**, cohabitation avec le GT911 |
| 7 | Voyant RGB | Les trois canaux répondent-ils, sur quelles broches ? | rouge, vert, bleu, et mélanges |
| 8 | Carte SD | Présence, brochage, montage, lecture et écriture | fichier écrit puis relu à l'identique |
| 9 | WiFi | Connexion et portée | association, adresse IP, RSSI correct |

Les tests **4** et **6** sont décisifs : le premier peut invalider la stratégie d'affichage du §3, le second conditionne la fonction cœur du produit. Les mener en premier, avant tout investissement dans le portage.

Chaque validation produit une trace consignée : brochage constaté, valeurs mesurées, écarts avec la documentation constructeur. Ces relevés deviendront la référence de brochage du projet — la documentation vendeur de ce type de carte étant, comme la mention « LX6 » l'a montré, à confronter systématiquement au réel.

## 9. Ordre de travail proposé

1. **Validations par sous-système** (§8), en commençant par les tests 4 et 6.
2. **Socle de compilation** : `platformio.ini` ESP32-S3, PSRAM octale, `Arduino_GFX`.
3. **Adaptateur d'affichage** (§3) et portage du splash, l'écran le plus simple.
4. **Pilote tactile GT911** et retrait de l'étalonnage devenu sans objet.
5. **Reprise de la mise en page** en 480×272, écran par écran.
6. **Revalidation fonctionnelle complète** : arrosage, planification, Web, OTA, NVS, SD.

L'étape 1 est la seule réellement décisive : elle peut remettre en cause la stratégie. Le reste est du travail dont l'issue est prévisible.

## 10. Risques identifiés

| Risque | Portée | Atténuation |
|---|---|---|
| Rafraîchissement PSRAM trop lent | conception de l'affichage | mesuré à l'étape 2, avant tout engagement ; repli sur des surfaces partielles |
| GPIO insuffisants pour le bloc relais | **fonction cœur** | vérifié à l'étape 1, avant tout développement |
| API `Arduino_GFX` divergente de l'analyse | adaptateur | analyse fondée sur la connaissance de la bibliothèque, **à confronter à la version réellement installée** |
| Binaire S3 plus volumineux | table de partitions | marge actuelle de 28 % ; table ajustable, la flash restant de 4 Mo |
| Absence de lecteur SD | ressources Web | vérifié à l'étape 1 |

## 11. Annexe — correspondance d'API détaillée

Établie par analyse du code réel. À confronter à la version d'`Arduino_GFX` effectivement installée avant de s'y fier : cette correspondance repose sur la connaissance de la bibliothèque, non sur une compilation.

### Ce qui se porte mécaniquement

| TFT_eSPI | Occurrences | Arduino_GFX | Remarque |
|---|---|---|---|
| `fillRect`, `drawRect`, `fillRoundRect`, `drawRoundRect` | 71 | identiques | signature et sémantique équivalentes |
| `drawFastVLine`, `drawFastHLine`, `drawLine` | 48 | identiques | |
| `fillCircle` | 11 | identique | |
| `fillScreen` | 7 | identique | |
| `setTextColor(fg[, bg])` | 105 | identique | |
| `setTextSize` | 60 | identique | |
| `print` / `println` | 99 | identiques | héritées de `Print` |
| `setFreeFont(&FreeSansBold9pt7b)` | 30 | `setFont(const GFXfont*)` | **aucune conversion de police** — voir ci-dessous |

**Les polices ne demandent aucun travail.** `Theme.h` référence `&FreeSans9pt7b`, `&FreeSansBold9pt7b`, `&FreeSansBold12pt7b` : ce sont des polices au format Adafruit `GFXfont`, précisément celui qu'`Arduino_GFX` attend nativement. C'est une bonne surprise sur un poste habituellement coûteux.

**Les couleurs non plus.** Le thème est déjà en RGB565 (`AMBER = 0xFD20`), format commun aux deux bibliothèques. Seul l'ordre des octets est à vérifier à l'affichage de la mire (test n° 3).

### Ce qui demande l'adaptateur

| TFT_eSPI | Occurrences | Traitement |
|---|---|---|
| `drawString(txt, x, y)` | 111 | combiné au datum courant : mesurer via `getTextBounds()`, décaler, puis `setCursor()` + `print()` |
| `setTextDatum(pos)` | 68 | état mémorisé dans l'adaptateur, consommé par `drawString` |
| `textWidth`, `fontHeight` | — | `getTextBounds()` |

C'est le seul poste réellement substantiel, et il est concentré dans une poignée de méthodes de l'adaptateur — pas dispersé dans 179 sites d'appel.

### Ce qui demande une réflexion individuelle

Huit sites d'appel seulement, mais qui ne se traitent pas mécaniquement :

| Site | Occurrences | Enjeu |
|---|---|---|
| `sprite.getPointer()` | 2 | → `Arduino_Canvas::getFramebuffer()` |
| `_tft.pushImage(x, y, w, h, ptr)` | 2 | → `draw16bitRGBBitmap()`. **Usage subtil** : le code pousse volontairement une *sous-partie* du tampon (`visibleH` lignes d'un sprite plus haut), avec un commentaire explicite en `DisplayManager.cpp:901`. Fonctionne parce que le tampon est contigu ligne par ligne — propriété à confirmer sur `Arduino_Canvas` |
| `sprite.pushSprite(x, y)` | 3 | → recopie de la surface vers l'écran |
| `TJpgDec.setSwapBytes(true)` | 1 | le décodeur JPEG est **local au projet** (`src/TJpg_Decoder.h`) ; seul son rappel de sortie, qui appelle `pushImage`, est à réorienter |

### Constantes de dimension

`SCREEN_W` (45 occurrences) et `SCREEN_H` (10) sont définis en dur dans `DisplayManager.cpp:83`. Leur simple modification ne suffira pas : de nombreux calculs de mise en page en dérivent avec des marges ajustées visuellement pour 320×240. Elles servent de point d'entrée au travail du §4, pas de solution.

### Bilan de portabilité

| Nature | Sites | Effort |
|---|---|---|
| Portage mécanique via l'adaptateur | ~423 | faible, une fois l'adaptateur écrit |
| Sites demandant une décision individuelle | 8 | modéré, bien identifiés |
| Mise en page 480×272 | — | **le vrai travail**, de nature conception |

Autrement dit : le risque n'est pas dans le nombre d'appels, mais dans la refonte de la mise en page. C'est rassurant, car c'est un travail visible et vérifiable à l'œil, pas une chasse aux régressions invisibles.

## 12. Ce que cette analyse ne prétend pas être

Une estimation en jours. Les étapes 1 et 2 peuvent invalider la stratégie du §3 ; chiffrer avant de les avoir menées produirait un nombre faux et rassurant. Le portage sera chiffré après la mesure de performance, pas avant.
