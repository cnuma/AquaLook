#include "WiFiManager.h"
#include "BootLoopGuard.h"
#include "EventBus.h"
#include "EventLog.h"
#include "FaultManager.h"
#include "TimeUtils.h"
#include <DNSServer.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <esp_wifi.h>

static DNSServer _dnsServer;
static bool _dnsStarted = false;

static constexpr uint8_t DNS_PORT = 53;
static constexpr const char* CAPTIVE_AP_SSID = "Arrosage-Setup";

// Namespace NVS dedie, distinct de la configuration principale
// (ConfigManager) : cette valeur est un parametre diagnostique lie au
// cycle de vie de la connexion WiFi (auto-derivee, modifiable pour les
// tests), pas une donnee de configuration deliberee de l'utilisateur.
static constexpr const char* KEEPALIVE_NVS_NAMESPACE = "aq_wifi_ka";
static constexpr const char* KEEPALIVE_NVS_KEY = "host";

// Traduit wl_status_t (WiFiType.h) pour le journal -- "wl_status=4" n'est
// lisible que pour qui a le header sous les yeux.
static const char* wlStatusName(wl_status_t s) {
    switch (s) {
        case WL_IDLE_STATUS:     return "idle";
        case WL_NO_SSID_AVAIL:   return "ssid_absent";
        case WL_SCAN_COMPLETED:  return "scan_termine";
        case WL_CONNECTED:       return "connecte";
        case WL_CONNECT_FAILED:  return "echec_connexion";
        case WL_CONNECTION_LOST: return "connexion_perdue";
        case WL_DISCONNECTED:    return "deconnecte";
        case WL_NO_SHIELD:       return "sans_module";
        default:                 return "inconnu";
    }
}

// Diagnostic pur (ajoute le 28 sept. 2026, investigation gel chronique sous
// RSSI degrade -- voir memoire checkpoint-2026-09-28-gel-chronique-resolu) :
// le code applicatif ne peut pas voir PLUS vite qu'une association wifi
// "zombie" que le pilote lui-meme (voir checkKeepaliveReachable() plus bas),
// mais le pilote CONNAIT deja la raison exacte d'une deconnexion reelle --
// simplement jamais journalisee jusqu'ici. Beacon timeout (200) confirmerait
// une perte RF (antenne/materiel) ; auth/assoc expire ou handshake timeout
// pointerait plutot vers la box. Couvre aussi les deconnexions provoquees
// par WiFi.disconnect(true) dans checkKeepaliveReachable() -- s'y reperer
// par l'horodatage, pas par la raison (generique dans ce cas-la).
static const char* wifiDisconnectReasonName(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_UNSPECIFIED:            return "non_precisee";
        case WIFI_REASON_AUTH_EXPIRE:            return "auth_expiree";
        case WIFI_REASON_AUTH_LEAVE:             return "auth_quittee";
        case WIFI_REASON_ASSOC_EXPIRE:           return "assoc_expiree";
        case WIFI_REASON_NOT_AUTHED:             return "non_authentifie";
        case WIFI_REASON_NOT_ASSOCED:            return "non_associe";
        case WIFI_REASON_ASSOC_LEAVE:            return "assoc_quittee_par_nous";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "handshake_wpa_timeout";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "renouvellement_cle_timeout";
        case WIFI_REASON_STA_LEAVING:             return "station_quitte";
        case WIFI_REASON_TIMEOUT:                 return "timeout_generique";
        case WIFI_REASON_BEACON_TIMEOUT:          return "beacon_timeout_RF";
        case WIFI_REASON_NO_AP_FOUND:             return "ap_introuvable";
        case WIFI_REASON_AUTH_FAIL:               return "echec_auth";
        case WIFI_REASON_ASSOC_FAIL:              return "echec_association";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:       return "handshake_timeout";
        case WIFI_REASON_CONNECTION_FAIL:         return "echec_connexion_generique";
        default:                                  return "autre";
    }
}

static void onWifiDisconnectedEvent(WiFiEvent_t, WiFiEventInfo_t info) {
    EventLog::log(
        LOG_WARN,
        "WiFi: deconnecte par le pilote, raison=%u (%s) rssi_avant=%ddBm "
        "bssid=%02x:%02x:%02x:%02x:%02x:%02x",
        static_cast<unsigned>(info.wifi_sta_disconnected.reason),
        wifiDisconnectReasonName(info.wifi_sta_disconnected.reason),
        WiFi.RSSI(),
        info.wifi_sta_disconnected.bssid[0], info.wifi_sta_disconnected.bssid[1],
        info.wifi_sta_disconnected.bssid[2], info.wifi_sta_disconnected.bssid[3],
        info.wifi_sta_disconnected.bssid[4], info.wifi_sta_disconnected.bssid[5]
    );
}

