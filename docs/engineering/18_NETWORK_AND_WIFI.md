# AquaLook Engineering Reference — Réseau et Wi-Fi

- Version documentaire : 1.3
- Statut : référence reliée au code
- Dernière consolidation : 2026-10-01
- Source de code : `src/WiFiManager.h`, `src/WiFiManager.cpp`, `src/WebManager.*`, `src/main.cpp`, `src/CloudSync.h`, `src/CloudSync.cpp`
- Composants : Wi-Fi STA, point d’accès, DNS captif, scan asynchrone, sonde de keepalive, Web, NTP et CloudSync
- Maturité : D4

## Mission

`WiFiManager` fournit une machine d’états non bloquante pour la connexion STA, la reconnexion, le portail captif et le scan réseau. Le Scheduler et la chaîne relais restent indépendants de sa disponibilité.

## API publique confirmée

```cpp
void begin(const char* ssid, const char* pwd);
void update();
void startCaptivePortal();
void stopCaptivePortal();
void startScan();
int16_t getScanCount() const;
ScanEntry getScanEntry(uint8_t i) const;
void clearScan();
State getState() const;
bool isConnected() const;
bool isCaptivePortal() const;
IPAddress getIP() const;
IPAddress getApIP() const;
const char* getSsid() const;
int8_t getRssi() const;
const char* stateStr() const;
```

## États C++ réels

- `IDLE` ;
- `CONNECTING` ;
- `CONNECTED` ;
- `DISCONNECTED` ;
- `CAPTIVE_STARTING` ;
- `CAPTIVE_PORTAL`.

Les actions différées internes sont `STA_SET_MODE`, `STA_BEGIN`, `AP_SET_MODE`, `AP_FINALIZE` et `RESTART`.

## Délais et bornes confirmés

| Paramètre | Valeur |
|---|---:|
| timeout de connexion STA | `15000 ms` |
| intervalle entre tentatives | `30000 ms` |
| nombre maximal de tentatives | `5` |
| stabilisation après déconnexion | `100 ms` |
| stabilisation changement de mode | `50 ms` |
| stabilisation AP | `200 ms` |
| délai avant reboot après arrêt AP | `200 ms` |

Tous ces délais utilisent des deadlines basées sur `millis()` et `AquaLook::Time::deadlineReached()` ; aucun `delay()` n’est présent dans le chemin Runtime de `WiFiManager`.

## Séquence STA réelle

1. `begin()` copie SSID et mot de passe, place le Wi-Fi en `WIFI_STA` et désactive l’auto-reconnexion native ;
2. si le SSID est vide, `EventBus::captiveRequested` est positionné ;
3. sinon `startConnection()` déconnecte, passe en `CONNECTING` et programme `STA_SET_MODE` après 100 ms ;
4. `STA_SET_MODE` programme `STA_BEGIN` après 50 ms ;
5. `STA_BEGIN` appelle `WiFi.begin()` ;
6. succès : état `CONNECTED`, compteur remis à zéro, sommeil Wi-Fi désactivé, défaut Wi-Fi levé ;
7. échec dur, SSID absent ou timeout : déconnexion, état `DISCONNECTED`, incrément du compteur ;
8. après 30 s, nouvelle tentative tant que le compteur est inférieur à 5 ;
9. au cinquième échec, `FaultManager::WIFI` est activé et les tentatives automatiques s’arrêtent.

Une perte après connexion ramène immédiatement l’état à `DISCONNECTED`; la prochaine tentative respecte l’intervalle de 30 s.

## Portail captif réel

Le portail utilise :

- SSID AP : `Arrosage-Setup` ;
- DNS : port `53`, wildcard `*` vers l’adresse AP ;
- HTTP : port `80`, route `/setup` et redirections de détection OS.

`startCaptivePortal()` arrête le DNS éventuel, déconnecte la STA puis programme la création de l’AP. `AP_FINALIZE` démarre le DNS et passe à `CAPTIVE_PORTAL`.

`stopCaptivePortal()` arrête DNS et AP, puis programme un redémarrage après 200 ms.

### État de sécurité actuel

Le code appelle `WiFi.softAP(CAPTIVE_AP_SSID)` sans mot de passe. Le point d’accès captif est donc ouvert dans l’implémentation actuelle. Il ne doit pas être présenté comme protégé avant correction du firmware.

## Détection de connexion "zombie" (sonde de keepalive)

**Absente des versions précédentes de ce document alors qu'en place depuis
plusieurs semaines** — rattrapé le 1er oct. 2026.

