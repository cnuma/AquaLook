# AquaLook Engineering Reference — Runtime et profiling

- Version documentaire : 1.2
- Statut : référence reliée au code
- Dernière consolidation : 2026-10-01
- Source de code : `src/main.cpp`, `src/RuntimeProfiler.*`, `src/SystemDiagnostics.*`, `src/WiFiManager.*`, `src/WeatherManager.cpp`, `src/CloudSync.*`
- Composants : `setup()`, `loop()`, Runtime V4, profiler, EventLog, tâches FreeRTOS dédiées
- Maturité : D4

## Mission

Le Runtime est câblé dans `src/main.cpp`. Il initialise les managers dans un ordre déterministe puis exécute une boucle coopérative instrumentée par `RuntimeProfiler`.

## Ordre d’initialisation confirmé

1. série, `FaultManager`, `EventLog`, `SystemDiagnostics` ;
2. bus I²C et scan ;
3. `ConfigManager` et configuration Runtime des équipements ;
4. TFT puis `StorageManager` ;
5. `RelaisManager`, backend legacy ou pilote V4, `EquipmentManager` ;
6. `ScheduleManager`, callback relais et application de la configuration ;
7. Wi-Fi, NTP, Web et météo ;
8. `DisplayManager`.

Les sorties sont initialisées avant l’activation du Scheduler.

## Boucle principale réelle

L’ordre instrumenté dans `loop()` est :

```text
SystemDiagnostics::loopEnter
PRE_LOOP        (BootLoopGuard / OtaBootGuard / configMgr.update())
FAULTS_PRE
STORAGE
WIFI            (WiFiManager::update() -- machine d'etats, jamais bloquant)
NTP             si connecte
WEATHER         (declenchement periodique seulement -- voir plus bas)
SCHEDULE        si NTP synchronise
UPDATE_CHECK
CLOUD_SYNC      (declenchement periodique seulement -- voir plus bas)
EQUIPMENT_SHADOW
INPUT_SAMPLER
SCRIPT_RUNNER
RELAY
IO_EXPANDER
WEB
DISPLAY_MANAGER (+ sous-composants : voir "Sous-mesures de l'affichage")
PLANNING_DECOR
FAULTS_POST
YIELD
SystemDiagnostics::loopExit
```

Liste exacte des composants dans `RuntimeProfiler::Component` (`src/RuntimeProfiler.h`) :
`FAULTS_PRE`, `STORAGE`, `WIFI`, `NTP`, `WEATHER`, `SCHEDULE`, `EQUIPMENT_SHADOW`,
`INPUT_SAMPLER`, `SCRIPT_RUNNER`, `RELAY`, `WEB`, `DISPLAY_MANAGER` (+
`DISPLAY_SCREENMGR`, `DISPLAY_TOUCH`, `DISPLAY_FULLREDRAW`, `DISPLAY_DYNAMIC`,
`DISPLAY_SLEEP_TOUCH`), `PLANNING_DECOR`, `FAULTS_POST`, `YIELD`, `CLOUD_SYNC`,
`UPDATE_CHECK`, `PRE_LOOP`, `IO_EXPANDER`. Chaque nom JSON exposé par
`/api/diagnostics -> runtimeComponents` est produit par
`RuntimeProfiler::componentName()`.

Chaque segment utilise :

```cpp
uint32_t startedUs = RuntimeProfiler::start();
RuntimeProfiler::stop(RuntimeProfiler::Component::<COMPONENT>, startedUs);
```

### Sous-mesures de l'affichage

`DisplayManager::update()` (composant global `DISPLAY_MANAGER`, exposé en JSON
sous `"display"`) se décompose en quatre sous-mesures historiques, posées le
27 septembre 2026 pour localiser un point chaud au-delà de ce que l'agrégat
global permettait de voir :

- `DISPLAY_SCREENMGR` (`"displayScreenMgr"`) — `_screenMgr.update()`,
  inconditionnel à chaque tour ;
- `DISPLAY_TOUCH` (`"displayTouch"`) — `handleTouch()`, throttlé à 80 ms,
  **uniquement emprunté quand l'écran est allumé** ;
- `DISPLAY_FULLREDRAW` (`"displayFullRedraw"`) — `drawXFull()` sur
  `_needsFullRedraw`, rare mais coûteux ;