void WiFiManager::begin(const char* ssid, const char* pwd) {
    strlcpy(_ssid, ssid, sizeof(_ssid));
    strlcpy(_pwd, pwd, sizeof(_pwd));

    loadKeepaliveHost();

    WiFi.onEvent(onWifiDisconnectedEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    // Mesh : un SSID, plusieurs noeuds sur le meme canal. Le scan rapide par
    // defaut s'arrete au PREMIER noeud qui repond -- le 10 oct. 2026, deux
    // redemarrages identiques de .141 ont donne -89 dBm (noeud lointain) puis
    // -54 dBm (noeud proche). Scanner tous les canaux et retenir le plus fort ;
    // reglage statique, valable pour toutes les reconnexions (WiFi.begin).
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

    if (_ssid[0] == '\0') {
        EventLog::log(LOG_WARN, "WiFi: pas de SSID, portail captif");
        EventBus::captiveRequested = true;
    } else {
        EventLog::log(
            LOG_INFO,
            "WiFi: SSID='%s', mot de passe present=%s",
            _ssid,
            strlen(_pwd) > 0 ? "oui" : "non"
        );
        startConnection();
    }
}

void WiFiManager::loadKeepaliveHost() {
    Preferences prefs;
    if (!prefs.begin(KEEPALIVE_NVS_NAMESPACE, true)) {
        _keepaliveHost[0] = '\0';
        return;
    }
    const String v = prefs.getString(KEEPALIVE_NVS_KEY, "");
    strlcpy(_keepaliveHost, v.c_str(), sizeof(_keepaliveHost));
    prefs.end();
}

void WiFiManager::saveKeepaliveHost() const {
    Preferences prefs;
    if (!prefs.begin(KEEPALIVE_NVS_NAMESPACE, false)) return;
    prefs.putString(KEEPALIVE_NVS_KEY, _keepaliveHost);
    prefs.end();
}

bool WiFiManager::setKeepaliveHost(const char* host) {
    if (host == nullptr) return false;

    char trimmed[64];
    strlcpy(trimmed, host, sizeof(trimmed));
    // Retirer les espaces de bord (copie/colle depuis l'UI).
    size_t start = 0;
    while (trimmed[start] == ' ') start++;
    size_t end = strlen(trimmed);
    while (end > start && trimmed[end - 1] == ' ') end--;
    trimmed[end] = '\0';
    if (start > 0) memmove(trimmed, trimmed + start, end - start + 1);

    strlcpy(_keepaliveHost, trimmed, sizeof(_keepaliveHost));
    saveKeepaliveHost();
    _consecutiveKeepaliveFailures = 0;
    EventLog::log(
        LOG_INFO,
        "WiFi: cible keepalive definie manuellement -> '%s'",
        _keepaliveHost[0] != '\0' ? _keepaliveHost : "(vide)"
    );
    return true;
}

void WiFiManager::update() {
    const uint32_t now = millis();

    if (processPendingAction(now)) {
        return;
    }

    if (EventBus::captiveRequested &&
        _state != State::CAPTIVE_STARTING &&
        _state != State::CAPTIVE_PORTAL) {
        EventBus::captiveRequested = false;
        startCaptivePortal();
        return;
    }

    switch (_state) {
        case State::CONNECTING:
            handleConnecting(now);
            break;
        case State::CONNECTED:
            handleConnected();
            break;
        case State::DISCONNECTED:
            handleDisconnected(now);
            break;
        case State::CAPTIVE_PORTAL:
            handleCaptivePortal();
            break;
        case State::CAPTIVE_STARTING:
        case State::IDLE:
            break;
    }
}

void WiFiManager::scheduleAction(
    PendingAction action,
    uint32_t deadlineMs
) {
    _pendingAction = action;
    _pendingDeadlineMs = deadlineMs;
}

bool WiFiManager::processPendingAction(uint32_t now) {
    if (_pendingAction == PendingAction::NONE) return false;
    if (!AquaLook::Time::deadlineReached(now, _pendingDeadlineMs)) return true;

    const PendingAction action = _pendingAction;
    _pendingAction = PendingAction::NONE;

    switch (action) {
        case PendingAction::STA_SET_MODE:
            WiFi.mode(WIFI_STA);
            _staModeSetMs = now;
            scheduleAction(PendingAction::STA_SCAN_START, now + WIFI_MODE_SETTLE_MS);
            return true;

        case PendingAction::STA_SCAN_START:
            // startConnection() vient d'arreter la station (disconnect(true)) :
            // un scan lance avant STA_START est perdu (constate au demarrage de
            // .141, scan jamais termine). Attendre le demarrage effectif.
            if (!(WiFi.getStatusBits() & STA_STARTED_BIT) &&
                (now - _staModeSetMs) < STA_START_TIMEOUT_MS) {
                scheduleAction(PendingAction::STA_SCAN_START, now + TARGET_SCAN_POLL_MS);
                return true;
            }
            // Tentatives paires : scan puis connexion au noeud le plus fort.
            // Tentatives impaires : choix du pilote, pour ne jamais rester
            // bloque sur un noeud visible qui refuserait l'association.
            // Un scan Web en cours garde la main sur le resultat du scan.
            // Scan actif 150 ms par canal : ~2 s sur 13 canaux ; Arduino
            // declare le scan en echec au-dela de 20 x cette duree (3 s).
            if ((_retryCount % 2U) == 0U && !_scanPending &&
                WiFi.scanNetworks(true, false, false, TARGET_SCAN_MS_PER_CHANNEL) ==
                    WIFI_SCAN_RUNNING) {
                _targetScanStartMs = now;
                scheduleAction(PendingAction::STA_SCAN_WAIT, now + TARGET_SCAN_POLL_MS);
            } else {
                scheduleAction(
                    PendingAction::STA_BEGIN,
                    now + WIFI_MODE_SETTLE_MS
                );
            }
            return true;

        case PendingAction::STA_SCAN_WAIT:
            if (WiFi.scanComplete() == WIFI_SCAN_RUNNING &&
                (now - _targetScanStartMs) < TARGET_SCAN_TIMEOUT_MS) {
                scheduleAction(PendingAction::STA_SCAN_WAIT, now + TARGET_SCAN_POLL_MS);
                return true;
            }
            beginTargeted(now);
            return true;

        case PendingAction::STA_BEGIN:
            WiFi.begin(_ssid, _pwd);
            _lastActionMs = now;
            return true;

        case PendingAction::AP_SET_MODE:
            WiFi.mode(WIFI_AP);
            WiFi.softAP(CAPTIVE_AP_SSID);
            scheduleAction(
                PendingAction::AP_FINALIZE,
                now + WIFI_AP_SETTLE_MS
            );
            return true;

        case PendingAction::AP_FINALIZE: {
            const IPAddress apIp = WiFi.softAPIP();

            EventLog::log(
                LOG_INFO,
                "WiFi: AP '%s' IP=%s",
                CAPTIVE_AP_SSID,
                apIp.toString().c_str()
            );

            _dnsServer.start(DNS_PORT, "*", apIp);
            _dnsStarted = true;
            _state = State::CAPTIVE_PORTAL;
            EventBus::displayDirty = true;

            // Lancer le scan reseau immediatement, avant qu'un client ne
            // rejoigne le point d'acces : scanner pendant que quelqu'un est
            // deja connecte au portail le deconnecte brievement (l'ESP32
            // n'a qu'une seule radio, le scan doit quitter le canal de l'AP
            // pour explorer les autres). En le lancant des la creation de
            // l'AP, le resultat est deja pret quand la page est ouverte.
            startScan();
            return true;
        }

        case PendingAction::RESTART:
            BootLoopGuard::restartDeliberately("bascule de mode WiFi");
            return true;

        case PendingAction::NONE:
            return false;
    }

    return false;
}

// Fin du scan de connexion : retient le BSSID le plus fort du SSID et s'y
// connecte. Sans resultat exploitable (scan echoue ou expire, SSID absent),
// repli sur le choix du pilote -- jamais d'attente supplementaire.
void WiFiManager::beginTargeted(uint32_t now) {
    const int16_t n = static_cast<int16_t>(WiFi.scanComplete());
    int best = -1;
    uint8_t seen = 0;
    for (int16_t i = 0; i < n; ++i) {
        if (WiFi.SSID(i) != _ssid) continue;
        ++seen;
        if (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best)) best = i;
    }

    if (best >= 0) {
        uint8_t bssid[6];
        memcpy(bssid, WiFi.BSSID(best), sizeof(bssid));
        const int32_t channel = WiFi.channel(best);
        EventLog::log(
            LOG_INFO,
            "WiFi: cible BSSID=%02x:%02x:%02x:%02x:%02x:%02x ch=%ld RSSI=%ddBm (%u noeud(s), %lu+%lums)",
            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
            static_cast<long>(channel), static_cast<int>(WiFi.RSSI(best)),
            static_cast<unsigned>(seen),
            static_cast<unsigned long>(_targetScanStartMs - _staModeSetMs),
            static_cast<unsigned long>(now - _targetScanStartMs)
        );
        WiFi.scanDelete();
        WiFi.begin(_ssid, _pwd, channel, bssid);
    } else {
        EventLog::log(
            LOG_INFO,
            "WiFi: scan de connexion sans resultat (n=%d, %lu+%lums), choix du pilote",
            static_cast<int>(n),
            static_cast<unsigned long>(_targetScanStartMs - _staModeSetMs),
            static_cast<unsigned long>(now - _targetScanStartMs)
        );
        if (n == WIFI_SCAN_RUNNING) esp_wifi_scan_stop();  // expire : liberer la radio
        else if (n >= 0) WiFi.scanDelete();
        WiFi.begin(_ssid, _pwd);
    }
    _lastActionMs = now;
}