`WiFi.status()` peut rester bloqué à `WL_CONNECTED` alors que l'association
radio ne répond plus réellement ("zombie"). `WiFiManager` détecte ce cas par
une sonde active, indépendante du simple sondage de statut :

- `checkKeepaliveReachable()` (`WiFiManager.cpp:374-389`), appelée depuis
  `handleConnected()` à chaque tour tant que l'état est `CONNECTED`, est
  elle-même **non bloquante** : elle se contente de vérifier un drapeau
  d'achèvement si une sonde est déjà en vol, et sinon de vérifier
  `now - _lastKeepaliveCheckMs < KEEPALIVE_CHECK_INTERVAL_MS` (45 000 ms) ;
- le travail bloquant réel (`WiFiClient::connect()` vers la passerelle,
  timeout `KEEPALIVE_CHECK_TIMEOUT_MS`=1000 ms) s'exécute dans une tâche
  FreeRTOS dédiée (`keepaliveProbeTask`), épinglée au **cœur 1**
  (`KEEPALIVE_PROBE_CORE`, `WiFiManager.h:151`), priorité 1 — voir
  `docs/engineering/15_RUNTIME_AND_PROFILING.md` pour la cartographie
  complète des tâches dédiées et la contention avec `loopTask` identifiée
  le 1er oct. 2026 ;
- la cible par défaut est la passerelle (`WiFi.gatewayIP()`), capturée à la
  première connexion réussie si aucune cible n'est déjà enregistrée en NVS ;
  modifiable ensuite (`setKeepaliveHost()`) sans jamais être écrasée
  automatiquement une fois définie ;
- après `KEEPALIVE_FAILURE_THRESHOLD`=3 échecs consécutifs de la sonde
  (`ZOMBIE_ESCALATION_WINDOW_MS`=20 min), une reconnexion forcée est
  déclenchée et `FaultManager` est notifié.

Cette sonde a été durcie par plusieurs correctifs successifs (historique
`fix(wifi)` dans `git log -- src/WiFiManager.cpp`) : non-blocage de
`loopTask` pendant l'attente de la sonde (28 sept. 2026, avant quoi un
blocage jusqu'à 1 s toutes les 45 s coïncidait avec des timeouts SD
mesurés), journalisation de la vraie raison de déconnexion, etc. Elle reste
un candidat identifié de contention avec `loopTask` sur le cœur 1 (voir
référence ci-dessus) — pas un défaut en soi, mais une tâche de plus à
considérer dans tout futur travail d'ordonnancement.

## CloudSync : plafond applicatif et recul après échecs (29-30 sept. 2026)