- `DISPLAY_DYNAMIC` (`"displayDynamic"`) — `updateXDynamic()`, throttlé à
  `_refreshNomMs`/`_refreshActMs`.

**`DISPLAY_SLEEP_TOUCH` (`"displaySleepTouch"`), ajouté le 1er octobre 2026** :
couvre l'appel `getTouchPoint()` fait DIRECTEMENT dans la branche "écran en
veille" de `DisplayManager::update()` (`src/DisplayManager.cpp`, autour de la
ligne 641) — ce chemin n'était couvert par AUCUN composant avant cet ajout,
alors que `DISPLAY_TOUCH` ne mesure que l'appel équivalent fait via
`handleTouch()` quand l'écran est allumé. Mesuré sur matériel de production
(.141) le 1er octobre 2026 : jusqu'à **543 763 µs (543,7 ms)**, très au-delà
du pire cas attendu d'un simple timeout I2C (`Wire.setTimeOut(10)`, 2
transactions par appel ≈ 20-30 ms). Voir "Tâches dédiées" ci-dessous pour
l'explication retenue (contention de planification, pas un blocage I2C
matériel).

## Chaîne de commande des zones

`ScheduleManager` appelle `onRelayRequest(zone, state)`. Cette fonction :

1. construit et soumet un plan au runtime shadow ;
2. appelle `equipmentMgr.startZone()` ou `stopZone()` ;
3. demande un rafraîchissement dynamique si l’action réussit ;
4. journalise l’échec ;
5. utilise `outputAdapter.setZoneValve()` comme repli.

Le mode physique de la pompe reste bloqué ; la configuration est forcée en exécution shadow passive.

## Profiling

Les mesures basées sur `micros()` sont du temps mural. Elles peuvent inclure interruptions, préemptions FreeRTOS et attente système. `yield()` est mesuré mais exclu des alertes de lenteur métier.

## Tâches dédiées et répartition cœur 0 / cœur 1

`loopTask` (celui qui exécute `setup()`/`loop()`) tourne sur le **cœur 1**
(`/api/diagnostics -> system.loopCore`, confirmé constant). Le cœur 0 est
réservé à la pile WiFi/lwIP native de l'IDF ; y placer une tâche qui fait un
travail CPU long (parsing, crypto) l'a déjà fait entrer en contention avec
cette pile, jusqu'au déclenchement du chien de garde (`task_wdt`) — 3
plantages les 16-17 août 2026, cause directe du choix du cœur 1 pour
`weather-fetch` (commentaire détaillé dans `WeatherManager.cpp` autour de la
création de la tâche).

Inventaire complet (audité le 1er octobre 2026, `xTaskCreate*` sur tout
`src/`) :

| Tâche | Fichier:ligne | Cœur | Priorité | Cadence | Nature du travail |
|---|---|---:|---:|---|---|
| `loopTask` | framework Arduino-ESP32, `cores/esp32/main.cpp` | 1 | **2** (voir plus bas) | continu | écran, relais, planificateur, web... |
| `cloud-sync` | `CloudSync.cpp` (`xTaskCreatePinnedToCore`) | 1 | 1 | `_cfg.intervalMinutes` (5-15 min typique), recul à 30 s après changement de config, retombe au nominal après 3 échecs consécutifs (`CLOUD_SYNC_FAILURE_CONFIRMATIONS`, `CloudSync.h`) | TLS bloquant, 2,5-4,6 s mesurés en service |
| `weather-fetch` | `WeatherManager.cpp:285-293` | 1 | `FETCH_TASK_PRIORITY`=1 (`WeatherManager.h:109`) | `OWM_CHECK_INTERVAL_MS`=7 200 000 (2h), retry `FETCH_RETRY_DELAY_MS`=60 000 | HTTP, parsing JSON ~16 Ko |
| `wifi-keepalive` | `WiFiManager.cpp:410-418` | `KEEPALIVE_PROBE_CORE`=1 (`WiFiManager.h:151`) | `KEEPALIVE_PROBE_PRIORITY`=1U (`WiFiManager.h:150`) | `KEEPALIVE_CHECK_INTERVAL_MS`=45 000 | `WiFiClient::connect()` bloquant, timeout 1 s |
| AsyncTCP (bibliothèque) | `platformio.ini` (`-DCONFIG_ASYNC_TCP_RUNNING_CORE=0`) | 0 | 10 (défaut bibliothèque, non modifié) | événementiel | seule tâche de l'inventaire à priorité strictement supérieure à `loopTask` d'origine — isolée au cœur 0 depuis le 28 sept. 2026 pour cette raison |
| `notify-supervisor` | `NotificationManager.cpp:243-246` | `TASK_CORE`=0 (`:45`) | `TASK_PRIORITY`=1U (`:44`) | permanent, poll 1 s (`SUPERVISOR_PERIOD_MS`) | — |
| `notify-sender`, `ota-tls-probe`, `sd-recovery`, `aqualook-maint` | `NotificationManager.cpp`, `OtaTlsProbe.cpp`, `StorageManager.cpp`, `MaintenanceSetupWrapper.cpp` | 0 | 1U | ponctuel (événement) | — |