void WiFiManager::handleConnecting(uint32_t now) {
    const wl_status_t s = WiFi.status();

    if (s == WL_CONNECTED) {
        WiFi.setSleep(false);

        _state = State::CONNECTED;
        _retryCount = 0;
        FaultManager::setActive(FaultId::WIFI, false);

        EventLog::log(
            LOG_INFO,
            "WiFi: connecte IP=%s RSSI=%ddBm BSSID=%s ch=%d, veille desactivee",
            WiFi.localIP().toString().c_str(),
            WiFi.RSSI(),
            WiFi.BSSIDstr().c_str(),
            static_cast<int>(WiFi.channel())
        );

        EventBus::displayDirty = true;
        return;
    }

    const bool timedOut =
        (now - _lastActionMs) > CONNECT_TIMEOUT_MS;
    const bool hardFail = (s == WL_CONNECT_FAILED);
    const bool noSsid = (s == WL_NO_SSID_AVAIL);

    if (hardFail || noSsid || timedOut) {
        const char* cause =
            hardFail ? "association refusee" :
            noSsid ? "SSID introuvable" :
            "timeout 15s";

        // Annoncer le VRAI delai. Le message affichait toujours 30 s, meme
        // quand le module attendait 15 minutes : le silence devenait alors
        // indiscernable d un gel -- constate le 6 septembre 2026, ou j ai
        // cru le module bloque alors qu il patientait normalement.
        EventLog::log(
            LOG_WARN,
            "WiFi: echec #%u, %s, statut=%s, prochaine tentative dans %lus",
            _retryCount + 1,
            cause,
            wlStatusName(s),
            retryDelayMs() / 1000UL
        );

        if (noSsid) {
            EventLog::log(
                LOG_WARN,
                "WiFi: '%s' absent, verifier SSID ou portee",
                _ssid
            );
        }

        if (hardFail) {
            // WL_CONNECT_FAILED est generique : mot de passe, mais aussi
            // filtrage MAC ou point d acces sature. Conclure "mot de passe"
            // envoyait chercher au mauvais endroit -- constate le 6 sept.
            // 2026 en mettant le module en liste noire cote box.
            EventLog::log(
                LOG_WARN,
                "WiFi: refus assoc: mot de passe, filtrage MAC ou AP sature ?"
            );
        }

        WiFi.disconnect(true);
        _state = State::DISCONNECTED;
        _lastActionMs = now;
        if (_retryCount < 250U) _retryCount++;  // pas de repli a 0
        EventBus::displayDirty = true;
    }
}

