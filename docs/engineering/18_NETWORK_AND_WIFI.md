# AquaLook Engineering Reference — Réseau et Wi-Fi

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/WiFiManager.h`, `src/WiFiManager.cpp`, `src/WebManager.*`, `src/main.cpp`
- Composants : Wi-Fi STA, point d’accès, DNS captif, scan asynchrone, Web et NTP
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
- absence de fuite mémoire sur cycles répétés.

## Écarts ouverts

- protéger le point d’accès captif par une politique d’authentification adaptée ;
- définir le mécanisme de reprise après cinq échecs sans intervention ni reboot ;
- vérifier la reprise NTP après reconnexion et documenter le déclencheur exact ;
- ajouter des tests automatisés de la machine d’états et des deadlines ;
- corriger l’impression en clair du mot de passe Wi-Fi dans `WebManager::handleSetWifi()`.

## Références

- `src/WiFiManager.h` ;
- `src/WiFiManager.cpp` ;
- `src/WebManager.h` et `.cpp` ;
- `src/main.cpp` ;
- `docs/engineering/09_WEB_AND_HTTP_INTERFACES.md` ;
- `docs/security/CYBERSECURITY_ARCHITECTURE.md`.

## Historique

### 1.1

Consolidation D4 de la machine d’états, des timeouts, des tentatives, du portail captif et du scan asynchrone.