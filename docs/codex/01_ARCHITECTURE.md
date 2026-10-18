# 01 — Architecture

Mis à jour le 18 octobre 2026 (base `main` = `ba027c7`, firmware 5.20.0),
relevé dans `src/main.cpp` et les sources citées. Le détail (tâches et cœurs,
chaîne de commande d'une vanne, partitions, bibliothèques patchées, canaux de
mise à jour) est dans `docs/REPRISE_INGENIEUR.md` §4 ; ce document en donne la
vue d'ensemble et les règles de structure.

## Vue d’ensemble

```text
main.cpp (setup() enveloppé par MaintenanceSetupWrapper, -Wl,--wrap=_Z5setupv)
 ├─ ConfigManager            NVS (blob ALOK) + montage LittleFS
 ├─ StorageManager           carte SD (SdFat), pages /www, incidents SD
 ├─ RelaisManager            table de câblage (RelayTopology), sécurité de durée
 │   └─ V4PilotRuntime → EquipmentOutputRuntimeAdapter → pilotes I²C (XL9535, MCP23017)
 ├─ EquipmentManager / EquipmentOrchestrator / EquipmentExecutionShadowRuntime (V4)
 ├─ ScheduleManager          planning, pluie (RainSchedule), pauses
 ├─ InputSampler + ScriptRunner   entrées TOR déclarées, moteur de scripts
 ├─ WiFiManager + NetName    station, portail captif, mDNS
 ├─ NTPManager
 ├─ WeatherManager           OpenWeatherMap ou Open-Meteo
 ├─ CloudSync                synchro optionnelle avec le service en ligne
 ├─ UpdateCheckScheduler     vérification des mises à jour (jamais d'installation)
 ├─ NotificationManager      ntfy
 ├─ WebManager               ESPAsyncWebServer, session Web, SdStaticHandler
 └─ DisplayManager           LCD, tactile, PIN (PinLock)
       └─ ScreenManager

Transverses : EventBus, EventLog (+ EventLogCatalogue), FaultManager,
IncidentManager, BootLoopGuard, OtaBootGuard, SystemDiagnostics,
RuntimeProfiler, HeapMetrics.
```

Moteur d’exécution unique : **V4** (zones → équipements → cartes → ports →
voies). Le moteur historique n’existe plus (commit `2750866`, 8 sept. 2026).
« Cartes interfaces » (`RelayTopology`) est l’unique couche d’entrées/sorties,
sorties et entrées dans une seule table (D023, 18 oct. 2026).

## Démarrage

Avant `setup()` : `MaintenanceSetupWrapper` lance le mode maintenance
(`MaintenanceBoot`, sur une pile dédiée) si une mise à jour a été demandée,
puis redémarre ; sinon `setup()` normal.

Ordre dans `setup()` :

1. série ; `BootLoopGuard::onBoot()`, `OtaBootGuard::onBoot()` (avant toute
   initialisation lourde) ;
2. `FaultManager`, `EventLog`, mesure du tas, `SystemDiagnostics` ;
3. bus I²C des relais (`Wire1`, 400 kHz) et scan de diagnostic ;
4. `ConfigManager::begin()` (NVS, LittleFS), configuration des équipements ;
5. `PanelSetup` (réglage de la dalle), TFT et splash ;
6. `StorageManager` (SD), catalogues de messages sur la SD ;
7. relais : `RelaisManager`, pilote V4 (`V4PilotRuntime`), modèle
   d’équipements, orchestrateur ; si le pilote V4 ne démarre pas, défaut
   `RELAY_I2C` et aucune sortie pilotée (pas de repli) ;
8. planning : `ScheduleManager`, callback relais `onRelayRequest`, application
   de la configuration ;
9. Wi-Fi (`NetName::prepare()` puis `WiFiManager`) ;
10. NTP, vérification des mises à jour, CloudSync ;
11. serveur Web (handler SD, routes de défauts) ;
12. météo ;
13. affichage complet, PIN, tactile ;
14. entrées (`InputSampler`), variables et moteur de scripts.