void WiFiManager::handleConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        EventLog::log(
            LOG_WARN,
            "WiFi: connexion perdue, statut=%s",
            wlStatusName(WiFi.status())
        );
        _state = State::DISCONNECTED;
        _lastActionMs = millis();
        EventBus::displayDirty = true;
        return;
    }

    // Cible de keepalive absente : la deduire de la passerelle du reseau.
    // Tente a chaque tour de boucle tant qu'elle manque (au lieu du seul
    // instant de la transition CONNECTING -> CONNECTED) car WiFi.gatewayIP()
    // peut brievement rendre 0.0.0.0 juste apres l'evenement GOT_IP ; ce
    // retry ferme cette fenetre de course sans complexite supplementaire.
    // Une valeur deja presente (definie manuellement, ou heritee d'une
    // connexion precedente) n'est jamais ecrasee automatiquement.
    if (_keepaliveHost[0] == '\0') {
        const IPAddress gateway = WiFi.gatewayIP();
        if (gateway != IPAddress(0, 0, 0, 0)) {
            strlcpy(_keepaliveHost, gateway.toString().c_str(), sizeof(_keepaliveHost));
            saveKeepaliveHost();
            EventLog::log(
                LOG_INFO,
                "WiFi: keepalive -> passerelle %s",
                _keepaliveHost
            );
        }
    }

    checkKeepaliveReachable(millis());
}

