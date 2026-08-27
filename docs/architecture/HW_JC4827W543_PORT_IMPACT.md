# Analyse d'impact — portage vers Guition JC4827W543C_I (ESP32-S3)

> ## ✅ Brochage confirmé sur matériel réel — 27 août 2026
>
> Les tests **1, 2, 3, 4, 5, 6, 8** du §8 sont passés sur la carte reçue
> (COM4). Le brochage croisé du 25 août ci-dessous est confirmé pour
> l'écran, le tactile et la SD ; deux réglages ont dû être ajustés par
> rapport aux hypothèses de départ (voir tableau). Détail des sessions de
> test : commits `test(hw): env test_boot_s3/test_screen_s3/
> test_relay_s3/test_touch_s3/test_perf_s3/test_sd_s3` sur
> `hw/jc4827w543-esp32s3-port-v2`.
>
> **Test 4 (décisif) — résultat chiffré** : `fillScreen` (dessin en PSRAM)
> ~7,4 ms, `flush()` (transfert QSPI vers le panneau) ~29,3 ms, soit
> ~36,7 ms par rafraîchissement plein écran complet — **34 images/s**
> maximum, **71 Mbit/s** effectifs. Tranche la question laissée ouverte au
> §3 : largement suffisant pour une interface de statut/contrôle sans
> animation, le framebuffer plein écran unique en PSRAM est validé, pas
> seulement envisagé.
>
> **Source de vérité pour tout code futur** : la section `[jc4827w543c_i]`
> de `platformio.ini` (broches en `-D` `AQ_S3_*`), réutilisée par tous les
> `env:test_*_s3` — ne pas redéfinir ces broches en dur ailleurs.
>
> | Sous-système | Brochage | Statut |
> |---|---|---|
> | Écran NV3041A (QSPI) | CS=45, SCK=47, D0=21, D1=48, D2=40, D3=39 | **confirmé** — mire couleur correcte avec `ips=true` (l'inverse, `ips=false`, essayé à tort le 26 août, est pire) |
> | Rétroéclairage | GPIO1, PWM (canal LEDC 0, style `ledcSetup`/`ledcAttachPin` — ce framework n'a pas l'API LEDC par broche des cœurs plus récents) | **confirmé** |
> | Tactile GT911 (I2C) | SCL=4, SDA=8, RST=38, INT=3 | **confirmé** — nécessite `ts.setRotation(ROTATION_INVERTED)` (valeur 1), pas `ROTATION_NORMAL` (deux axes inversés sinon) ; lib `tamctec/TAMC_GT911` |
> | Bloc relais XL9535 (I2C dédié, câblage banc de test) | SCL=17, SDA=18, adresse 0x20 | **confirmé** — logique **DIRECTE** (bit=1→ON), la même que `RelayTopology::LOGIC_DIRECT` déjà en production (`main.cpp:245`), pas réinventée |
> | Carte SD (bus SPI dédié, distinct du QSPI écran) | MISO=13, MOSI=11, SCLK=12, CS=10 | **confirmé** — SDHC ~30 Go montée, écriture/lecture identique. Carte insérée pour ce banc **sans** les fichiers du projet (3 entrées seulement, pas d'`index.html`) — normal pour du bring-up, à ne pas confondre avec une carte de production |
>
> **Bug réel trouvé par le test tactile** : `TAMC_GT911` ne déclare que
> `points[5]`, mais `ts.touches` a été observé jusqu'à 14 sur cette carte —
> boucler sur `ts.touches` sans borne lit hors tableau. Se limiter à
> `ts.points[0]` et borner à 5 par sécurité.
>
> Reste du §8 : test **9** (WiFi). Le **7** (voyant RGB) reste conditionnel,
> sans objet tant qu'aucun WS2812 n'est câblé.

> ## 🚧 Phase A du portage — socle + splash, en cours — 27 août 2026
>
> Bascule dans l'adaptation du firmware réel (pas seulement des bancs de
> test) : commit `feat(hw): socle de compilation ESP32-S3 + adaptateur
> TFT_eSPI (phase A)`. Contrainte actée par l'utilisateur : **la carte
> actuelle reste l'unité de production réelle**, son build
> (`env:ProgrammeArrosage`) ne doit jamais régresser — vérifié après
> chaque étape tout au long de cette phase, toujours au vert.
>
> Approche : adaptateur `lib/tft_espi_compat_s3/` exposant les classes
> `TFT_eSPI`/`TFT_eSprite` sur `Arduino_GFX` — `DisplayManager.cpp` et
> consorts compilent **sans modification** pour la partie dessin, seule
> la bibliothèque liée change selon l'environnement PlatformIO. Tactile
> excepté : édition directe et confinée de `getTouchPoint()` (choix
> explicite de l'utilisateur, pas un adaptateur XPT2046).
>
> Trois bugs réels trouvés et corrigés sur matériel réel, tous par
> décodage de backtrace (`addr2line`), pas par supposition :
>
> 1. **Double init du bus QSPI** — `TFT_eSprite::createSprite()` devait
>    passer `GFX_SKIP_OUTPUT_BEGIN` à `Arduino_Canvas::begin()` : sans ça,
>    le panneau déjà initialisé par `TFT_eSPI::init()` était réinitialisé
>    à la création du premier sprite → `abort()` immédiat.
> 2. **Bus I2C partagé à tort** — `TAMC_GT911` (tierce) est câblée en dur
>    sur l'objet `Wire` global ; son `begin()` y déplaçait le bus déjà
>    configuré pour le relais XL9535, qui retentait alors en continu sur
>    les mauvaises broches. Le relais utilise désormais `Wire1` (macro
>    `RELAY_WIRE_BUS`), le tactile garde `Wire`.
> 3. **Watchdog d'interruption cœur 1** — `SystemDiagnostics::sampleMemory()`
>    appelle `heap_caps_get_largest_free_block()` à chaque tour de
>    boucle ; sur cette carte (8 Mo de PSRAM), le parcours bloc par bloc
>    est devenu assez lent pour déclencher le watchdog, juste après le
>    scan WiFi. Un premier essai (restreindre à la RAM interne) n'a pas
>    suffi — même crash identique. Appel retiré pour cette carte plutôt
>    que re-deviné une deuxième fois.
>
> **État à la clôture de cette session** : démarrage complet stable et
> reproductible (COM4), les 8 étapes du splash s'exécutent, portail
> captif WiFi actif, aucun crash sur 30 s d'observation continue.
> **Rendu visuel du splash NON CONFIRMÉ** — pas d'accès physique au
> module à ce moment de la session pour vérifier à l'œil ce qui
> s'affiche réellement sur l'écran 480×272. À faire dès que possible
> avant de considérer la phase A terminée.

> ## 🌤️ Reprise du chantier — 25 août 2026
>
> **Cartes reçues.** Référence confirmée : **JC4827W543C_I** (le fabricant décline ce modèle en `C`/`R` — capacitif/résistif — et en `_I`, la révision reçue ici). Variante capacitive comme prévu au §5.
>
> Nouvelle branche `hw/jc4827w543-esp32s3-port-v2`, repartie du HEAD courant de `agent/ota-3.1-stage-inactive` (commit `949c309`) plutôt que de l'ancien point de fourche : la branche `hw/jc4827w543-esp32s3-port` d'origine datait d'avant tout le travail cloud/OTA de ces deux dernières semaines. Les deux commits de cette analyse ont été rapatriés par cherry-pick ; aucun autre historique n'a été repris. La décision du 16 août reste valable : aucun code à deux cartes.
>
> ### Brochage croisé par des sources communautaires, avant toute mesure physique
>
> Les mêmes réserves qu'au 16 août s'appliquent (documentation constructeur déjà fautive deux fois) : ce qui suit vient de plusieurs projets tiers convergents ([atomic14.com](https://www.atomic14.com/esp32/boards/guition-jc4827w543/), [profi-max/JC4827W543_4.3inch_ESP32S3_board](https://github.com/profi-max/JC4827W543_4.3inch_ESP32S3_board)), pas d'une mesure sur la carte reçue — statut inchangé tant que le test 1 du §8 n'a pas été fait.
>
> | Fonction | Broches |
> |---|---|
> | LCD NV3041A (QSPI) | CS=45, SCK=47, D0=21, D1=48, D2=40, D3=39 |
> | Rétroéclairage | GPIO1 (PWM) |
> | Tactile GT911 (I2C) | SCL=4, SDA=8, RST=38, INT=3 |
> | Carte SD (SPI **dédié**, pas le bus QSPI de l'écran) | MISO=13, MOSI=11, SCLK=12, CS=10 |
>
> 15 broches ainsi comptées. Ni la doc constructeur ni les sources communautaires ne listent les broches restantes parmi les « 10 IO utilisateur » annoncées — celles pour le bus I2C du bloc relais (test **6**, décisif) et pour un WS2812 restent à identifier sur la carte réelle.
>
> **Correction du §8 (test 7)** : pas de voyant RGB embarqué sur cette carte, contrairement à l'hypothèse initiale — des broches libres supplémentaires sont disponibles pour câbler un WS2812 (ou équivalent) en externe si un indicateur visuel est voulu. Le test 7 devient donc conditionnel : à faire seulement si un WS2812 est effectivement câblé.
>
> **Trouvaille complémentaire** ([discussion Arduino_GFX #557](https://github.com/moononournation/Arduino_GFX/discussions/557)) qui précise le §7 existant : `gfx->flush()` peut monopoliser le bus SPI et casser des accès concurrents à la SD si elle partage le même périphérique SPI que l'écran. Parade constatée par un tiers : isoler la SD sur un bus SPI dédié (`HSPI`) distinct du QSPI de l'écran — cohérent avec le brochage ci-dessus, qui donne déjà des broches SD séparées de celles du LCD.
>
> ### Où reprendre concrètement
>
> 1. Dérouler les validations par sous-système du **§8**, en commençant par les tests **1** (démarrage + PSRAM, prérequis à tout le reste), puis **4** (coût d'un rafraîchissement plein écran) et **6** (bus I2C et bloc relais) — décisifs, cf. ci-dessus.
> 2. Consigner le brochage **réellement constaté**, y compris s'il confirme le tableau ci-dessus : celui-ci reste une source tierce, pas une mesure.
> 3. Seulement ensuite, engager le portage selon l'ordre du **§9**.
>
> ### Hypothèses à revérifier avant de s'y fier
>
> Ce document a été écrit sans matériel ni bibliothèque installée. Deux points reposent sur la connaissance d'`Arduino_GFX` et non sur une compilation :
>
> - la correspondance d'API du **§11**, à confronter à la version d'`Arduino_GFX` effectivement installée ;
> - la contiguïté ligne par ligne du tampon d'`Arduino_Canvas`, dont dépend le portage des deux appels sensibles de `DisplayManager.cpp:901` et `1339`.
>
> ### Ce qui reste vrai indépendamment du matériel
>
> Les mesures du §3 portent sur le code du projet, pas sur la carte : 3 625 lignes d'affichage, 431 appels TFT_eSPI, ~423 portables mécaniquement, 8 demandant une décision individuelle. Elles ne se périment que si la couche d'affichage évolue entre-temps — auquel cas, les refaire avant de reprendre.

---

Document d'analyse préalable, rédigé le 16 août 2026 sur la branche `hw/jc4827w543-esp32s3-port`, avant réception du matériel. Aucune modification de code n'accompagne cette analyse : elle sert à dimensionner le travail et à identifier ce qui doit être vérifié sur la carte réelle. Voir le callout de reprise ci-dessus pour l'état au 25 août 2026.

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

**Arbitré par la mesure du 27 août 2026 (test 4, §8)** : un framebuffer plein écran en PSRAM impose de transférer 261 Ko à chaque rafraîchissement complet, mesuré à ~29,3 ms (`flush()` seul) sur la carte réelle — 34 images/s maximum, très au-delà du besoin d'une interface de statut/contrôle sans animation. Le framebuffer plein écran unique est retenu, les sprites partiels abandonnés.

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
| 7 | Voyant RGB (si câblé) | **Pas de voyant embarqué sur cette carte** — test conditionnel, seulement si un WS2812 externe est câblé sur une broche libre | rouge, vert, bleu, et mélanges |
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
