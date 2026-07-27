# AquaLook Engineering Reference — Configuration et persistance

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/ConfigManager.h`, `src/ConfigManager.cpp`, `src/main.cpp`, `platformio.ini`
- Composants : `ConfigManager`, NVS, LittleFS
- Maturité : D4

## Mission

`ConfigManager` possède le chargement, la validation et la persistance de la configuration. Il est l’unique propriétaire du montage LittleFS. La configuration active est stockée en NVS ; l’ancien `/config.json` LittleFS n’est utilisé que pour une migration historique contrôlée.

## Constantes persistantes confirmées

Définies dans `src/ConfigManager.h` :

```cpp
#define CFG_PATH "/config.json"
#define CFG_VERSION 2
#define CFG_NVS_NAMESPACE "aqualook"
#define CFG_NVS_KEY "config"
#define CFG_NVS_INTERVAL_ANCHORS_KEY "intAnchors"
#define CFG_NVS_SCHEMA 1
```

## Structures confirmées

- `CfgWifi` : SSID et mot de passe ;
- `CfgTouch` : calibration tactile ;
- `CfgManual` : durée manuelle ;
- `CfgNtp` : serveur, décalage GMT et DST ;
- `CfgOwm` : clé API, coordonnées, unités, ville et pays ;
- `CfgSystem` : durée maximale, écran, zones, relais, logique et contrôleur ;
- `CfgZone` : nom, mode, intervalle, pluie et créneaux ;
- `CfgDisplay` : couleurs, dimensions, cadences et options météo.

La limite fonctionnelle active est `MAX_ACTIVE_ZONES = 8`; les structures internes préservent `MAX_ZONES`.

## API publique confirmée

```cpp
void begin();
void save();
void resetPersistent();
void applyToSchedule(ScheduleManager& sched) const;
const CfgWifi& wifi() const;
const CfgTouch& touch() const;
const CfgManual& manual() const;
const CfgNtp& ntp() const;
const CfgOwm& owm() const;
const CfgSystem& system() const;
const CfgZone& zone(uint8_t z) const;
uint32_t intervalAnchorDay(uint8_t z) const;
const CfgDisplay& display() const;
uint8_t nbZones() const;
uint8_t nbRelais() const;
uint8_t relayLogic() const;
uint8_t relayController() const;
bool isLoaded() const;
```

Les setters enregistrent immédiatement selon les commentaires du fichier. `setWifi()` implique sauvegarde puis redémarrage ; les paramètres d’affichage sont conçus pour le hot-reload via `EventBus::displayDirty`.

## Séquence réelle au boot

Dans `src/main.cpp` :

```cpp
configMgr.begin();
...
relaisMgr.begin(&configMgr);
...
configMgr.applyToSchedule(scheduleMgr);
wifiMgr.begin(configMgr.wifi().ssid, configMgr.wifi().password);
ntpMgr.begin(&configMgr);
weatherMgr.begin(&configMgr);
```

La configuration est donc chargée avant l’initialisation des relais, du Scheduler et des services réseau.

## LittleFS et build

`platformio.ini` fixe :

```ini
[platformio]
data_dir = littlefs
board_build.filesystem = littlefs
```

Le répertoire `littlefs/` contient uniquement les secours techniques. `data/` reste la source complète destinée à la carte SD.

## Invariants

- `INV-CFG-001` : `LittleFS.begin()` appartient à `ConfigManager`.
- `INV-CFG-002` : la configuration active est persistée en NVS versionnée.
- `INV-CFG-003` : `/config.json` est un format historique de migration, pas la source active.
- `INV-CFG-004` : toute évolution de `CFG_NVS_SCHEMA` exige migration ou repli explicite.
- `INV-CFG-005` : aucune configuration n’est appliquée directement au matériel sans validation.

## Validation

- premier boot sans NVS ;
- sauvegarde puis redémarrage ;
- conservation des zones, relais et paramètres d’affichage ;
- rejet ou repli sur données invalides ;
- migration depuis `/config.json` ;
- `pio run -e ProgrammeArrosage -t buildfs` après modification de `littlefs/`.

## Références

- `src/ConfigManager.h` ;
- `src/ConfigManager.cpp` ;
- `src/main.cpp` ;
- `platformio.ini` ;
- `docs/engineering/35_CODE_TRACEABILITY_REGISTER.md`.

## Historique

### 1.1

Consolidation D4 avec clés NVS, schéma, structures et séquence de consommation extraits du code.