// Sonde active la cible de keepalive (passerelle par defaut, ou toute
// autre IP/hote enregistre — voir _keepaliveHost), independamment de ce
// que rapporte WiFi.status(). Necessaire car un incident observe sur le
// terrain a montre que le pilote WiFi peut continuer a annoncer
// WL_CONNECTED pendant de longues minutes (~24 min observees) apres une
// perte reelle d'association (ex. echec de renouvellement de cle de
// groupe WPA2) : une surveillance purement passive ne peut jamais
// detecter ce cas plus vite que le pilote lui-meme ne s'en apercoit.
void WiFiManager::checkKeepaliveReachable(uint32_t now) {
    // Ne jamais bloquer ici : si une sonde precedente est encore en vol,
    // se contenter de regarder si elle a fini (voir keepaliveProbeTask()).
    if (_keepaliveProbeRunning) {
        if (_keepaliveProbeDone) {
            _keepaliveProbeRunning = false;
            processKeepaliveResult(_keepaliveProbeResult);
        }
        return;
    }

    if (now - _lastKeepaliveCheckMs < KEEPALIVE_CHECK_INTERVAL_MS) return;
    _lastKeepaliveCheckMs = now;

    if (_keepaliveHost[0] == '\0') return;  // pas encore de cible connue

    IPAddress target;
    if (!target.fromString(_keepaliveHost)) {
        // Pas une IP litterale : tenter une resolution DNS (utile le jour
        // ou la cible pointe vers un hote/service cloud). hostByName() reste
        // bloquant lui aussi, mais ce chemin n'est emprunte que si la cible
        // est un nom -- jamais le cas pour la passerelle auto-remplie.
        if (WiFi.hostByName(_keepaliveHost, target) != 1) {
            EventLog::log(
                LOG_WARN,
                "WiFi: cible keepalive '%s' non resolue",
                _keepaliveHost
            );
            return;
        }
    }

    _keepaliveProbeTarget = target;
    _keepaliveProbeDone = false;
    _keepaliveProbeRunning = true;

    const BaseType_t created = xTaskCreatePinnedToCore(
        keepaliveProbeTask, "wifi-keepalive", KEEPALIVE_PROBE_STACK, this,
        KEEPALIVE_PROBE_PRIORITY, nullptr, KEEPALIVE_PROBE_CORE
    );
    if (created != pdPASS) {
        _keepaliveProbeRunning = false;
        EventLog::log(LOG_WARN, "WiFi: sonde keepalive indisponible (creation tache)");
    }
}

void WiFiManager::keepaliveProbeTask(void* param) {
    WiFiManager* self = static_cast<WiFiManager*>(param);
    // Tentative d'instrumentation du 1er oct. 2026 (horodatage + EventLog)
    // RETIREE : le log s'executait sur ce MEME coeur 1 que CloudSync, et la
    // mesure elle-meme pouvait donc aggraver la contention qu'elle cherchait
    // a observer (effet observateur). Un entrelacement caractere-par-
    // caractere avec la sortie serie de CloudSync a ete constate juste
    // apres l'ajout -- pas une preuve formelle, mais un risque suffisant
    // pour revenir a la version sans log ici. Voir
    // docs/checkpoints/CHECKPOINT_2026-10-01_cloudsync-outage-and-core1-contention.md
    // pour le contexte complet.
    WiFiClient probe;
    const bool reachable = probe.connect(
        self->_keepaliveProbeTarget, KEEPALIVE_CHECK_PORT, KEEPALIVE_CHECK_TIMEOUT_MS
    );
    probe.stop();
    self->_keepaliveProbeResult = reachable;
    self->_keepaliveProbeDone = true;
    vTaskDelete(nullptr);
}

