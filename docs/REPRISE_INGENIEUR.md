# AquaLook — Dossier de reprise ingénieur

- Date de rédaction : 5 octobre 2026
- Base inspectée : branche `feat/moteur-de-regles`, commit `485ce3c` (état
  Git mis à jour après le ménage du 5 octobre, base `d83f0d5`)
- Version fonctionnelle (`VERSION`) : `5.9.7` — dernière release publiée sur GitHub : tag `v5.9.7` (17 août 2026)
- Public visé : ingénieur embarqué qui reprend le projet sans avoir suivi son historique

Ce document est le **point d'entrée unique** pour reprendre AquaLook. Il ne
remplace pas le corpus existant (`docs/engineering/`, `docs/codex/`,
`docs/architecture/`, `docs/firmware/`, `docs/developer/`) : il dit ce qui est
vrai **aujourd'hui**, ce qui a changé depuis que ce corpus a été écrit, et où
creuser. Quand un document plus ancien contredit celui-ci, c'est en général
l'ancien qui est périmé — la section 12 donne la fraîcheur de chaque famille.

---

## 1. À lire avant toute action — cinq alertes

1. **Tout le travail vit sur `feat/moteur-de-regles`, pas sur `main`.**
   La branche a plus de 400 commits d'avance sur `origin/main` ; les 272
   commits postérieurs à `v5.9.7` (portage ESP32-S3, moteur V4 seul,
   CloudSync, moteur de scripts, page Santé, correctifs réseau) n'ont jamais
   été fusionnés. Elle est poussée sur GitHub depuis le 5 octobre 2026 (elle
   n'existait auparavant qu'en local). `main` a de son côté 6 commits absents
   de la branche : préparation de la release 5.9.2 et un workflow CI
   temporaire ajouté puis retiré — fusion attendue sans difficulté.
   **Le dépôt est public** : ne jamais y committer de secret (`.env`,
   identifiants Wi-Fi, jetons).

2. **Le dépôt a été nettoyé le 5 octobre 2026.** Il ne reste que deux
   branches, `main` et `feat/moteur-de-regles`, et aucune pull request
   ouverte. Les 25 anciennes branches distantes ont été supprimées après
   archivage sous des tags `archive/<nom-de-branche>` (vérifiés un à un sur le
   dernier commit de chaque branche) ; les PR #21 et #24 ont été fermées avec
   un renvoi vers leur archive. Récupérer une branche :
   `git checkout -b <nom> archive/<nom>`. Les derniers travaux non commités
   (banc N16R8 sans écran, badge « Aqualook / Aqualook Pro », purge
   `cleanup.php`, registre de soak) ont été commités et poussés ; **le banc
   N16R8 et le badge n'ont toujours pas été validés sur matériel**.

3. **`AGENTS.md` est partiellement obsolète sur le point central du build.**
   Il impose de compiler `ProgrammeArrosage_legacy` comme référence et de
   flasher `ProgrammeArrosage_v4`. Or le moteur historique a été **supprimé**
   le 8 septembre (commit `2750866`) : V4 est le seul moteur. `_legacy` et
   `_v4` sont désormais deux alias identiques de `ProgrammeArrosage` (carte
   CYD), et **la cible de production réelle est `ProgrammeArrosage_s3`**. Les
   autres règles d'`AGENTS.md` (sécurité relais, persistance, LittleFS,
   identité de build, checkpoint) restent valides. Voir §5 pour la chaîne de
   build à jour, et §11 pour la proposition de mise à jour d'`AGENTS.md`.

4. **La documentation de référence est figée fin juillet.** `docs/START_HERE.md`,
   `docs/engineering/01_PROJECT_STATUS.md` et `docs/codex/00_CONTEXT.md`
   décrivent encore : OTA « téléchargement seulement, `setInsecure()` »,
   notifications ntfy « en échec TLS », plate-forme unique CYD, MQTT/HiveMQ
   comme trajectoire. Tout cela est dépassé (voir §7). Seuls les chapitres
   `08`, `15`, `18` et `31` du manuel d'ingénierie ont été tenus à jour en
   septembre-octobre.

5. **L'interface Web du module n'a aucune authentification.** Choix assumé
   pendant la conception (accès direct aux routes `/api/*` pour le
   diagnostic). `/api/resetConfig`, `/api/zone`, les routes Wi-Fi sont
   ouvertes à tout poste du LAN. Les écritures sensibles récentes (scripts,
   câblage, redémarrage) sont signées HMAC (`data/hmac.js`, `ApiAuth`), mais
   l'ensemble n'est pas fermé. **À traiter avant toute installation réelle ou
   exposition hors LAN.**

---

## 2. Le produit en une page

AquaLook est un programmateur d'arrosage autonome sur ESP32 :

- planification locale de 1 à 8 zones (5 créneaux/jour/zone, jours fixes ou
  intervalle), décalage ou annulation sur pluie (prévisions OpenWeatherMap ou
  Open-Meteo/Météo-France) ;
- pilotage de vannes par cartes relais I²C (XL9535, MCP23017), plusieurs
  cartes possibles, câblage décrit comme une **donnée** (table de topologie
  éditable dans le navigateur) ;
- écran tactile local, interface Web locale, portail captif de première
  configuration ;
- **moteur de scripts** embarqué (langage maison compilé dans le navigateur,
  exécuté par une machine à pile non bloquante) réagissant aux entrées TOR et
  aux démarrages/arrêts de zones ;
- notifications push ntfy (HTTPS) ;
- mise à jour assistée du firmware (GitHub Releases) et des ressources Web
  (hébergeur), **toujours déclenchée par l'utilisateur** ;