`CloudSync` (voir `docs/architecture/CLOUD_REMOTE_CONFIG.md` pour la
conception d'origine) exécute aussi sa synchronisation dans une tâche dédiée
du cœur 1 (`cloud-sync`, voir `docs/engineering/15_RUNTIME_AND_PROFILING.md`).
Deux renforcements de robustesse, commités fin septembre 2026 :

- **Plafond applicatif** (`CloudSyncWatchdog`, `CloudSync.h`/`.cpp`) : le
  framework vendored (`WiFiClientSecure`/mbedTLS) peut rester bloqué
  indéfiniment dans `client->connect()` sur une erreur directe (codes
  mbedTLS tels que -78/-80), car `handshake_timeout` n'est consulté que sur
  un retour `WANT_READ`/`WANT_WRITE`, jamais sur une erreur immédiate — un
  angle mort structurel de la bibliothèque, pas un réglage accessible côté
  client. `CloudSyncWatchdog` impose un plafond externe de 25 s
  (`PHASE_HARD_DEADLINE_MS`) : au-delà, `checkSyncWatchdog()` (appelée
  depuis `loop()`, donc cross-tâche) ferme le descripteur de socket brut
  (`lwip_shutdown()`, jamais les structures mbedTLS elles-mêmes) pour forcer
  l'échec de la tentative en cours.
- **Recul après échecs** : `CloudSyncScheduler::update()` n'autorise le
  délai rapide `SYNC_SOON_SECONDS`=30 s (utilisé quand la configuration
  locale a changé) que si `_consecutiveFailures < CLOUD_SYNC_FAILURE_
  CONFIRMATIONS` (3) ; au-delà, retombe sur l'intervalle nominal même si un
  changement est en attente. Corrige un défaut latent où une panne externe
  soutenue faisait marteler le serveur à 30 s indéfiniment, aggravant une
  éventuelle protection anti-abus côté hébergeur au lieu de la laisser se
  résorber.

**Incident de référence (29 sept.-1er oct. 2026)** : une panne de ~22h40
(aucun cycle réussi) a suivi un plantage `panic/exception` du 29 sept. matin
(cause corrigée séparément, use-after-free dans `checkSyncWatchdog()`
lisant `activeClient` hors du verrou qui le protège). Campagne d'isolation
en 8 étapes (bancs isolés successifs : blocage IP/matériel, contenu de
requête, AsyncTCP, carte SD, écran, les trois combinés, sonde keepalive sur
le même cœur que CloudSync, et 8h en continu sur le banc le plus simple) —
**toutes écartées**. La reprise du firmware de production tel quel, sans
changement de code, a suffi à rétablir CloudSync. Conclusion retenue : état
transitoire côté hébergeur/réseau déclenché par l'incident initial combiné
au défaut de martelage (déjà corrigé par le recul ci-dessus), résorbé avec
le temps — pas un défaut structurel du firmware. Détail complet de la
campagne :
`docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md`.

### Taux d'échec CloudSync corrélé à l'activité web (1er oct. 2026)

Suite de l'investigation ci-dessus : une session de ~6h d'édition active
via l'interface web a été comparée à une fenêtre calme équivalente sur le
même firmware. Taux d'échec CloudSync mesuré : **~1 échec / 67 min au
repos contre ~1 échec / 31 min pendant l'activité web active** (environ
2x plus fréquent ; échantillons modestes — 3 contre 12 événements — donc
tendance, pas une preuve statistique forte). Explication retenue : la
tâche `cloud-sync` partage le cœur 1 avec `loopTask`
(`docs/engineering/15_RUNTIME_AND_PROFILING.md`), et le même gel résiduel
qui touche `display`/`web` de temps en temps tombe aussi, parfois,
pendant la fenêtre de connexion TLS d'un cycle CloudSync -- pas un
phénomène CloudSync séparé. Détail complet et pistes de réduction des
fausses alertes :
`docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md`,
section "Corrélation activité web / fiabilité CloudSync".

## Scan réseau

`startScan()` utilise `WiFi.scanNetworks(true, false)` en mode asynchrone. En portail captif, le mode passe temporairement à `WIFI_AP_STA`. `clearScan()` supprime les résultats et revient à `WIFI_AP` si nécessaire.

Chaque `ScanEntry` contient SSID, RSSI et indicateur de chiffrement.

## Intégration Runtime

Dans `src/main.cpp` :

- `wifiMgr.begin(...)` est appelé après le Scheduler ;
- `wifiMgr.update()` est exécuté à chaque boucle avant NTP et météo ;
- NTP et météo ne sont mis à jour que si `wifiMgr.isConnected()` ;
- le Scheduler n’est pas conditionné directement par le Wi-Fi, mais son évaluation calendaire attend `ntpMgr.isSynced()`.

Le serveur Web est initialisé même si la connexion STA n’est pas encore établie ; son accessibilité dépend du mode réseau actif.

## Invariants

- `INV-NET-001` : aucune reconnexion Wi-Fi ne bloque la boucle principale.
- `INV-NET-002` : l’auto-reconnexion native est désactivée ; la stratégie appartient à `WiFiManager`.
- `INV-NET-003` : les tentatives sont espacées de 30 s et bornées à 5.
- `INV-NET-004` : un SSID vide conduit au portail captif, pas à une boucle de connexion vide.
- `INV-NET-005` : NTP et météo ne sont actualisés que lorsque la STA est connectée.
- `INV-NET-006` : le moteur d’arrosage et les relais ne dépendent pas directement du réseau.
- `INV-NET-007` : le scan est asynchrone.
- `INV-NET-008` : la sonde de keepalive ne bloque jamais `loopTask` — le
  travail réseau bloquant vit exclusivement dans sa tâche dédiée
  (`keepaliveProbeTask`), `checkKeepaliveReachable()` ne fait que lire un
  drapeau d'achèvement.
- `INV-NET-009` : aucun plafond de connexion CloudSync ne peut rester
  bloqué indéfiniment — `CloudSyncWatchdog` impose une borne externe de
  25 s même quand le framework vendored ne respecte pas son propre
  `handshake_timeout`.
- `INV-NET-010` : le délai rapide de CloudSync (30 s) ne se maintient pas
  indéfiniment face à des échecs répétés — recul vers l'intervalle nominal
  après 3 échecs consécutifs, pour ne jamais aggraver une panne externe par
  un martelage soutenu.