void WiFiManager::processKeepaliveResult(bool reachable) {
    if (reachable) {
        _consecutiveKeepaliveFailures = 0;
        return;
    }

    const uint32_t now = millis();
    _consecutiveKeepaliveFailures++;
    EventLog::log(
        LOG_WARN,
        "WiFi: cible keepalive %s (%s) injoignable (%u/%u), statut=%s toujours 'connecte'",
        _keepaliveHost,
        _keepaliveProbeTarget.toString().c_str(),
        static_cast<unsigned>(_consecutiveKeepaliveFailures),
        static_cast<unsigned>(KEEPALIVE_FAILURE_THRESHOLD),
        wlStatusName(WiFi.status())
    );

    if (_consecutiveKeepaliveFailures < KEEPALIVE_FAILURE_THRESHOLD) return;

    EventLog::log(
        LOG_ERROR,
        "WiFi: connexion zombie detectee (cible keepalive injoignable x%u malgre wl_status=connecte), reconnexion forcee",
        static_cast<unsigned>(_consecutiveKeepaliveFailures)
    );
    _consecutiveKeepaliveFailures = 0;
    recordZombieEventAndMaybeEscalate(now);
    WiFi.disconnect(true);
    _state = State::DISCONNECTED;
    _lastActionMs = now;
    EventBus::displayDirty = true;
}

// Un cycle zombie isole se resout seul en general (~30s, prochaine tentative
// de connexion) et ne merite pas d'alerte persistante. Mais si
// ZOMBIE_ESCALATION_COUNT cycles surviennent en moins de
// ZOMBIE_ESCALATION_WINDOW_MS, ca sent l'instabilite recurrente (routeur,
// signal marginal...) plutot qu'un accident isole : on le signale via
// FaultManager, comme l'incident SD (triangle LCD + LED), pour laisser une
// trace visible et acquittable au lieu que chaque episode se referme tout
// seul sans jamais rien remonter a l'utilisateur.
void WiFiManager::recordZombieEventAndMaybeEscalate(uint32_t now) {
    _zombieEventsMs[_zombieEventIdx] = now;
    _zombieEventIdx = (_zombieEventIdx + 1) % ZOMBIE_ESCALATION_COUNT;
    if (_zombieEventCount < ZOMBIE_ESCALATION_COUNT) _zombieEventCount++;

    if (_zombieEventCount < ZOMBIE_ESCALATION_COUNT) return;

    // Apres l'incrementation ci-dessus, cet index pointe sur la case la
    // plus ancienne des ZOMBIE_ESCALATION_COUNT dernieres (celle qui sera
    // ecrasee au prochain evenement).
    const uint32_t oldest = _zombieEventsMs[_zombieEventIdx];
    if (now - oldest > ZOMBIE_ESCALATION_WINDOW_MS) return;

    FaultManager::setActive(FaultId::WIFI, true);
    EventLog::log(
        LOG_ERROR,
        "WiFi: instabilite recurrente (x%u cycles en <%lumin), signalement persistant",
        static_cast<unsigned>(ZOMBIE_ESCALATION_COUNT),
        static_cast<unsigned long>(ZOMBIE_ESCALATION_WINDOW_MS / 60000UL)
    );
}

// Espacement croissant plafonne : 30 s tant qu on espere une reprise
// immediate, puis 1 min, 5 min, et 15 min au plus. Assez frequent pour
// revenir vite, assez espace pour ne pas marteler la radio des heures.
uint32_t WiFiManager::retryDelayMs() const {
    if (_retryCount < MAX_RETRIES) return RETRY_INTERVAL_MS;
    const uint8_t beyond = static_cast<uint8_t>(_retryCount - MAX_RETRIES);
    if (beyond < 3U) return 60000UL;
    if (beyond < 6U) return 300000UL;
    return 900000UL;
}

