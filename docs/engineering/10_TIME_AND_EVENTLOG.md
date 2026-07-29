# AquaLook Engineering Reference — Temps et journalisation

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/EventLog.h`, `src/EventLog.cpp`, `src/NTPManager.*`, `src/WebManager.*`
- Composants : NTP, chronologie relative, `EventLog`, `FaultManager`
- Maturité : D4

## Mission

La gestion du temps fournit l’heure calendaire au Scheduler après synchronisation NTP. `EventLog` fournit un journal mémoire borné et une chronologie relative depuis le boot.

## Correction de référence

Dans l’implémentation actuelle, les entrées `EventLog` stockent uniquement `millis()` dans `LogEntry::ms`. Elles ne basculent pas vers un timestamp absolu après synchronisation NTP. L’heure absolue est utilisée par les composants qui interrogent `NTPManager`, mais elle n’est pas persistée dans chaque entrée du journal.

Toute évolution vers un double horodatage `uptime_ms` + timestamp absolu devra modifier explicitement la structure `LogEntry` et ses consommateurs.

## Modèle de données réel

```cpp
enum LogLevel : uint8_t {
    LOG_INFO = 0,
    LOG_WARN = 1,
    LOG_ERROR = 2
};

static constexpr uint8_t LOG_CAPACITY = 60;
static constexpr uint8_t LOG_MSG_LEN = 72;

struct LogEntry {
    uint32_t ms;
    LogLevel level;
    char msg[LOG_MSG_LEN];
};
```

Le stockage est un buffer circulaire statique de 60 entrées. Lorsque la capacité est atteinte, la plus ancienne entrée est remplacée.

## API publique confirmée

```cpp
static void log(LogLevel level, const char* fmt, ...);
static uint8_t count();
static const LogEntry& get(uint8_t i);
static void clear();
static bool hasErrors();
static void ackErrors();
static void msToHms(uint32_t ms, char* buf, uint8_t len);
static const char* levelStr(LogLevel level);
static uint16_t levelColor(LogLevel level);
```

`get(0)` retourne l’entrée la plus récente.

## Fonctionnement de `log()`

1. formate le message dans un buffer local de 72 caractères ;
2. capture `millis()` ;
3. ajoute l’entrée au buffer circulaire ;
4. positionne `_hasErrors` pour `LOG_ERROR` ;
5. appelle `FaultManager::notifyError()` pour une erreur ;
6. émet la ligne sur `Serial` au format relatif `HH:MM:SS.mmm`.

Préfixes série : `[INF]`, `[WRN]`, `[ERR]`.

## Acquittement et effacement

- `ackErrors()` efface l’alarme historique et appelle `FaultManager::acknowledge()` sans supprimer les entrées ;
- `clear()` vide le buffer, efface l’alarme historique et acquitte `FaultManager` ;
- les défauts encore actifs restent gérés séparément par `FaultManager`.

Routes associées :

| Route | Méthode | Fonction |
|---|---|---|
| `/api/logs` | GET | consultation du buffer |
| `/api/logs/ack` | POST | `EventLog::ackErrors()` |
| `/logs` | GET | page embarquée de consultation |
| `/api/faults` | GET | défauts actifs et non acquittés |

## Politique NTP confirmée

`src/main.cpp` appelle `ntpMgr.update()` uniquement lorsque le Wi-Fi est connecté. Le Scheduler n’est évalué que lorsque `ntpMgr.isSynced()` est vrai.

La configuration NTP vient de `CfgNtp` : serveur, `gmtOffset` et `dstOffset`. La valeur par défaut est `pool.ntp.org`. L’intervalle réel de resynchronisation doit être lu dans `NTPManager` et ne doit pas être déduit d’un document historique.

## Invariants

- `INV-TIME-001` : les événements de boot sont journalisés avant toute synchronisation réseau.
- `INV-TIME-002` : le Scheduler n’utilise l’heure calendaire qu’après `isSynced()`.
- `INV-EVT-001` : le journal mémoire est borné à `LOG_CAPACITY`.
- `INV-EVT-002` : un message est tronqué à `LOG_MSG_LEN - 1` plutôt que d’allouer dynamiquement.
- `INV-EVT-003` : une erreur notifie `FaultManager`.
- `INV-EVT-004` : acquitter une erreur ne supprime pas les défauts encore actifs.
- `INV-EVT-005` : le journal courant est relatif au boot ; aucun timestamp absolu n’est revendiqué.

## Limites connues

- journal volatil en RAM ;
- aucune persistance après redémarrage ;
- aucune source ou code d’événement structuré ;
- messages limités à 71 caractères utiles ;
- `log()` n’utilise pas de section critique ; la sécurité en cas d’appels concurrents doit être évaluée avant multiplication des tâches ;
- sortie série synchrone susceptible d’ajouter du temps mural sous forte rafale.

## Validation

- remplissage au-delà de 60 entrées ;
- ordre récent vers ancien ;
- troncature des messages ;
- `LOG_ERROR` et notification `FaultManager` ;
- acquittement sans effacement ;
- effacement complet ;
- consultation HTTP ;
- journalisation avant Wi-Fi et NTP ;
- vérification d’absence de secrets.

## Références

- `src/EventLog.h` ;
- `src/EventLog.cpp` ;
- `src/FaultManager.h` et `.cpp` ;
- `src/WebManager.h` et `.cpp` ;
- `src/main.cpp` ;
- `docs/engineering/24_DIAGNOSTICS_AND_OBSERVABILITY.md`.

## Historique

### 1.1

Consolidation D4 avec structure, capacité, API, routes et correction de la politique d’horodatage réellement implémentée.