**Constat central** : avant le 1er octobre 2026, `loopTask` et les trois
tâches réseau du cœur 1 (`cloud-sync`, `weather-fetch`, `wifi-keepalive`)
tournaient toutes à **priorité égale (1)**. FreeRTOS ne protégeait donc pas
`loopTask` pendant qu'une de ces tâches exécutait une portion de travail CPU
non cédée (essentiellement la négociation TLS) — mécanisme de même famille
que le gel de fond causé par AsyncTCP et corrigé le 28 sept. 2026 (voir
l'historique de ce document, section 1.2), mais cette fois porté par les
propres tâches réseau du projet plutôt que par une bibliothèque tierce —
conséquence directe de leur épinglage délibéré au cœur 1.

### Tentative du 1er octobre 2026 : priorité de `loopTask` relevée à 2 — ESSAYÉE PUIS RETIRÉE (résultat négatif)

Hypothèse testée : élever `loopTask` à la priorité 2 (au-dessus des trois
tâches réseau du cœur 1, restées à 1) via `vTaskPrioritySet(NULL, 2);` en
tête de `setup()`, pour que FreeRTOS le préempte systématiquement au lieu
de partager le temps CPU à égalité avec elles.

**Déployée sur `.141` (production) puis RETIRÉE après ~10 minutes
d'observation, pour deux raisons convergentes** :

1. **N'a pas résolu le problème ciblé** : un nouveau pic de 557 761 µs
   (557,8 ms) sur `DISPLAY_SLEEP_TOUCH` a été mesuré malgré le changement
   — magnitude quasi identique au pic de 543,7 ms mesuré AVANT le
   correctif. Si une simple inégalité de priorité FreeRTOS suffisait à
   expliquer le phénomène, ce changement aurait dû l'empêcher. Sa
   persistance suggère une cause différente de celle supposée (contention
   de priorité égale) — piste à reprendre entièrement, pas à raffiner.
2. **A coïncidé avec une dégradation de CloudSync**, stable depuis sa
   reprise (voir `docs/engineering/18_NETWORK_AND_WIFI.md`) : 3 échecs
   consécutifs immédiatement après ce flash, dont un avec la signature
   SÉVÈRE de l'ancienne panne de 22h40 (plafond de sécurité 25 s
   déclenché, 45,7 s de blocage total, erreur mbedTLS -78 / reset par le
   pair) — jamais revue depuis la reprise. Hypothèse retenue mais NON
   formellement prouvée : préempter plus agressivement la tâche
   `cloud-sync` pendant sa poignée de main TLS peut étirer sa durée au
   point de déclencher un reset côté serveur ou réseau.