- synchronisation optionnelle avec un serveur (CloudSync) : remontée d'état et
  de configuration, application de changements de configuration demandés
  depuis un espace utilisateur Web.

**Principe directeur, non négociable** : l'arrosage et les sécurités
fonctionnent sans Wi-Fi, sans Internet, sans serveur et sans carte SD. Tout le
reste est une couche optionnelle.

---

## 3. Matériel

### 3.1 Deux plates-formes

| | Historique « CYD » | Production actuelle |
|---|---|---|
| Carte | ESP32-2432S028 | Guition **JC4827W543C_I** (ESP32-S3) |
| Flash / PSRAM | 4 Mo / aucune | 4 Mo utilisés (table commune) / PSRAM OPI 8 Mo |
| Écran | ILI9341 320×240, SPI, `bodmer/TFT_eSPI` | NV3041A **480×272**, QSPI, `GFX Library for Arduino` via l'adaptateur `lib/tft_espi_compat_s3` |
| Tactile | XPT2046 résistif, SPI séparé | GT911 capacitif, I²C dédié (SDA 8, SCL 4) |
| Relais | XL9535 sur I²C SDA 27 / SCL 22 | XL9535 sur **second** bus I²C (`Wire1`, SCL 17 / SDA 18), adresse 0x20 |
| SD | lecteur intégré | SPI dédié (MISO 13, MOSI 11, SCK 12, CS 10) |
| Voyant | LED RGB embarquée (LEDC) | ruban **WS2812** externe sur GPIO 46 (luminosité plafonnée à 40) |
| Env PlatformIO | `ProgrammeArrosage` (alias `_legacy`, `_v4`) | `ProgrammeArrosage_s3` |

Le brochage S3 est centralisé dans la section `[jc4827w543c_i]` de
`platformio.ini` (macros `AQ_S3_*`), chaque broche annotée de sa date de
validation sur matériel. L'adaptateur `tft_espi_compat_s3` permet à
`DisplayManager.cpp` de compiler sans modification sur les deux cartes ; les
dimensions d'écran passent par `ScreenGeometry.h` (plus de 320/240 en dur).

La CYD n'est plus la cible de développement. Sa compilation n'a pas été
revérifiée dans le cadre de ce document.

### 3.2 Modules en service sur le réseau du propriétaire

| Adresse | Rôle | Remarques |
|---|---|---|
| `192.168.1.141` | **banc de développement principal**, ESP32-S3, COM4 | 5 zones avec **vannes réelles câblées** depuis le 26 sept. — un flash ou un test a des conséquences physiques. Alimentation 5 V externe. Antenne U.FL au contact marginal (voir §9). Module déclaré `Jardin-01` sur le serveur cloud. |
| `192.168.1.156` | firmware ancien | Possiblement l'unité qui arrose réellement. **Ne rien y écrire sans accord.** Se distingue de .141 par l'absence de `bootGuard` dans `/api/adminStatus`. |
| — | ESP32-S3-WROOM-1 **N16R8** nu | banc parallèle sans écran ni relais (SD seule), env `ProgrammeArrosage_s3_n16r8` créé mais **jamais flashé** ; câblage SD communiqué, non confirmé. |

### 3.3 Alimentation — à connaître avant de chercher un bug