## Validation

- boot avec identifiants valides ;
- SSID absent ;
- mot de passe refusé ;
- timeout de 15 s ;
- cinq tentatives espacées de 30 s ;
- perte puis retour du Wi-Fi ;
- portail captif et DNS wildcard ;
- scan en mode AP ;
- fonctionnement du Runtime et des relais sans réseau ;
- absence de fuite mémoire sur cycles répétés ;
- **1er oct. 2026** : CloudSync reflashé en production après ~22h40 de
  panne, confirmé stable (cycles réussis répétés à l'intervalle nominal) ;
  plafond `CloudSyncWatchdog` confirmé fonctionnel sur 4 déclenchements
  réels consécutifs une nuit de test (voir checkpoint de session) ;
  déclenchement du recul après 3 échecs mesuré en direct (espacement passé
  de 30 s à 5 min pile après le 3e échec).

## Écarts ouverts

- protéger le point d’accès captif par une politique d’authentification adaptée ;
- définir le mécanisme de reprise après cinq échecs sans intervention ni reboot ;
- vérifier la reprise NTP après reconnexion et documenter le déclencheur exact ;
- ajouter des tests automatisés de la machine d’états et des deadlines ;
- corriger l’impression en clair du mot de passe Wi-Fi dans `WebManager::handleSetWifi()` ;
- la cause exacte du blocage CloudSync de ~22h40 (29 sept.-1er oct. 2026)
  côté hébergeur n'a pas pu être confirmée depuis ce poste (pas d'accès aux
  journaux serveur) — hypothèse retenue (état transitoire + martelage avant
  correctif) plausible et cohérente avec les faits observés, mais non
  confirmée par une source côté serveur ;
  envisager un contact avec le support de l'hébergeur si l'incident se
  reproduit ;
- **réduire les fausses alertes CloudSync liées au gel résiduel cœur 1**
  (1er oct. 2026) : `FaultId::CLOUD_SYNC` se déclenche après 3 échecs
  consécutifs (`CLOUD_SYNC_FAILURE_CONFIRMATIONS`), ce qui arrive
  régulièrement pendant une activité web soutenue à cause du gel résiduel
  (voir ci-dessus et `docs/engineering/15_RUNTIME_AND_PROFILING.md`) —
  l'utilisateur doit alors potentiellement acquitter un défaut qui s'est
  déjà auto-résolu quelques minutes plus tard. Pistes non implémentées,
  voir le checkpoint du 1er oct. 2026 pour le détail : distinguer dans
  l'UI "s'est produit et résolu seul" de "actif maintenant", s'attaquer
  plutôt à la cause racine (le gel lui-même), ou revoir le seuil de 3.
  Vérifier d'abord si l'UI (Web/LCD) exige réellement un acquittement
  manuel pour ce défaut précis ou si elle se contente de l'afficher tant
  qu'il est actif (non vérifié à ce stade).

## Références

- `src/WiFiManager.h` ;
- `src/WiFiManager.cpp` ;
- `src/WebManager.h` et `.cpp` ;
- `src/CloudSync.h` et `.cpp` ;
- `src/main.cpp` ;
- `docs/engineering/09_WEB_AND_HTTP_INTERFACES.md` ;
- `docs/engineering/15_RUNTIME_AND_PROFILING.md` (tâches dédiées, répartition des cœurs) ;
- `docs/architecture/CLOUD_REMOTE_CONFIG.md` (conception d'origine de CloudSync) ;
- `docs/security/CYBERSECURITY_ARCHITECTURE.md`.

## Historique

### 1.3

Ajout de la corrélation mesurée entre activité web soutenue et taux
d'échec CloudSync (~2x plus fréquent, ~1/31min contre ~1/67min au repos),
rattachée au gel résiduel cœur 1 déjà documenté en 1.2. Nouvel écart
ouvert sur la réduction des fausses alertes `FaultId::CLOUD_SYNC`
déclenchées par ce gel plutôt que par une vraie panne prolongée.

### 1.2

Ajout de la sonde de keepalive (détection de connexion "zombie"),
jusqu'alors absente de ce document malgré plusieurs semaines de présence
et de durcissements successifs en service. Ajout d'une section CloudSync
(plafond applicatif `CloudSyncWatchdog`, recul après échecs, incident de
~22h40 fin sept. 2026 et sa résolution). Nouveaux invariants INV-NET-008 à
010.

### 1.1

Consolidation D4 de la machine d’états, des timeouts, des tentatives, du portail captif et du scan asynchrone.