L’ordre est important : LittleFS et la configuration avant les managers, le
pilote V4 et le callback relais avant l’exécution du planning. Chaque étape
est annoncée au splash et sur la série (règle d’identité de démarrage,
`AGENTS.md`).

## Boucle principale

Dans `loop()`, chaque poste mesuré par `RuntimeProfiler` : gardes de
démarrage, sauvegarde NVS différée, défauts, SD, Wi-Fi et mDNS ; NTP et météo
si connecté ; planificateur si NTP synchronisé ; vérification des mises à
jour ; CloudSync ; moteur V4 en validation ; entrées ; scripts ; relais et
coupure de sécurité ; Web ; affichage ; défauts ; `yield()`.

Aucun traitement d’arrosage ne doit devenir bloquant ; tout travail réseau
long vit dans une tâche dédiée (`cloud-sync`, `weather-fetch`,
`wifi-keepalive`, envois ntfy…), voir `docs/REPRISE_INGENIEUR.md` §4.2.

## Responsabilités à préserver

- **ConfigManager** : seul propriétaire de `LittleFS.begin()` et du blob de
  configuration NVS ; setters persistants. La migration depuis l’ancien
  `/config.json` a été retirée le 8 sept. 2026.
- **RelaisManager** : table de câblage, logique directe/inverse, durée
  maximale de sécurité, retour d’état ; les écritures matérielles passent par
  le pilote V4.
- **ScheduleManager** : décide quand arroser, ne touche jamais le matériel
  (callback câblé dans `main.cpp`).
- **StorageManager** : carte SD, pages `/www`, distinction carte neuve /
  panne (P3.33), incidents SD.
- **WebManager** : routes et API ; écritures sous session Web locale
  (`WebSession`, défi-réponse HMAC sur `/login`, D016 lot F) ; CSP unique
  (`WebCsp.h`) ; routes `/api/debug/*` seulement dans le binaire de banc
  (D020). Liste à jour des routes : `data/urls.html`.
- **DisplayManager** : LCD, tactile, navigation, rendu, hot-reload du thème.
- **CloudSync**, **UpdateCheckScheduler** : jamais d’installation déclenchée à
  distance (D014).

### EventBus

Drapeaux statiques : `displayDirty`, `configDirty`, `wifiDirty`,
`captiveRequested`, `updateInProgress`.

### EventLog

Journal circulaire en RAM de 60 entrées, messages de 120 octets
(`LOG_MSG_LEN`), niveaux INFO/WARN/ERROR, sans allocation dynamique ;
explications des codes dans le catalogue SD (`EventLogCatalogue`).

## Données persistées

### NVS

Configuration principale : espace `aqualook`, clé `config`, blob binaire
versionné (magic `ALOK`, schéma, taille, CRC32). Les autres modules ont leur
propre espace ou clé (par exemple `aq_topo` pour le câblage, `aq_cloud`,
`aqlsec` pour le PIN, `aqlhw` pour la dalle, `aq_script` dans `aqualook`).
L’espace `aq_io` de l’ancienne couche `IoExpander` n’est plus lu (D023).

### LittleFS

`littlefs/` (le `data_dir` PlatformIO) ne contient que les secours
indispensables : aujourd’hui `splash.jpg`. Les pages de secours (`/login`,
portail, `/ota`) sont intégrées au firmware.

### Carte SD

Les pages Web servies par le module vivent sur la SD, dans `/www`, installées
depuis le paquet Web signé (`data/` dans le dépôt).

## Frontend

Pages de `data/` : `index.html` (+ `app.js`, `index.js`), `sante`,
`cloudsync`, `logs`, `diagnostic`, `ota`, `setup`, `urls`, éditeur de scripts
(`scripts-schema.html`). Les IDs HTML sont des contrats entre les pages et
leurs scripts ; aucun script inline ni attribut `on*=`.