**Décision** : retrait immédiat par prudence (commentaire explicatif laissé
dans `src/main.cpp` à l'emplacement exact), sans attendre une confirmation
statistique complète — le risque (dégrader une fonctionnalité critique tout
juste rétablie) dépassait le gain non confirmé sur un problème secondaire
(latence tactile). Ne pas retenter cette approche précise sans une
meilleure compréhension de la vraie cause de `DISPLAY_SLEEP_TOUCH` ; elle
n'est PAS, à ce stade, une simple histoire d'inégalité de priorité
FreeRTOS entre tâches de même cœur. **Cause du gel tactile toujours non
résolue au 1er octobre 2026.**

## Invariants

- `INV-RUN-001` : l’ordre des managers dans `loop()` reste explicite et non bloquant.
- `INV-RUN-002` : le Scheduler n’est évalué qu’après `ntpMgr.isSynced()`.
- `INV-RUN-003` : le fallback legacy est conservé tant que la migration V4 n’est pas validée.
- `INV-RUN-004` : les mesures du profiler ne sont pas interprétées comme du temps CPU strict.
- `INV-RUN-005` : `FaultManager::update()` encadre le cycle avant et après les services.
- `INV-RUN-006` : le Runtime shadow ne pilote pas physiquement la pompe.
- `INV-RUN-007` : toute tâche FreeRTOS dédiée créée par un manager (réseau ou
  autre) doit documenter explicitement son cœur, sa priorité et sa cadence
  dans ce document — l'absence de cette information a déjà coûté plusieurs
  nuits de diagnostic (gel AsyncTCP du 28 sept., contention `loopTask` du
  1er oct.).
- `INV-RUN-008` : `loopTask` et les tâches réseau dédiées du cœur 1
  (`cloud-sync`, `weather-fetch`, `wifi-keepalive`) restent TOUTES à
  priorité FreeRTOS égale (1) — une tentative d'élever `loopTask` au-dessus
  d'elles a été essayée et retirée le 1er oct. 2026 (voir ci-dessus),
  n'ayant ni résolu le gel tactile ciblé ni amélioré la fiabilité CloudSync
  (l'a même dégradée). Ne pas réintroduire une inégalité de priorité sur ce
  groupe de tâches sans une cause racine confirmée et une validation longue
  durée explicite.

## Validation

- compilation legacy ;
- compilation et upload V4 ;
- démarrage matériel ;
- diagnostics Runtime ;
- fonctionnement hors réseau ;
- synchronisation NTP sans redraw complet ;
- tests relais sur une zone ;
- observation du profiler sous charge Web et météo ;
- **1er oct. 2026** : campagne d'isolation CloudSync (8 bancs isolés
  successifs sur `.141`, voir le checkpoint de session) ayant conduit à la
  découverte de `DISPLAY_SLEEP_TOUCH`. Tentative de correctif
  `vTaskPrioritySet` déployée puis RETIRÉE le même jour après ~10 min
  (résultat négatif sur les deux plans, voir section dédiée ci-dessus) —
  le gel tactile reste donc NON RÉSOLU, cause racine toujours à trouver.

## Références

- `src/main.cpp` ;
- `src/RuntimeProfiler.h` et `.cpp` ;
- `src/SystemDiagnostics.h` et `.cpp` ;
- `src/WiFiManager.h` et `.cpp` (sonde keepalive, tâche cœur 1) ;
- `src/WeatherManager.cpp` (tâche `weather-fetch`, rationale du choix de cœur) ;
- `src/CloudSync.h` et `.cpp` (tâche `cloud-sync`, `CloudSyncWatchdog`) ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_STEP6_RUN6-26.md` ;
- `docs/engineering/35_CODE_TRACEABILITY_REGISTER.md`.

## Historique

### 1.2

Ajout de l'inventaire complet des tâches FreeRTOS dédiées et de leur
répartition cœur 0 / cœur 1 (`cloud-sync`, `weather-fetch`,
`wifi-keepalive`, AsyncTCP, notifications). Documentation du composant
`DISPLAY_SLEEP_TOUCH` (lecture tactile en veille, jusqu'alors non mesurée,
jusqu'à 543,7 ms mesurés en production). Documentation d'une tentative de
correctif (`vTaskPrioritySet(NULL, 2)`) ESSAYÉE PUIS RETIRÉE le 1er oct.
2026 : n'a résolu ni le gel tactile (pic de 557,8 ms mesuré malgré le
changement) ni amélioré CloudSync (dégradation observée à la place) — gel
tactile toujours NON RÉSOLU à la clôture de cette version. Complète la liste des composants `RuntimeProfiler::Component`
(ajout de `CLOUD_SYNC`, `UPDATE_CHECK`, `PRE_LOOP`, `IO_EXPANDER`, et des
cinq sous-composants `DISPLAY_*`), absents de la version 1.1.

### 1.1

Consolidation D4 avec ordre exact de `setup()`, ordre instrumenté de `loop()` et chaîne réelle de commande.