void WiFiManager::handleDisconnected(uint32_t now) {
    if ((now - _lastActionMs) < retryDelayMs()) return;

    if (_retryCount == MAX_RETRIES) {
        FaultManager::setActive(FaultId::WIFI, true);
        EventLog::log(
            LOG_ERROR,
            "WiFi: %u echecs sur '%s', reprise espacee",
            MAX_RETRIES,
            _ssid
        );
        EventBus::displayDirty = true;
    }

    // On ne renonce JAMAIS. Un module d arrosage doit revenir seul quand
    // le reseau revient, sans qu on aille le debrancher. Avant le 6
    // septembre 2026 il abandonnait apres 5 echecs, soit 2 min 30 : une
    // box lente a redemarrer suffisait a le couper du monde jusqu au
    // prochain redemarrage.
    startConnection();
}

void WiFiManager::handleCaptivePortal() {
    if (_dnsStarted) _dnsServer.processNextRequest();
}

void WiFiManager::startConnection() {
    EventLog::log(
        LOG_INFO,
        "WiFi: tentative #%u sur '%s'",
        _retryCount + 1,
        _ssid
    );

    WiFi.disconnect(true);
    _state = State::CONNECTING;
    scheduleAction(
        PendingAction::STA_SET_MODE,
        millis() + WIFI_DISCONNECT_SETTLE_MS
    );
}

void WiFiManager::startCaptivePortal() {
    EventLog::log(LOG_INFO, "WiFi: demarrage portail captif");

    if (_dnsStarted) {
        _dnsServer.stop();
        _dnsStarted = false;
    }

    WiFi.disconnect(true);
    _state = State::CAPTIVE_STARTING;
    scheduleAction(
        PendingAction::AP_SET_MODE,
        millis() + WIFI_DISCONNECT_SETTLE_MS
    );
}

void WiFiManager::stopCaptivePortal() {
    if (_dnsStarted) {
        _dnsServer.stop();
        _dnsStarted = false;
    }

    WiFi.softAPdisconnect(true);
    EventLog::log(LOG_INFO, "WiFi: portail arrete, reboot programme");

    _state = State::IDLE;
    scheduleAction(
        PendingAction::RESTART,
        millis() + WIFI_RESTART_SETTLE_MS
    );
}

int8_t WiFiManager::getRssi() const {
    if (_state != State::CONNECTED) return 0;
    return static_cast<int8_t>(WiFi.RSSI());
}

const char* WiFiManager::stateStr() const {
    switch (_state) {
        case State::IDLE:
            return "IDLE";
        case State::CONNECTING:
            return "CONNECTING";
        case State::CONNECTED:
            return "CONNECTED";
        case State::DISCONNECTED:
            return "DISCONNECTED";
        case State::CAPTIVE_STARTING:
            return "CAPTIVE_STARTING";
        case State::CAPTIVE_PORTAL:
            return "CAPTIVE_PORTAL";
        default:
            return "UNKNOWN";
    }
}

void WiFiManager::startScan() {
    if (_scanPending) return;

    if (_state == State::CAPTIVE_PORTAL) {
        WiFi.mode(WIFI_AP_STA);
    }

    WiFi.scanNetworks(true, false);
    _scanPending = true;
    EventLog::log(LOG_INFO, "WiFi: scan reseau lance");
}

int16_t WiFiManager::getScanCount() const {
    if (!_scanPending) return 0;

    const int16_t n = static_cast<int16_t>(WiFi.scanComplete());
    return n == WIFI_SCAN_RUNNING ? -1 : n;
}

WiFiManager::ScanEntry
WiFiManager::getScanEntry(uint8_t i) const {
    ScanEntry e;
    e.ssid[0] = '\0';
    e.rssi = 0;
    e.secured = false;
    memset(e.bssid, 0, sizeof(e.bssid));
    e.channel = 0;

    const int16_t n = static_cast<int16_t>(WiFi.scanComplete());
    if (n <= 0 || i >= static_cast<uint8_t>(n)) return e;

    const String s = WiFi.SSID(i);
    strlcpy(e.ssid, s.c_str(), sizeof(e.ssid));
    e.rssi = static_cast<int8_t>(WiFi.RSSI(i));
    e.secured =
        WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    const uint8_t* bssid = WiFi.BSSID(i);
    if (bssid) memcpy(e.bssid, bssid, sizeof(e.bssid));
    e.channel = static_cast<uint8_t>(WiFi.channel(i));

    return e;
}

void WiFiManager::clearScan() {
    WiFi.scanDelete();
    _scanPending = false;

    if (_state == State::CAPTIVE_PORTAL) {
        WiFi.mode(WIFI_AP);
    }
}