Les redémarrages et les « deux relais qui collent ensemble » observés sur le
banc ont été, à plusieurs reprises, des **chutes de tension** (pointes Wi-Fi /
TLS sur l'USB du PC ; appel de courant à l'ouverture d'une vanne), pas des
bugs. Réflexe : lire `resetReason` dans `/api/diagnostics` **avant** de lire le
code. `mise sous tension` / `brownout` = matériel ; `panic` / `watchdog` =
logiciel.

---

## 4. Architecture logicielle

Framework Arduino sur `espressif32 @ 6.13.0`. Environ 31 000 lignes dans
`src/` (hors `src/domain/`). Le code est commenté en français, souvent avec la
date et la raison de chaque décision : **lire les commentaires avant de
modifier**, ils contiennent l'historique des incidents.

### 4.1 Carte des composants

| Domaine | Fichiers principaux | Rôle |
|---|---|---|
| Point d'entrée | `main.cpp`, `MaintenanceSetupWrapper.cpp`, `DisplaySplashWrap.cpp` | `setup()` est **enveloppé** à l'édition de liens (`-Wl,--wrap=_Z5setupv`) pour permettre le démarrage en mode maintenance avant l'application normale. Câble le callback relais `onRelayRequest()`. |
| Planification | `ScheduleManager`, `RainSchedule`, `PausedWateringStore` | Décide *quand* arroser ; ne touche jamais le matériel (callback). |
| Moteur d'exécution V4 | `src/domain/*`, `EquipmentManager`, `EquipmentOrchestrator`, `EquipmentOutputRuntimeAdapter`, `EquipmentExecutionShadowRuntime`, `EquipmentRuntimeConfig*` | Modèle zones → équipements → cartes → ports → voies ; pilotes `I2cExpanderBinaryActuatorDriver` (XL9535, MCP23017), `GpioBinaryActuatorDriver`, `SimulatedBinaryActuatorDriver`. |
| Topologie relais | `RelaisManager`, `RelayTopology`, `RelayTopologyStore`, `IoExpanderManager`, `IoExpanderConfig` | Table de câblage persistée (sorties **et** entrées TOR), logique directe/inverse. |
| Entrées et scripts | `InputSampler`, `domain/ScriptVm`, `ScriptRunner`, `ScriptStore`, `ScriptHostRuntime`, `ScriptMessageCatalogue`, `ScriptVmSelfTest` | Entrées avec anti-rebond ; VM à pile à budget d'instructions par tour ; état d'attente sérialisable (survit au redémarrage) ; bytecode validé avant stockage. |
| Configuration | `ConfigManager` | Propriétaire unique de la NVS et du montage LittleFS. Blob versionné (**schéma 5**, `CFG_NVS_SCHEMA`), migrations 1→5 dans `load()`. |
| Stockage | `StorageManager`, `SdStaticHandler`, `WebAssetsUpdater` | SD (SdFat) ; ressources Web servies depuis `/www` sur SD, repli LittleFS ; récupération SD en tâche dédiée. |
| Web | `WebManager`, `ApiAuth`, `data/*` | ESPAsyncWebServer (patché au build, §4.6). |
| Affichage | `DisplayManager` (4 100 lignes), `DisplayPlanningDecor`, `ScreenManager`, `Theme`, `StatusLed` | Écrans LCD, veille, voyant LED / WS2812. |
| Réseau | `WiFiManager`, `NTPManager`, `WeatherManager`, `NotificationManager`, `CloudSync` | Tâches réseau dédiées (§4.2). |
| Mise à jour | `MaintenanceBoot`, `MaintenanceRequest/Result`, `OtaStageUpdate`, `OtaPartitionWriter`, `OtaTlsTrust`, `OtaBootGuard`, `UpdateCheckScheduler`, `OtaBuildIdentity.h`, `BuildInfo.h` | OTA firmware, mise à jour des ressources Web, vérification périodique. |
| Résilience | `BootLoopGuard`, `FaultManager`, `IncidentManager`, `EventLog`, `EventLogCatalogue` | Mode dégradé après boucle de redémarrages, défauts actifs, journal. |
| Observabilité | `SystemDiagnostics`, `RuntimeProfiler`, `HeapMetrics` | `/api/diagnostics`, `/api/health`, profileur par composant de boucle. |

Des guides par composant existent dans `docs/firmware/FW-0xx_*.md` (état
juillet, structure toujours pertinente) et des recettes « comment ajouter… »
dans `docs/developer/DEV-0xx_*.md`.

### 4.2 Exécution : boucle, tâches et cœurs

`loopTask` (`setup()`/`loop()`) tourne sur le **cœur 1** et porte l'écran, le
planificateur, les relais, le Web côté applicatif. Le **cœur 0** est réservé à
la pile Wi-Fi/lwIP.

| Tâche | Cœur | Prio | Rôle |
|---|---:|---:|---|
| `loopTask` | 1 | 1 | boucle applicative |
| `cloud-sync` | 1 | 1 | cycle CloudSync (TLS bloquant 2,5-4,6 s) |
| `weather-fetch` | 1 | 1 | météo toutes les 2 h (~16 Ko JSON) |
| `wifi-keepalive` | 1 | 1 | sonde de connexion zombie, 45 s |
| AsyncTCP (lib) | **0** | 10 | serveur Web — épinglé au cœur 0 le 28 sept. (`-DCONFIG_ASYNC_TCP_RUNNING_CORE=0`) |
| `notify-*`, `sd-recovery`, `ota-tls-probe`, `aqualook-maint` | 0 | 1 | ponctuel |

Tableau complet et justification : `docs/engineering/15_RUNTIME_AND_PROFILING.md`.
**Règle** : aucune opération bloquante dans `loop()` ; tout travail réseau long
va dans une tâche dédiée. Ne **jamais** mettre de travail CPU lourd (parsing,
crypto) sur le cœur 0 : trois plantages `task_wdt` en août en sont venus.

### 4.3 Chaîne de commande d'une vanne

```
ScheduleManager / ScriptRunner / commande manuelle
   └─> callback onRelayRequest(zone, état)          (main.cpp)
        └─> EquipmentOutputRuntimeAdapter / V4 physical backend
             └─> pilote I2cExpanderBinaryActuatorDriver (une instance par carte)
                  └─> I2cExpanderSharedOutputState (propriétaire unique du registre de sortie)
```

Invariants : le planificateur ne pilote jamais le matériel ; la durée maximale
de sécurité ne se retire pas ; une zone sans sortie affectée est signalée
partout (hachures LCD, trame Web) plutôt qu'ignorée ; un échec de pilotage est
un échec (plus de repli silencieux depuis la suppression du moteur
historique). Détail : `docs/engineering/08_RELAY_AND_EQUIPMENT_CONTROL.md`
(tenu à jour en septembre).

### 4.4 Persistance et partitions

`aqualook_partitions.csv` (identique CYD et S3) :

| Partition | Offset | Taille | Remarque |
|---|---|---|---|
| otadata | 0xE000 | 8 Kio | |
| app0 / app1 | 0x10000 / 0x1F0000 | 1 920 Kio chacun | firmware S3 ≈ 1,49 Mo (76 %) |
| nvs | 0x3D0000 | 84 Kio | agrandie le 16 août après saturation |
| spiffs (LittleFS) | 0x3E5000 | 108 Kio | ne contient que `splash.jpg` |

**Une table de partitions ne se déploie pas par OTA** : un module livré avant
le 16 août garde l'ancienne NVS de 20 Kio jusqu'à un reflash USB.

NVS : un blob de configuration principal réécrit en entier à chaque
changement, plus les clés de topologie, scripts, pauses, jetons. Elle contient
des **secrets non reconstituables** (jeton d'appairage cloud dont le serveur ne
garde que l'empreinte). Avant toute migration de schéma :
`python tools/nvs_backup.py save COM4 avant-maj.bin` (`restore` pour revenir).
Toute nouvelle taille de structure doit être ajoutée à la garde de longueur
de `ConfigManager::load()` — l'oubli a déjà effacé une configuration.

`littlefs/` = `data_dir` PlatformIO, réservé aux secours indispensables.
`data/` = ressources Web complètes, copiées sur SD (`/www`).

### 4.5 Interface Web

Pages (`data/`) : `index.html` (accueil + tiroir de paramètres), `setup.html`
(portail captif), `sante.html`, `logs.html`, `diagnostic.html`, `ota.html`,
`scripts.html` (éditeur + compilateur `script-lang.js`), `cloudsync.html`,
`urls.html`. Script principal `app.js`, signature des écritures `hmac.js`.

Routes principales (`WebManager.cpp`, et quelques-unes définies en ligne dans
`WebManager.h`, dont `/api/health`) : `/api/status`, `/api/zonesConfig`,
`/api/forecast` (séparées de `/api/status` pour alléger), `/api/zone`,
`/api/display`, `/api/adminStatus`, `/api/diagnostics`, `/api/health`,
`/api/logs`, `/api/logs.txt`, `/api/log-messages`, `/api/relay/topology[/persist|/reset]`,
`/api/relay/input`, `/api/io`, `/api/scripts`, `/api/script-one`,
`/api/script-source`, `/api/script-messages`, `/api/webassets/update`,
`/api/wifi/scan`, `/api/bootguard/clear`, `/api/resetConfig`, routes
`/api/debug/*`, et les URL de détection de portail captif.

Règles : ne pas renommer une route ni un ID HTML utilisé par `app.js` ; après
un POST qui change l'affichage, lever `EventBus::displayDirty` ; répondre au
client **avant** un redémarrage.

### 4.6 Bibliothèques patchées au build

`tools/patch_asyncwebserver.py` (en `extra_scripts`) patche à la source
ESPAsyncWebServer et AsyncTCP dans `.pio/` (idempotent, marqueurs
`AQUALOOK_*`) : plafond 8 Ko sur ligne/en-tête (DoS URI), bornage du corps au
`Content-Length` (débordement de tas **critique** trouvé par fuzzing), garde
null à l'acceptation, purge des événements à la destruction d'un client. Un
build qui échoue juste après un changement de bibliothèque **réussit au second
passage** : ce n'est pas une vraie erreur. Rapport :
`docs/ROBUSTESSE_RESEAU_2026-09-04.md`, outils : `tools/robustesse/`.

### 4.7 Mises à jour (trois canaux, une seule page `/ota`)

- **Firmware** : manifeste GitHub Releases, téléchargement HTTPS avec chaîne de
  confiance embarquée (`OtaTlsTrust`, plus de `setInsecure()`), SHA-256,
  écriture de la partition inactive, bascule, **garde de retour arrière**
  (validation après 45 s stables, retour automatique après 3 échecs).
  Release : pousser un tag `vX.Y.Z` → workflow `.github/workflows/ota-release.yml`.
  Procédure : `docs/ota/RELEASE_PROCESS.md`.
- **Ressources Web** : manifeste `aqualook-web-manifest.json` sur l'hébergeur,
  fichiers sous `/web/v<version>/`, écriture sur SD. Préparation :
  `python tools/publish_web_assets.py <version>` (produit `dist/alwaysdata`,
  vérifie ses propres empreintes), **dépôt FTP manuel**, puis contrôle
  `python tools/check_published_web.py <url-manifeste>`.
- **Vérification périodique** (`UpdateCheckScheduler`) : voyant violet,
  marqueur « MAJ DISPO » au bandeau LCD, badge Web.

**Invariant** : un serveur distant ne déclenche **jamais** une mise à jour.
CloudSync n'accepte qu'un seul type de commande, `config.apply` ; tout autre
type est refusé. Ne pas proposer `ota.*` ou `webassets.*` : décision définitive
du propriétaire.

### 4.8 Cloud

- **Côté module** : `CloudSync.cpp`. Le module est toujours à l'initiative
  (pas de connexion entrante, pas de courtier MQTT) : toutes les
  `intervalMinutes` (5 min en service), `POST /v1/report` puis
  `GET /v1/pending-command`, jeton porteur par module, HTTPS imposé hors LAN,
  verrouillage optimiste par `baseRevision`. Config renvoyée seulement si elle
  a changé. Plafond applicatif de 25 s contre les blocages TLS, timeout socket
  4 s, recul vers l'intervalle nominal après 3 échecs ; défaut
  `FaultId::CLOUD_SYNC` levé au 3ᵉ échec consécutif, effacé au premier succès.
- **Côté serveur, en service** : `cloud/php-mutualized/` (PHP + MySQL) sur
  **AlwaysData**, `https://aqualook.alwaysdata.net` — sert à la fois l'API
  (`/health`, `/v1/*`, `/admin/*`), la console d'administration
  (`admin.html`), l'espace utilisateur (`app.html` : comptes, sessions,
  planning hebdomadaire, sauvegarde d'installation) et les ressources Web du
  module. Base `aqualook_tst`, hôte `mysql-aqualook.alwaysdata.net:3306`.
  Configuration dans un `.env` à la racine du site (non versionné ; les
  variables du panneau AlwaysData n'atteignent pas PHP). Schémas :
  `schema.sql`, `schema-v2-comptes.sql`, `schema-v3-sauvegarde.sql`.
- `cloud/api` (FastAPI), `cloud/bridge`, `cloud/mosquitto`, `cloud/caddy`,
  `docker-compose.yml` : piste VPS/MQTT **non déployée**, conservée comme
  référence du même contrat d'API.

Architecture : `docs/architecture/CLOUD_REMOTE_CONFIG.md`,
`SYSTEM_ARCHITECTURE.md` §5, `CLOUD_ENVIRONMENT_EVALUATION.md`.

### 4.9 Démarrage, résilience, identité

- Identité de build unique : `VERSION` → `tools/version_build.py` →
  `AQUALOOK_VERSION`, numéro de build, SHA, branche, env (`BuildInfo.h`,
  `OtaBuildIdentity.h`). Ne jamais écrire une version en dur ailleurs.
- Splash et série affichent étape par étape la progression du boot et un
  bilan final (exigence `AGENTS.md`).
- `BootLoopGuard` : après une boucle de redémarrages, mode dégradé (services
  suspects suspendus), levé seul après avoir testé le suspect.
- L'horloge survit à un redémarrage logiciel : l'arrosage reprend sans
  attendre NTP.

---

## 5. Build, flash, observation

### 5.1 Environnements utiles

| Env | Usage |
|---|---|
| `ProgrammeArrosage_s3` | **production**, module .141 (COM4 inscrit dans l'env) |
| `ProgrammeArrosage_s3_n16r8` | banc N16R8 sans écran (non commité, port à préciser) |
| `ProgrammeArrosage` (= `_legacy` = `_v4`) | carte CYD historique ; seul env compilé par la CI `platformio-v4-domain.yml` |
| `test_cloudsync_probe_s3*` (6 variantes) | bancs isolés de la campagne CloudSync |
| `test_*_s3`, `test_relais`, `test_execution_engine`, `calibration` | bancs matériels unitaires jetables |

Les fichiers `test_*.cpp` définissent chacun `setup()`/`loop()` : tout nouveau
banc doit être **exclu** du `build_src_filter` de `[env:ProgrammeArrosage]`
(hérité par les envs S3), sinon le build de production échoue.

### 5.2 Chaîne courante (S3)

```powershell
git diff --check
pio run -e ProgrammeArrosage_s3
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

- Toujours **confirmer le port COM** de la machine courante (`pio device list`).
- `--dtr 1` est **obligatoire** sur le S3 (USB natif) : sans lui le moniteur
  reste muet et l'on croit le module silencieux. Le port se ré-énumère à
  chaque redémarrage ; si une capture cesse de grossir, relancer le moniteur
  avant de conclure.
- Fermer le moniteur avant un upload (il tient le port).
- Un build S3 complet prend environ 4 minutes.
- Après modification de `littlefs/` : `pio run -e ProgrammeArrosage_s3 -t buildfs`.
- Si la carte CYD doit rester supportée : `pio run -e ProgrammeArrosage`.
- Avant un flash sur .141 pendant une campagne de soak :
  `python tools/soak/announce_reboot.py`.

### 5.3 Observer un module

- **Série en priorité** (`logs/device-monitor-*.log`, ou
  `tools/robustesse/serial_watch.py`) : le tampon `/api/logs` est en RAM et se
  vide à chaque redémarrage de maintenance, précisément quand il se passe
  quelque chose.
- `/api/diagnostics` : `resetReason`, `wifi.rssi`, heap interne,
  `loop.overrunCount`, `runtimeComponents.<composant>` (`lastUs`, `maxUs`,
  `slowCount`, `schedulerSuspectCount`) — premier réflexe pour toute
  lenteur.
- `/api/health` et la page Santé (Web et LCD) : défauts actifs consolidés.
- `/api/adminStatus` : `bootGuard`, `updateCheck`, `cloudSync.lastSyncOk`.
- Décoder un plantage : `xtensa-esp32s3-elf-addr2line -pfiaC -e .pio/build/ProgrammeArrosage_s3/firmware.elf <adresses>` (pas de partition core dump, faute de place).

### 5.4 Contrôles disponibles

- CI GitHub : `platformio-v4-domain.yml` (build CYD), `security-contracts.yml`
  (`tests/contracts/test_security_contracts.py`), `ota-release.yml`.
  Elles ne se déclenchent que sur `main` ou sur une pull request vers `main` :
  aucun des travaux de `feat/moteur-de-regles` n'y est encore passé. Ouvrir une
  PR de cette branche vers `main` suffit à les lancer. `materialize-run6-22.yml`
  et le déclencheur `push` de `platformio-v4-domain.yml` visent des branches
  qui n'existent plus (workflows morts, à retirer ou réorienter).
- `tools/check_inline_js.py` : **ne détecte pas** une apostrophe non échappée
  dans une chaîne JS (l'interface entière s'affiche vide dans ce cas). Il n'y
  a ni Node ni test JS sur le poste : relire la ligne modifiée.
- `tools/apercu_web/` : rendu des pages avec données factices.
- `ScriptVmSelfTest` + `/api/debug/script-selftest` : autotest de la VM sur cible.
- Soak V4 : `tools/soak/check_soak.py` (critères :
  `docs/architecture/AQUALOOK_V4_CRITERE_FIABILITE.md`).

---

## 6. Règles de travail du projet

Héritées d'`AGENTS.md` et de la pratique, toujours en vigueur :

- `main` est stable ; travail sur branche `feat/`, `fix/`, `docs/`… ;
- **un commit par modification validée** (compilée **et** vérifiée sur
  matériel quand elle touche l'embarqué) : chaque commit est un point de
  retour ;
- « compile » n'est pas « validé » : prouver que le code est appelé et produit
  l'effet observable, dans tous les modes concernés ; si un mécanisme a un
  repli, **journaliser le chemin réellement emprunté** ;
- modification minimale, pas de reformatage global, pas de renommage de
  route/ID/structure persistée sans décision documentée ;
- tests matériels relais : une zone, durée courte, sous surveillance ;
- `checkpoint` (mot exact) déclenche la procédure de `AGENTS.md` (document dans
  `docs/checkpoints/`, ZIP, SHA-256) ;
- documenter en même temps que le code ; chaque incident significatif est
  consigné dans un checkpoint ou un chapitre du manuel.

---

## 7. État fonctionnel au 5 octobre 2026

### 7.1 Réalisé et validé sur matériel

| Chantier | Période | Références |
|---|---|---|
| Ressources Web sur SD avec repli LittleFS | août | `ROADMAP.md` |
| OTA firmware complet avec retour arrière, release v5.9.6 / v5.9.7 de bout en bout | 13-17 août | `docs/ota/`, `docs/engineering/21_OTA.md` (partiellement à jour) |
| Mise à jour des ressources Web à distance, page `/ota` unifiée | 18-31 août | `CHECKPOINT_2026-08-18_*` |
| Mode dégradé après boucle de redémarrages | 17 août, 25 sept. | `CHECKPOINT_2026-08-17_*` |
| Portage ESP32-S3 JC4827W543C_I (écran, tactile, SD, relais, rétroéclairage, WS2812, mise en page 480×272) | 27 août - sept. | `docs/architecture/HW_JC4827W543_PORT_IMPACT.md`, `ROADMAP.md` |
| Serveur cloud PHP sur AlwaysData, configuration à distance, espace utilisateur | 18 août - 3 sept. | `cloud/php-mutualized/README.md` |
| Open-Meteo (Météo-France) comme second fournisseur | 3 sept. | — |
| Notifications ntfy en HTTPS | 4 sept. | — |
| Durcissement réseau (fuzzing, 3 défauts de bibliothèque corrigés) | 4-6 sept. | `docs/ROBUSTESSE_RESEAU_2026-09-04.md` |
| Moteur V4 seul, multi-cartes (XL9535 + MCP23017), câblage éditable | 4-8 sept. | `CHECKPOINT_2026-09-05_*`, `08_RELAY_AND_EQUIPMENT_CONTROL.md` |
| Entrées TOR, moteur de scripts, éditeur, catalogue de phrases, signature des envois | 8-10 sept., 27 sept. | commentaires de `ScriptVm.h`, `ScriptRunner.cpp` |
| Page Santé (Web + LCD), `/api/health` | 25 sept. | — |
| Journal : 52 messages techniques réécrits pour être actionnables | 26-27 sept. | `docs/roadmap/ROADMAP_EVENTLOG_MESSAGES_INVENTORY.md` |
| Gel chronique de boucle ~1/s : cause dominante (AsyncTCP non épinglé) corrigée | 28 sept. | `15_RUNTIME_AND_PROFILING.md` |
| CloudSync : plafond 25 s, recul après échecs, correction d'un use-after-free | 28-29 sept. | `18_NETWORK_AND_WIFI.md` |

### 7.2 Préparé, non validé

- Banc N16R8 sans écran (`d83f0d5`) et badge « Aqualook / Aqualook Pro »
  (`5e3f512`) : commités, compilés, **jamais flashés**.
- Purge programmée de l'historique cloud, `cloud/php-mutualized/cleanup.php`
  (`1a7d4b6`, script CLI pour tâche planifiée AlwaysData) : commitée ;
  déploiement FTP et création de la tâche planifiée non confirmés.
- Après la campagne CloudSync : fichiers `app.html`/`auth.php`/`index.php`
  de l'espace utilisateur à redéployer en FTP (non confirmé).

### 7.3 Non commencé, mais demandé

- Écran LCD dédié à CloudSync (`Screen::CLOUDSYNC`).
- Phase B du portage S3 : refonte de l'interface LCD pour le 480×272
  (disposition globale, boutons jugés « sommaires ») — travail de conception,
  écran par écran.
- Badge de gamme produit sur les pages autres que `index.html` (chaque page
  duplique son en-tête).
- Voir `ROADMAP.md` pour la suite produit (mode point d'accès autonome,
  mesure de débit, humidité du sol, recommandations, première mise en
  service).

---

## 8. Problèmes ouverts

Classés par ordre d'impact estimé.

1. **Gel résiduel de la boucle sur le cœur 1 (jusqu'à ~560 ms).** Après la
   correction AsyncTCP, il reste environ un dépassement toutes les 30 s au
   repos, et nettement plus pendant une activité Web soutenue. Le pic tombe
   le plus souvent sur `DISPLAY_SLEEP_TOUCH` (lecture GT911 pendant la veille
   écran), mais touche aussi d'autres composants : c'est un blocage global
   d'ordonnancement, pas un coût propre au tactile. Élever la priorité de
   `loopTask` à 2 a été **essayé puis retiré** (pic inchangé et CloudSync
   dégradé) — ne pas le retenter tel quel. Pistes : instrumenter l'intérieur
   de `getTouchPoint()` pour séparer I²C et attente ; observer ce qui tourne
   sur le cœur 1 au moment d'un pic **sans écriture série** (effet
   observateur constaté). Le RSSI dégradé du banc (§8.4) est un facteur
   aggravant constaté, pas une cause démontrée.
   → `docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md`

2. **CloudSync : échecs isolés environ deux fois plus fréquents pendant une
   activité Web** (1/67 min au repos, 1/31 min en édition), cohérent avec le
   point 1 puisque `cloud-sync` partage le cœur 1. Effet visible : des
   groupes de 3 échecs lèvent le défaut `CLOUD_SYNC` puis se résorbent seuls.
   À vérifier avant de corriger : l'interface exige-t-elle un acquittement
   manuel d'un défaut déjà effacé ? Pistes : distinguer « s'est produit et
   résolu » de « actif » ; traiter la cause (point 1) ; en dernier lieu,
   revoir le seuil de 3. La panne totale de ~22 h 40 du 29 sept.-1er oct. est
   attribuée à un état transitoire côté hébergeur/réseau (huit bancs
   d'isolation sans reproduction, reprise sans changement de code) — non
   confirmé faute d'accès aux journaux serveur.

3. **AsyncTCP 3.3.2 : course au cycle de vie des connexions.** Une rafale de
   connexions coupées par RST peut provoquer un `assert` lwIP
   (`tools/robustesse/repro_error_uaf.py`) et un redémarrage. Pas de version
   plus récente ; empiler des gardes recule le seuil sans corriger. Le
   redémarrage avec configuration préservée est, à ce jour, la résilience
   retenue. Ne **pas** désactiver le chien de garde d'`async_tcp` : testé, cela
   transforme un redémarrage en gel total.

4. **Antenne du banc .141.** Contact U.FL marginal : RSSI qui tombe de −60 à
   −90 dBm après certains reconnects. Débrancher/rebrancher le connecteur a
   corrigé le cas le plus net. Toujours relever `wifi.rssi` avant
   d'interpréter un taux d'échec réseau.

5. **Sécurité de l'interface locale** (§1, point 5) et limitation de débit des
   routes sensibles.

6. **Première mise en service d'un module neuf** : la table de partitions et
   certains prérequis ne passent pas par OTA. Chapitre ouvert dans
   `ROADMAP.md`, non rédigé.

7. **Défaut de bibliothèque ESPAsyncWebServer** : pages HTML tronquées sous un
   `Content-Length` complet — contourné côté application, non corrigé à la
   source (`ROADMAP.md`).

---

## 9. Dette technique identifiée

- **Octets NUL bruts dans trois sources commitées** : `src/WebManager.cpp:2095`,
  `src/CloudSync.cpp:481`, `src/NotificationManager.cpp:364` contiennent un
  littéral `'\0'` écrit comme octet NUL réel (séquelle d'une génération par
  heredoc). Le code est correct, mais **Git traite ces fichiers comme
  binaires** : `git diff`, `git log -p` et `grep` n'en montrent plus le
  contenu. Corriger en remplaçant l'octet par `'\0'` (trois éditions
  triviales, à vérifier par `git diff` redevenu textuel).
- **Fins de ligne mélangées** (CRLF dans l'arbre de travail pour `data/*`,
  normalisés LF à la publication) : source d'avertissements Git ;
  `.gitattributes` à compléter.
- **`platformio.ini` de 1 300 lignes**, dont une majorité de bancs jetables et
  des commentaires périmés (« le profil nominal reste explicitement LEGACY »).
  `_legacy` et `_v4` n'ont plus de raison d'être.
- **Racine du dépôt encombrée** : `build_run_*.log`, `*.patch`,
  `platformio.run7-2.ini`, `platformio_ota.ini`, plusieurs `README_*.md`,
  `src-V0/`, `backups/`, `tools/run6-22`…`run7-9` (scripts d'application de
  patchs d'anciens chantiers). À trier, sans rien supprimer d'utile à
  l'historique.
- **`DisplayManager.cpp` (4 100 lignes) et `WebManager.cpp` (2 900 lignes)** :
  candidats à un découpage par écran / par famille de routes.
- **`WiFiManager`** : seize correctifs successifs sans refonte. Le propriétaire
  préfère une réécriture propre à un correctif de plus quand un module montre
  ce profil.
- **Pages Web sans en-tête commun** : chaque page duplique son bandeau.
- **Pas de test automatisé du JavaScript** ni de test natif du domaine V4 en
  CI au-delà de la compilation.

---

## 10. Invariants et décisions à ne pas rediscuter sans raison nouvelle

1. L'arrosage local ne dépend ni du réseau, ni du serveur, ni de la SD.
2. Le planificateur ne commande jamais directement une sortie.
3. La durée maximale de sécurité des relais est intangible.
4. Une défaillance de capteur ou d'extension ne provoque jamais une activation.
5. Toute évolution de la NVS est versionnée, migrée, et ajoutée à la garde de
   longueur ; sauvegarde NVS avant de flasher une migration sur un module qui
   compte.
6. Un serveur distant ne déclenche jamais une mise à jour ; le module assiste,
   l'utilisateur déclenche. Seul `config.apply` est accepté du cloud.
7. Le module est toujours à l'initiative des échanges avec le serveur (pas de
   courtier permanent : MQTT écarté le 16 août pour l'hébergement mutualisé).
8. Un échec de pilotage est un échec : pas de repli silencieux.
9. Un message de journal ne doit pas pouvoir devenir faux (compter plutôt
   qu'affirmer).
10. Pas de délai esthétique non borné au démarrage ; jamais de retard sur une
    action de sécurité.
11. Les sprites d'affichage ne se libèrent pas pour gagner de la mémoire (déjà
    essayé : régression tactile et portail captif).

Registre complet : `docs/codex/03_INVARIANTS.md` et les sections « Invariants »
des chapitres `08`, `15`, `18` du manuel (identifiants `INV-*`).

---

## 11. Pièges connus

- **Recherche récursive depuis la racine** : `.pio/` (~830 Mo) est synchronisé
  par OneDrive ; un `grep -r .` le fait retélécharger. Restreindre à `src/
  include/ data/ cloud/ tools/ docs/`. À terme, sortir le dépôt d'OneDrive.
- **Génération de code par heredoc** : un niveau de `\` est perdu (`"\n"`
  devient un vrai saut de ligne, `'\0'` un octet NUL) ; en JS, une apostrophe
  non échappée vide toute l'interface. Préférer une édition directe et
  l'apostrophe typographique (’) dans les chaînes affichées.
- **Cache de build incrémental** : un build peut réussir en masquant une erreur
  ; en cas de doute, `pio run -t clean` avant de conclure.
- **Silence série ≠ gel** : vérifier `--dtr 1`, l'énumération du port et
  `resetReason`.
- **Mauvaise carte** : vérifier l'adresse et la présence de `bootGuard` avant
  toute écriture (`/api/debug/deploy-*`, `/api/webassets/update`, flash).
- **Publication Web** : toujours régénérer le manifeste *après* avoir figé les
  fichiers ; déposer les fichiers *avant* le manifeste ; contrôler avec
  `check_published_web.py`.
- **Ports de développement recopiés en production** (3307 MariaDB portable,
  8000 FastAPI local) : ils produisent un « Connection refused » qui ressemble
  à une panne réseau.
- **Corriger une règle à un seul endroit** : plusieurs chemins de rendu
  coexistent (LCD liste / grille, Web, page Santé) ; vérifier ce qui est
  réellement à l'écran dans la configuration testée.

### Proposition de mise à jour d'`AGENTS.md` (à décider par le propriétaire)

- remplacer la chaîne `legacy` + `v4` par `ProgrammeArrosage_s3` comme cible
  de validation, `ProgrammeArrosage` restant compilé si la CYD est encore
  supportée ;
- retirer la section « Qualification Legacy / V4 », close par la suppression
  du moteur historique ;
- ajouter `--dtr 1 --rts 0` à la commande de moniteur ;
- faire pointer « Source de vérité » vers ce document et le dernier checkpoint.

---

## 12. Carte de la documentation existante

| Emplacement | Contenu | Fraîcheur |
|---|---|---|
| `AGENTS.md` | règles de travail, procédure checkpoint | règles valides ; build obsolète (§1) |
| `docs/START_HERE.md` | ancien point d'entrée | juillet |
| `docs/engineering/00-38` | manuel d'ingénierie par domaine | **juillet** sauf `08` (7 sept.), `15`, `18`, `31` (1er oct.), `38` (27 août) |
| `docs/codex/` | contexte, invariants, règles, build | juin-août ; `00`, `10` décrivent l'état OTA de juillet |
| `docs/architecture/` | modèle V4 (≈70 docs de conception par phase), cloud, portage S3, topologie | conception toujours valable ; état d'avancement à recouper avec §7 |
| `docs/firmware/FW-0xx` | un guide par composant | juillet |
| `docs/developer/DEV-0xx` | recettes d'extension (ajouter une route, un équipement, un défaut…) | juillet, structure valable |
| `docs/ota/` | manifeste et processus de release | août, valable |
| `docs/security/` | architecture et registre des risques | juillet |
| `docs/checkpoints/` | un document par jalon ; le plus récent : `CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md` | — |
| `docs/ROBUSTESSE_RESEAU_2026-09-04.md` | campagne de fuzzing et correctifs | septembre |
| `ROADMAP.md` | évolutions, statut de chaque chantier, incidents | 27 sept., la plus à jour des vues produit |
| `cloud/php-mutualized/README.md` | API serveur en service | septembre |
| Commentaires du code | justification datée de chaque choix | **la source la plus fiable** |

---

## 13. Premiers pas recommandés

**Jour 1 — sécuriser l'existant**

1. Lire ce document, puis le checkpoint du 1er octobre.
2. Ouvrir une pull request `feat/moteur-de-regles` → `main` (après fusion
   des 6 commits propres à `main`) pour faire tourner la CI sur le travail
   récent, sans la fusionner avant validation firmware et LittleFS.
3. Valider sur matériel le banc N16R8 et le badge produit, déjà commités mais
   jamais flashés ; confirmer le déploiement de `cleanup.php`.
4. `pio run -e ProgrammeArrosage_s3` pour confirmer que la base compile sur
   votre poste (environ 4 min ; le premier build télécharge les bibliothèques
   et applique les patchs — relancer une fois s'il échoue).
5. Récupérer les accès hors dépôt : AlwaysData (FTP, MySQL, `.env`), jeton
   administrateur cloud, compte GitHub `cnuma`, sujet ntfy, identifiants
   Wi-Fi du banc. Aucun de ces secrets n'est versionné.

**Semaine 1 — se mettre en position de travailler**

6. Flasher .141 depuis votre poste, ouvrir le moniteur, lire `/api/diagnostics`
   et `/api/health` ; déclencher une zone courte pour voir la chaîne complète.
7. Corriger la dette mécanique sans risque : octets NUL (§9), fins de ligne.
8. Mettre `AGENTS.md`, `docs/START_HERE.md` et `01_PROJECT_STATUS.md` en
   cohérence avec ce document.
9. Choisir le premier chantier de fond : gel du cœur 1 (§8.1) ou
   authentification de l'interface locale (§8.5) avant toute installation
   réelle.
