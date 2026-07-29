# AquaLook Engineering Reference — Web et interfaces HTTP

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/WebManager.h`, `src/WebManager.cpp`, `src/main.cpp`, `src/SdStaticHandler.*`
- Composant : `WebManager`
- Maturité : D4

## Mission

`WebManager` expose le service HTTP local sur le port `80`, sert les ressources statiques, reçoit les commandes JSON et délègue aux propriétaires métier. Le verrouillage administrateur actuel reste visuel côté navigateur et ne constitue pas une authentification serveur.

## Initialisation confirmée

Dans `src/main.cpp`, avant `begin()` :

```cpp
webMgr.setOutputAdapter(&outputAdapter);
webMgr.registerSdStaticHandler(&storageMgr);
webMgr.registerFaultRoutes();
webMgr.begin(&ntpMgr, &weatherMgr, &relaisMgr,
             &scheduleMgr, &configMgr, &wifiMgr);
```

`begin()` enregistre les routes, démarre `AsyncWebServer _server{80}` puis appelle `_server.begin()`.

## Routes GET confirmées

| URL | Traitement |
|---|---|
| `/` | redirection vers `/index.html` |
| `/setup` | portail captif embarqué |
| `/api/status` | état fonctionnel principal |
| `/api/adminStatus` | état d’administration |
| `/api/diagnostics` | diagnostic JSON système |
| `/api/zone?z=N` | créneaux d’une zone |
| `/api/display` | lecture de la configuration d’affichage |
| `/api/wifi/scan` | scan Wi-Fi |
| `/api/logs` | journal d’événements |
| `/api/faults` | état synthétique des défauts |
| `/logs` | page de consultation du journal |

Routes de détection captive redirigées vers `/setup` :

```text
/hotspot-detect.html
/generate_204
/gen_204
/connecttest.txt
/redirect
/success.txt
/ncsi.txt
/canonical.html
/chat
```

En mode captif, `onNotFound` redirige vers `/setup`; sinon il renvoie `404 Not found`.

## Routes POST JSON confirmées

| URL | Handler |
|---|---|
| `/api/mode` | `handleSetMode` |
| `/api/interval` | `handleSetInterval` |
| `/api/intervalAnchor` | `handleSetIntervalAnchor` |
| `/api/deleteInterval` | `handleDeleteIntervalProgramming` |
| `/api/dayslot` | `handleSetDaySlot` |
| `/api/intervalslot` | `handleSetIntervalSlot` |
| `/api/rain` | `handleSetRain` |
| `/api/manual` | `handleManual` |
| `/api/manualDuration` | `handleSetManualDuration` |
| `/api/saveSchedule` | `handleSaveSchedule` |
| `/api/wifi` | `handleSetWifi` |
| `/api/touch` | `handleSetTouch` |
| `/api/ntp` | `handleSetNtp` |
| `/api/owm` | `handleSetOwm` |
| `/api/system` | `handleSetSystem` |
| `/api/zoneName` | `handleSetZoneName` |
| `/api/display` | `handleSetDisplay` |

Autres routes POST :

| URL | Traitement |
|---|---|
| `/api/captive` | activation du mode captif |
| `/api/resetConfig` | réinitialisation de la configuration |
| `/api/logs/ack` | acquittement des erreurs sans effacer le journal |

## Ressources statiques

`registerSdStaticHandler()` ajoute `SdStaticHandler` avant le démarrage du serveur. `setupRoutes()` ajoute ensuite :

```cpp
_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
```

L’ordre des handlers permet le service SD lorsqu’il est disponible, puis LittleFS. Les fallbacks firmware spécialisés restent documentés dans `14_SD_AND_STATIC_RESOURCES.md`.

## Écritures différées et redémarrage

Les modifications système ne sont pas écrites depuis le callback AsyncTCP. `handleSetSystem()` copie la demande dans une structure protégée, répond au client, puis `WebManager::update()` applique la sauvegarde dans la tâche Arduino. Un redémarrage éventuel est différé après la réponse HTTP.

`handleSetWifi()` répond également avant `ConfigManager::setWifi()`, qui sauvegarde en NVS et redémarre.

## Rafraîchissement de l’affichage

Les handlers de planning et de configuration d’affichage positionnent `EventBus::displayDirty` lorsque nécessaire. Les transitions de relais réussies utilisent un rafraîchissement dynamique plutôt qu’un redraw complet.

## Diagnostics HTTP

`sendJson()`, `sendOk()` et `sendError()` constituent les helpers de réponse. `SystemDiagnostics::noteWebResponse()` enregistre l’URI, le statut, la taille, la durée de génération et les compteurs d’erreurs HTTP.

## Sécurité actuelle

- HTTP local non chiffré sur le port 80 ;
- aucune authentification serveur forte confirmée ;
- validation des paramètres effectuée dans les handlers ;
- secrets à exclure des réponses et journaux ;
- aucune exposition Internet directe autorisée par cette architecture.

Une observation de code reste à corriger : `handleSetWifi()` imprime actuellement le mot de passe en clair sur la sortie série de diagnostic. Ce comportement est incompatible avec la politique de secrets et doit être traité dans un chantier de sécurité distinct.

## Invariants

- `INV-WEB-001` : les routes confirmées ne sont pas renommées sans décision documentée.
- `INV-WEB-002` : le client reçoit une réponse avant tout redémarrage.
- `INV-WEB-003` : aucune écriture persistante lourde n’est exécutée directement dans le callback AsyncTCP.
- `INV-WEB-004` : les actions matérielles passent par le Scheduler ou l’adaptateur de sorties.
- `INV-WEB-005` : l’absence de SD ne supprime pas le portail captif ni les ressources minimales.
- `INV-WEB-006` : le verrouillage navigateur n’est pas présenté comme une authentification.

## Validation

- requête de chaque route GET ;
- tests positifs et négatifs de chaque POST JSON ;
- réponse avant reboot ;
- fonctionnement avec et sans SD ;
- portail captif iOS, Android et Windows ;
- contrôle de `EventBus::displayDirty` ;
- absence de secrets dans les logs ;
- `pio run -e ProgrammeArrosage -t buildfs` après modification du contenu LittleFS.

## Références

- `src/WebManager.h` ;
- `src/WebManager.cpp` ;
- `src/main.cpp` ;
- `src/SdStaticHandler.h` et `.cpp` ;
- `docs/engineering/14_SD_AND_STATIC_RESOURCES.md` ;
- `docs/security/CYBERSECURITY_ARCHITECTURE.md`.

## Historique

### 1.1

Consolidation D4 avec inventaire exhaustif des routes et méthodes enregistrées par le firmware courant.