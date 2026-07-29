# AquaLook Engineering Reference — Diagnostic et observabilité

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/SystemDiagnostics.h`, `src/SystemDiagnostics.cpp`, `src/RuntimeProfiler.*`, `src/WebManager.*`, `src/EventLog.*`
- Composants : diagnostic système, Runtime, Web, mémoire, Wi-Fi, EventLog
- Maturité : D4

## Mission

`SystemDiagnostics` collecte des métriques légères en RAM, sans tâche dédiée ni écriture Flash périodique. Les données sont exposées par `GET /api/diagnostics` et complétées par `RuntimeProfiler::fillJson()`.

## API publique confirmée

```cpp
static void begin();
static void loopEnter();
static void loopExit();
static void noteWebResponse(const char* uri,
                            uint16_t statusCode,
                            size_t responseBytes,
                            uint32_t generationUs);
static void fillJson(JsonDocument& doc, const WiFiManager* wifi);
```

Seuils :

```cpp
LOOP_OVERRUN_THRESHOLD_US = 100000;
LOOP_OVERRUN_LOG_INTERVAL_MS = 5000;
```

Une boucle dépassant 100 ms incrémente le compteur d’overruns. Le log d’avertissement est limité à une occurrence toutes les 5 secondes afin d’éviter une tempête de logs.

## Instrumentation du Runtime

`src/main.cpp` appelle `SystemDiagnostics::loopEnter()` au début de `loop()` et `loopExit()` à la fin. `loopExit()` mesure le temps mural avec `micros()`, maintient les valeurs courantes, maximales et moyennes, puis journalise les dépassements hors section critique.

## Instrumentation Web

`noteWebResponse()` conserve :

- nombre total de réponses ;
- nombre de statuts `>= 400` ;
- dernière URI ;
- dernier statut ;
- taille de la dernière réponse ;
- dernière et maximale durée de génération ;
- âge de la dernière réponse.

## Schéma JSON confirmé

`SystemDiagnostics::fillJson()` produit les objets suivants.

### `system`

```text
uptimeSec
cpuMhz
sdk
chipRevision
resetReason
loopCore
```

### `build`

```text
version
number
gitSha
gitBranch
relayBackend
compiledDate
compiledTime
```

`relayBackend` vaut `legacy` ou `v4` selon les macros de compilation.

### `memory`

```text
heapFree
heapMin
heapLargestBlock
heapSize
psramSize
psramFree
loopStackHighWaterWords
```

### `loop`

```text
count
lastDurationUs
maxDurationUs
lastPeriodUs
maxPeriodUs
ageMs
averageDurationUs
healthy
overrunThresholdUs
overrunCount
lastOverrunUs
lastOverrunAgeMs
cpuStatsAvailable
```

`healthy` est vrai lorsque l’âge du dernier passage de boucle est inférieur à 2 secondes.

### `web`

```text
responses
errors
lastUri
lastStatus
lastBytes
lastGenerationUs
maxGenerationUs
lastAgeMs
```

### `wifi`

```text
state
connected
captive
rssi
ip
channel
mac
```

`RuntimeProfiler::fillJson(doc)` ajoute ensuite ses propres structures au même document.

## EventLog et défauts

- `/api/logs` expose le buffer mémoire `EventLog` ;
- `/api/logs/ack` acquitte les erreurs historiques ;
- `/api/faults` retourne `active`, `unacknowledged` et `mask` ;
- `/logs` fournit une page embarquée de consultation.

Le journal actuel conserve des timestamps relatifs `millis()` uniquement. Il n’embarque pas de timestamp NTP absolu.

## Cause de redémarrage

`resetReasonStr()` traduit notamment : mise sous tension, reset externe, redémarrage logiciel, panic, watchdog, sortie de veille, brownout et reset SDIO.

## Invariants

- `INV-OBS-001` : aucune écriture Flash périodique n’est réalisée par `SystemDiagnostics`.
- `INV-OBS-002` : les métriques sont collectées sous section critique et journalisées hors section critique.
- `INV-OBS-003` : un overrun est qualifié en temps mural, pas en temps CPU strict.
- `INV-OBS-004` : la limitation des logs évite d’aggraver le ralentissement observé.
- `INV-OBS-005` : les diagnostics restent consultables sans microSD.
- `INV-OBS-006` : l’identité du build provient des macros générées, avec valeur `unknown` en absence de génération.

## Limites

- métriques volatiles après redémarrage ;
- aucune rétention historique longue ;
- pas de statistiques CPU FreeRTOS lorsque `configGENERATE_RUN_TIME_STATS` est désactivé ;
- les mesures Web concernent la génération de réponse instrumentée, pas nécessairement le transfert réseau complet ;
- la sortie EventLog série peut influencer le temps mural mesuré.

## Validation

- accès à `/api/diagnostics` ;
- présence et type de chaque champ ;
- build legacy et V4 ;
- test d’overrun supérieur à 100 ms ;
- limitation du log à 5 secondes ;
- requêtes HTTP 2xx et 4xx ;
- comportement sans Wi-Fi et sans SD ;
- cohérence des compteurs Web et du journal ;
- absence de secrets.

## Références

- `src/SystemDiagnostics.h` ;
- `src/SystemDiagnostics.cpp` ;
- `src/RuntimeProfiler.h` et `.cpp` ;
- `src/WebManager.h` et `.cpp` ;
- `src/EventLog.h` et `.cpp` ;
- `docs/engineering/10_TIME_AND_EVENTLOG.md` ;
- `docs/engineering/15_RUNTIME_AND_PROFILING.md`.

## Historique

### 1.1

Consolidation D4 avec API, seuils et schéma JSON réellement produits par le firmware.