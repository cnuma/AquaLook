# AquaLook Engineering Reference — microSD et ressources statiques

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/StorageManager.*`, `src/SdStaticHandler.*`, `src/WebManager.*`, `src/main.cpp`, `platformio.ini`
- Composants : SdFat, SPI logiciel, microSD, LittleFS, fallbacks firmware
- Maturité : D4

## Mission

La chaîne de stockage sert prioritairement les ressources Web présentes sur la microSD sans rendre le Runtime dépendant de la carte. Les ressources LittleFS et les fallbacks embarqués restent disponibles selon les chemins explicitement enregistrés.

## API publique confirmée

`StorageManager` expose :

```cpp
void begin();
void end();
void update();
bool isSdAvailable() const;
bool areWebAssetsAvailable() const;
StorageStatus status() const;
const char* statusCode() const;
const char* statusMessage() const;
bool existsOnSd(const char* path);
bool openRead(const char* path, FsFile& file);
void reportReadError(const char* path);
```

États : `NOT_INITIALIZED`, `READY`, `SD_UNAVAILABLE`, `WEB_ASSETS_MISSING`, `READ_ERROR`.

## Initialisation et santé

`begin()` utilise SdFat avec `SoftSpiDriver<SD_MISO_PIN, SD_MOSI_PIN, SD_SCLK_PIN>`, le CS défini par `SD_CS_PIN`, `SHARED_SPI` et `SD_SCK_MHZ(0)`.

La carte n'est déclarée prête que si :

1. `_sd.begin()` réussit ;
2. une carte et un volume sont disponibles ;
3. `/www/index.html` existe.

La sentinelle `/www/index.html` est contrôlée toutes les `2000 ms`. Sa disparition bascule l'état en `READ_ERROR`, active `FaultId::STORAGE_SD`, ferme la carte et interdit les nouvelles lectures SD.

Le calcul de l'espace libre n'est pas effectué au boot : `freeClusterCount()` est volontairement évité pour ne pas bloquer longtemps sur une grande carte en SPI logiciel.

## Ordre réel des handlers Web

Dans `main.cpp`, `registerSdStaticHandler()` est appelé avant `WebManager::begin()`. Le `SdStaticHandler` est donc consulté avant le `serveStatic()` LittleFS enregistré ensuite par `WebManager`.

Ordre effectif :

```text
requête GET/HEAD
  -> SdStaticHandler si chemin éligible et fichier SD présent
  -> handlers Web explicites / fallbacks embarqués
  -> serveStatic LittleFS
  -> onNotFound
```

Il n'existe pas de mécanisme générique firmware pour chaque ressource absente. Les fallbacks embarqués sont spécifiques.

## Manifeste URL → support

| URL ou motif | SD prioritaire | Repli confirmé |
|---|---|---|
| `/setup` | `/www/setup.html` | page captive embarquée dans `WebManager.cpp` |
| `/logs` | `/www/logs.html` | page journal embarquée par `registerFaultRoutes()` |
| `/logo.png` | `/www/logo.png` | LittleFS `/logo.png`, puis SVG firmware |
| `/*.html`, `*.css`, `*.js`, `*.json` | `/www/<URL>` | LittleFS si le handler SD décline |
| images, icônes, XML, polices, TXT | `/www/<URL>` | LittleFS si présent |
| `/api/storage` | sans fichier | JSON produit par `SdStaticHandler` |

Les chemins contenant `..`, une barre oblique inverse ou commençant par `/api/` ne sont jamais traduits en chemin SD.

## Types MIME confirmés

Le handler reconnaît HTML, CSS, JavaScript, JSON, SVG, PNG, JPEG, GIF, WebP, ICO, XML, WOFF, WOFF2 et TTF. Les autres chemins acceptés sont servis en `text/plain; charset=utf-8`.

## Livraison HTTP

Les fichiers SD sont servis par réponse chunkée. Le contexte partagé ferme automatiquement le `FsFile` en fin de lecture.

En-têtes :

- ressource SD : `X-AquaLook-Storage: SD`, cache public `300 s` ;
- logo firmware : `X-AquaLook-Storage: Firmware-Fallback`, cache public `3600 s` ;
- `/api/storage` : `no-store, no-cache, must-revalidate`.

Une erreur d'ouverture ou de lecture appelle `reportReadError()` ; l'ouverture initiale retourne HTTP `503` avec `SD read error`.

## Contrat JSON `/api/storage`

Champs confirmés :

```json
{
  "status": "ready|sd-unavailable|web-assets-missing|read-error|not-initialized",
  "message": "...",
  "sdAvailable": true,
  "webAssetsAvailable": true,
  "cardType": "SD1|SD2|SDHC/SDXC|inconnue",
  "capacityBytes": 0
}
```

`totalBytes()` et `usedBytes()` existent dans l'API C++, mais ne sont pas exposés par cette route ; `usedBytes` reste actuellement initialisé à zéro.

## Invariants

- `INV-SD-001` : l'absence ou la perte de la SD ne bloque pas le moteur d'arrosage.
- `INV-SD-002` : LittleFS est monté par `ConfigManager`, pas par `DisplayManager` ou `StorageManager`.
- `INV-SD-003` : la SD ne sert un fichier que pour GET ou HEAD et après validation du chemin.
- `INV-SD-004` : `/www/index.html` est la sentinelle de disponibilité des ressources Web SD.
- `INV-SD-005` : une erreur de lecture retire immédiatement la SD de la chaîne de résolution.
- `INV-SD-006` : un fallback firmware n'est annoncé que s'il existe réellement dans le code.

## Validation

- SD valide avec `/www/index.html` ;
- SD absente, volume invalide et `/www` incomplet ;
- retrait de carte en fonctionnement ;
- priorité SD puis LittleFS ;
- pages hybrides `/setup` et `/logs` ;
- logo sur chacun des trois niveaux ;
- rejet des traversées de chemin ;
- vérification MIME et lecture chunkée ;
- contrat `/api/storage` ;
- `pio run -e ProgrammeArrosage -t buildfs` après modification de `data/`.

## Écarts ouverts

- ajouter un manifeste versionné du contenu de la carte SD de référence ;
- exposer ou supprimer les métriques `totalBytes/usedBytes` non exploitées ;
- tester automatiquement les priorités et les en-têtes `X-AquaLook-Storage` ;
- documenter la stratégie de mise à jour atomique de `/www` ;
- qualifier le comportement d'une erreur survenant après l'envoi partiel d'une réponse chunkée.

## Références

- `src/StorageManager.h` et `.cpp` ;
- `src/SdStaticHandler.h` et `.cpp` ;
- `src/WebManager.h` et `.cpp` ;
- `src/main.cpp` ;
- `platformio.ini` ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_MAIN_STEP6_CLOSED.md`.

## Historique

### 1.1

Consolidation D4 de l'API stockage, du contrôle de santé, des chemins, types MIME, en-têtes et fallbacks réels.