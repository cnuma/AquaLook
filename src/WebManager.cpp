#include "WebManager.h"
#include "EventBus.h"
#include "EventLog.h"

// ─────────────────────────────────────────────────────────────
//  Page HTML du portail captif — servie en mode AP
//  Formulaire SSID/PWD minimaliste, POST vers /api/wifi
// ─────────────────────────────────────────────────────────────
static const char CAPTIVE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AquaLook — Configuration WiFi</title>
<style>
  body{font-family:sans-serif;background:#1a1a2e;color:#eee;
       display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}
  .card{background:#16213e;border-radius:12px;padding:2rem;width:90%;max-width:360px;
        box-shadow:0 4px 20px rgba(0,0,0,.4)}
  h2{margin:0 0 1.5rem;text-align:center;color:#4fc3f7}
  label{display:block;margin-bottom:.3rem;font-size:.9rem;color:#90caf9}
  input,select{width:100%;box-sizing:border-box;padding:.6rem .8rem;border:1px solid #334;
        border-radius:6px;background:#0f3460;color:#eee;font-size:1rem;margin-bottom:1rem}
  select option{background:#0f3460}
  .row{display:flex;gap:.5rem;margin-bottom:1rem}
  .row input{margin-bottom:0;flex:1}
  .btn-scan{flex-shrink:0;padding:.6rem .9rem;border:1px solid #4fc3f7;border-radius:6px;
            background:transparent;color:#4fc3f7;font-size:.9rem;cursor:pointer;white-space:nowrap}
  .btn-scan:hover{background:#4fc3f71a}
  .btn-scan:disabled{opacity:.4;cursor:default}
  button{width:100%;padding:.75rem;border:none;border-radius:6px;
         background:#4fc3f7;color:#000;font-size:1rem;font-weight:600;cursor:pointer}
  button:hover{background:#81d4fa}
  #networks{margin-bottom:1rem;display:none}
  #msg{margin-top:1rem;text-align:center;font-size:.9rem}
  /* Barre de progression scan */
  #scanBar{height:3px;border-radius:2px;background:#0f3460;margin-bottom:1rem;display:none;overflow:hidden}
  #scanFill{height:100%;width:0;background:#4fc3f7;border-radius:2px;
            transition:width .5s ease}
  /* Spinner inline dans le bouton */
  @keyframes spin{to{transform:rotate(360deg)}}
  .spinner{display:inline-block;width:12px;height:12px;border:2px solid #4fc3f755;
           border-top-color:#4fc3f7;border-radius:50%;
           animation:spin .7s linear infinite;vertical-align:middle;margin-right:4px}
</style></head><body>
<div class="card">
  <h2>&#127807; Configuration WiFi</h2>
  <label>Réseau WiFi (SSID)</label>
  <div class="row">
    <input type="text" id="ssid" placeholder="Nom du réseau" autocomplete="off">
    <button class="btn-scan" id="btnScan" onclick="startScan()">&#128246; Scanner</button>
  </div>
  <div id="scanBar"><div id="scanFill"></div></div>
  <select id="networks" onchange="pickNetwork(this.value)">
    <option value="">-- Sélectionner un réseau --</option>
  </select>
  <label>Mot de passe</label>
  <input type="password" id="pwd" placeholder="Mot de passe">
  <button onclick="save()">Enregistrer et connecter</button>
  <div id="msg"></div>
</div>
<script>
var _pollTimer=null;
var _barTimer=null;
var _barPct=0;
var _ip=window.location.hostname;

function startScanBar(){
  var bar=document.getElementById('scanBar');
  var fill=document.getElementById('scanFill');
  bar.style.display='block';
  _barPct=0;
  fill.style.width='0';
  // Progression simulee : monte vite jusqu a 85%, puis ralentit
  // La barre complete a 100% seulement quand le scan repond
  _barTimer=setInterval(function(){
    if(_barPct<85){_barPct+=7;}
    else if(_barPct<95){_barPct+=0.5;}
    fill.style.width=_barPct+'%';
  },300);
}

function completeScanBar(ok){
  if(_barTimer){clearInterval(_barTimer);_barTimer=null;}
  var fill=document.getElementById('scanFill');
  fill.style.background=ok?'#4caf50':'#f44336';
  fill.style.width='100%';
  setTimeout(function(){
    document.getElementById('scanBar').style.display='none';
    fill.style.width='0';
    fill.style.background='#4fc3f7';
  },800);
}

function startScan(){
  var btn=document.getElementById('btnScan');
  var msg=document.getElementById('msg');
  btn.disabled=true;
  btn.innerHTML='<span class="spinner"></span>Scan...';
  msg.textContent='';
  startScanBar();
  fetch('http://'+_ip+'/api/wifi/scan').then(function(r){return r.json();}).then(function(d){
    if(d.scanning){_pollTimer=setTimeout(pollScan,600);}
    else{onScanDone(d);}
  }).catch(function(){onScanError();});
}

function pollScan(){
  fetch('http://'+_ip+'/api/wifi/scan').then(function(r){return r.json();}).then(function(d){
    if(d.scanning){_pollTimer=setTimeout(pollScan,600);}
    else{onScanDone(d);}
  }).catch(function(){onScanError();});
}

function onScanDone(d){
  var btn=document.getElementById('btnScan');
  completeScanBar(true);
  showNetworks(d.networks);
  btn.disabled=false;
  btn.innerHTML='&#128246; Scanner';
}

function onScanError(){
  var btn=document.getElementById('btnScan');
  var msg=document.getElementById('msg');
  completeScanBar(false);
  msg.textContent='Erreur scan';msg.style.color='#f44';
  btn.disabled=false;btn.innerHTML='&#128246; Scanner';
}

function rssiBar(rssi){
  if(rssi>=-60)return'\u2588\u2588\u2588\u2588';
  if(rssi>=-70)return'\u2588\u2588\u2588\u2591';
  if(rssi>=-80)return'\u2588\u2588\u2591\u2591';
  return'\u2588\u2591\u2591\u2591';
}

function showNetworks(nets){
  var sel=document.getElementById('networks');
  sel.innerHTML='<option value="">-- Sélectionner un réseau --</option>';
  if(!nets||nets.length===0){
    sel.style.display='none';
    document.getElementById('msg').textContent='Aucun réseau trouvé';
    document.getElementById('msg').style.color='#ff9800';
    return;
  }
  for(var i=0;i<nets.length;i++){
    var n=nets[i];
    var opt=document.createElement('option');
    opt.value=n.ssid;
    opt.textContent=n.ssid+' '+rssiBar(n.rssi)+(n.secured?' \uD83D\uDD12':'');
    sel.appendChild(opt);
  }
  sel.style.display='block';
}

function pickNetwork(ssid){
  if(ssid){
    document.getElementById('ssid').value=ssid;
    document.getElementById('pwd').focus();
  }
}

async function save(){
  var ssid=document.getElementById('ssid').value.trim();
  var pwd=document.getElementById('pwd').value.trim();
  var msg=document.getElementById('msg');
  if(!ssid){msg.textContent='SSID requis';msg.style.color='#f44';return;}
  msg.textContent='Connexion en cours...';msg.style.color='#4fc3f7';
  try{
    var r=await fetch('http://'+_ip+'/api/wifi',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({ssid:ssid,pwd:pwd})});
    if(r.ok){msg.textContent='\u2713 Enregistré — redémarrage...';msg.style.color='#4caf50';}
    else{msg.textContent='Erreur serveur';msg.style.color='#f44';}
  }catch(e){msg.textContent='Erreur réseau';msg.style.color='#f44';}
}
</script></body></html>
)rawhtml";

// ═══════════════════════════════════════════════════════════════
//  begin()
// ═══════════════════════════════════════════════════════════════
void WebManager::begin(NTPManager* ntp, WeatherManager* weather,
                       RelaisManager* relais, ScheduleManager* schedule,
                       ConfigManager* config, WiFiManager* wifi) {
    _ntp      = ntp;
    _weather  = weather;
    _relais   = relais;
    _schedule = schedule;
    _config   = config;
    _wifi     = wifi;
    // Invariant I1 : LittleFS déjà monté par ConfigManager
    setupRoutes();
    _server.begin();
    Serial.println("[Web] Serveur démarré port 80");
}

// ═══════════════════════════════════════════════════════════════
//  Routes
// ═══════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────
//  update — opérations différées hors de la tâche AsyncTCP
// ─────────────────────────────────────────────────────────────
void WebManager::update() {
    if (!_systemSavePending) return;
    if ((int32_t)(millis() - _systemSaveAtMs) < 0) return;

    CfgSystem pending;
    uint16_t manualDuration = 10;
    bool manualDurationValid = false;
    bool rebootAfter = false;

    portENTER_CRITICAL(&_pendingMux);
    if (!_systemSavePending) {
        portEXIT_CRITICAL(&_pendingMux);
        return;
    }
    pending = _pendingSystem;
    manualDuration = _pendingManualDuration;
    manualDurationValid = _pendingManualDurationValid;
    rebootAfter = _pendingSystemReboot;
    _systemSavePending = false;
    _pendingManualDurationValid = false;
    portEXIT_CRITICAL(&_pendingMux);

    EventLog::log(LOG_INFO, "Config: application differee (%u zones, manuel=%u min)",
                  pending.nbZones,
                  manualDurationValid ? manualDuration : _config->manual().durationMin);

    if (manualDurationValid) {
        _schedule->setManualDuration(manualDuration);
        _config->setSystemAndManualDuration(pending, manualDuration);
    } else {
        _config->setSystem(pending);
    }

    if (rebootAfter) {
        EventLog::log(LOG_INFO, "Systeme: redemarrage apres sauvegarde");
        delay(100);
        ESP.restart();
    }
}

void WebManager::setupRoutes() {
    // ── Détection portail captif (iOS/Android/Windows) ──────────
    // Stratégie : répondre de façon à ce que chaque OS détecte un portail
    // et ouvre automatiquement le navigateur captif.
    //
    // iOS/macOS : hotspot-detect.html → redirection vers /setup
    // Android   : generate_204 → 302 redirect (attendait 204 = pas de portail)
    // Windows   : connecttest.txt → redirection (attendait "Microsoft Connect Test")
    //
    // Le onNotFound redirige tout le reste vers /setup en mode captif.
    // Ces handlers évitent les erreurs LittleFS sur ces URLs connues.

    auto captiveRedirect = [this](AsyncWebServerRequest* req) {
        String url = "http://";
        url += (_wifi && _wifi->isCaptivePortal())
               ? _wifi->getApIP().toString()
               : req->host();
        url += "/setup";
        req->redirect(url);
    };

    _server.on("/hotspot-detect.html",   HTTP_GET, captiveRedirect);
    _server.on("/generate_204",          HTTP_GET, captiveRedirect);
    _server.on("/gen_204",               HTTP_GET, captiveRedirect);
    _server.on("/connecttest.txt",       HTTP_GET, captiveRedirect);
    _server.on("/redirect",              HTTP_GET, captiveRedirect);
    _server.on("/success.txt",           HTTP_GET, captiveRedirect);
    _server.on("/ncsi.txt",              HTTP_GET, captiveRedirect);
    _server.on("/canonical.html",        HTTP_GET, captiveRedirect);
    _server.on("/chat",                  HTTP_GET, captiveRedirect);

    // Racine
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/index.html");
    });

    // Page portail captif — accessible même sans LittleFS
    _server.on("/setup", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", CAPTIVE_HTML);
    });
    // Redirect captif — répond à toute URL inconnue en mode AP
    _server.onNotFound([this](AsyncWebServerRequest* req) {
        if (_wifi && _wifi->isCaptivePortal()) {
            req->redirect("http://" + _wifi->getApIP().toString() + "/setup");
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });

    // ── GET ───────────────────────────────────
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleStatus(req);
    });
    _server.on("/api/adminStatus", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleAdminStatus(req);
    });
    // Slots d une zone en particulier (demande a la demande depuis le modal)
    // Route query param : /api/zone?z=N — evite les problemes de regex AsyncWebServer
    _server.on("/api/zone", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!req->hasParam("z")) { sendError(req, "parametre z manquant"); return; }
        uint8_t z = (uint8_t)req->getParam("z")->value().toInt();
        if (!_config || z >= _config->nbZones()) { sendError(req, "zone invalide"); return; }
        JsonDocument doc;
        if (_schedule) {
            ZoneSchedule zs = _schedule->getZoneSchedule(z);
            JsonArray days = doc["daySlots"].to<JsonArray>();
            for (uint8_t d = 0; d < NB_DAYS; d++) {
                JsonArray row = days.add<JsonArray>();
                for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                    const TimeSlot& ts = zs.daySlots[d].slots[s];
                    JsonObject so = row.add<JsonObject>();
                    so["h"] = ts.hour; so["m"] = ts.minute;
                    so["d"] = ts.duration; so["e"] = ts.enabled;
                }
            }
            JsonArray isl = doc["intervalSlots"].to<JsonArray>();
            for (uint8_t s = 0; s < MAX_SLOTS; s++) {
                const TimeSlot& ts = zs.intervalSlots.slots[s];
                JsonObject so = isl.add<JsonObject>();
                so["h"] = ts.hour; so["m"] = ts.minute;
                so["d"] = ts.duration; so["e"] = ts.enabled;
            }
        }
        sendJson(req, doc);
    });

    // ── POST planning ─────────────────────────
    _server.on("/api/display", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleGetDisplay(req);
    });

#define POST_JSON(uri, handler) \
    addJsonHandler(uri, [this](AsyncWebServerRequest* req, JsonVariant& jv) { \
        JsonDocument doc; doc.set(jv); handler(req, doc); \
    })

    POST_JSON("/api/mode",          handleSetMode);
    POST_JSON("/api/interval",      handleSetInterval);
    POST_JSON("/api/dayslot",       handleSetDaySlot);
    POST_JSON("/api/intervalslot",  handleSetIntervalSlot);
    POST_JSON("/api/rain",          handleSetRain);
    POST_JSON("/api/manual",        handleManual);
    POST_JSON("/api/manualDuration",handleSetManualDuration);
    POST_JSON("/api/saveSchedule",  handleSaveSchedule);

    // ── POST config (v2) ──────────────────────
    POST_JSON("/api/wifi",          handleSetWifi);
    POST_JSON("/api/touch",         handleSetTouch);
    POST_JSON("/api/ntp",           handleSetNtp);
    POST_JSON("/api/owm",           handleSetOwm);
    POST_JSON("/api/system",        handleSetSystem);
    POST_JSON("/api/zoneName",      handleSetZoneName);
    POST_JSON("/api/display",       handleSetDisplay);

#undef POST_JSON

    _server.on("/api/captive", HTTP_POST, [this](AsyncWebServerRequest* req) {
        handleStartCaptive(req);
    });
    _server.on("/api/resetConfig", HTTP_POST, [this](AsyncWebServerRequest* req) {
        handleResetConfig(req);
    });
    // Scan réseau WiFi — portail captif
    _server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleWifiScan(req);
    });

    // Journal d'événements — lien caché, diagnostic uniquement
    _server.on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleGetLogs(req);
    });

    // Fichiers statiques LittleFS
    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/status
// ═══════════════════════════════════════════════════════════════
void WebManager::handleStatus(AsyncWebServerRequest* req) {
    JsonDocument doc;

    doc["time"]    = _ntp->getTimeStr();
    doc["synced"]  = _ntp->isSynced();
    doc["uptime"]  = millis() / 1000UL;
    doc["heap"]    = ESP.getFreeHeap();

    JsonObject weather = doc["weather"].to<JsonObject>();
    weather["rainExpected"] = _weather->isRainExpected();
    weather["rainMm"]       = _weather->getRainMm();
    weather["tempC"]        = _weather->getTempC();
    weather["status"]       = _weather->getStatusStr();
    weather["fetched"]      = _weather->hasFetched();

    // Prévisions J+0..J+4
    JsonArray forecast = doc["forecast"].to<JsonArray>();
    for (uint8_t i = 0; i < 5; i++) {
        ForecastDay fd = _weather->getForecastDay(i);
        JsonObject fo  = forecast.add<JsonObject>();
        fo["rainMm"]          = fd.rainMm;
        fo["tempMax"]         = fd.tempMax;
        fo["tempMin"]         = fd.tempMin;
        fo["feelsLikeMax"]    = fd.feelsLikeMax;
        fo["rainProbability"] = fd.rainProbability;
        fo["humidityMax"]     = fd.humidityMax;
        fo["windMaxKmh"]      = fd.windMaxKmh;
        fo["gustMaxKmh"]      = fd.gustMaxKmh;
        fo["cloudsMax"]       = fd.cloudsMax;
        fo["pressureAvg"]     = fd.pressureAvg;
        fo["description"]     = fd.description;
        fo["icon"]            = fd.icon;
        fo["valid"]           = fd.valid;
    }

    // Zones
    JsonArray zones = doc["zones"].to<JsonArray>();
    const uint8_t nbZ = _config ? _config->nbZones() : NB_ZONES;
    for (uint8_t z = 0; z < nbZ; z++) {
        JsonObject zo  = zones.add<JsonObject>();
        zo["active"]   = _relais   ? _relais->getState(z)      : false;
        zo["elapsed"]  = _schedule ? _schedule->getElapsedMs(z) : 0;
        zo["remaining"]= _schedule ? _schedule->getRemainingMs(z): 0;
        zo["schedActive"] = _schedule ? _schedule->isZoneActive(z) : false;
        zo["reason"]   = _schedule ? _schedule->getLastReason(z).c_str() : "";
        if (_config) zo["name"] = _config->zone(z).name;

        ZoneSchedule zs = _schedule->getZoneSchedule(z);
        zo["mode"]            = zs.mode;
        zo["intervalDays"]    = zs.intervalDays;
        zo["lastWateredDay"]  = zs.lastWateredDay;

        JsonObject rain = zo["rain"].to<JsonObject>();
        rain["threshMm"] = zs.rain.thresholdMm;
        rain["hours"]    = zs.rain.forecastHours;

        // daySlots et intervalSlots exclus du status global (trop volumineux pour N zones)
        // Ils sont disponibles via GET /api/zone?z=N
    }

    doc["manualDurationMin"] = _schedule->getManualDurationMin();

    sendJson(req, doc);
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/adminStatus — état système pour page ADMIN web
// ═══════════════════════════════════════════════════════════════
void WebManager::handleAdminStatus(AsyncWebServerRequest* req) {
    JsonDocument doc;

    // Système
    doc["uptime"]   = millis() / 1000UL;
    doc["heap"]     = ESP.getFreeHeap();
    doc["heapMin"]  = ESP.getMinFreeHeap();
    doc["chipRev"]  = ESP.getChipRevision();

    // WiFi
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    if (_wifi) {
        wifi["state"] = _wifi->stateStr();
        wifi["rssi"]  = _wifi->getRssi();
        wifi["ip"]    = _wifi->isConnected()
                        ? _wifi->getIP().toString()
                        : (_wifi->isCaptivePortal()
                           ? _wifi->getApIP().toString()
                           : "");
    }
    if (_config) wifi["ssid"] = _config->wifi().ssid;

    // NTP
    if (_config) {
        JsonObject ntp = doc["ntp"].to<JsonObject>();
        ntp["server"]    = _config->ntp().server;
        ntp["gmtOffset"] = _config->ntp().gmtOffset;
        ntp["dstOffset"] = _config->ntp().dstOffset;
        ntp["synced"]    = _ntp->isSynced();
        ntp["time"]      = _ntp->getTimeStr();
    }

    // OWM
    if (_config) {
        JsonObject owm = doc["owm"].to<JsonObject>();
        // Masquer la clé : montrer seulement les 4 premiers chars
        const char* key = _config->owm().apiKey;
        char masked[12] = "****";
        if (strlen(key) > 4) {
            strncpy(masked, key, 4);
            masked[4] = '\0';
            strcat(masked, "****");
        }
        owm["apiKeyMasked"] = masked;
        owm["hasKey"]  = (key[0] != '\0');
        owm["lat"]     = _config->owm().lat;
        owm["lon"]     = _config->owm().lon;
        owm["units"]   = _config->owm().units;
        owm["city"]    = _config->owm().city;
        owm["country"] = _config->owm().country;
        owm["fetched"] = _weather->hasFetched();
    }

    // Système config
    if (_config) {
        JsonObject sys = doc["system"].to<JsonObject>();
        sys["maxWateringMin"]  = _config->system().maxWateringMin;
        sys["screenTimeout"]   = _config->system().screenTimeoutMin;
        sys["ledMode"]         = _config->system().ledMode;
        sys["nbZones"]         = _config->system().nbZones;
        sys["nbRelais"]        = _config->system().nbRelaisPhysical;
        sys["relayLogic"]      = _config->system().relayLogic;
        sys["relayController"] = _config->system().relayController;
        sys["maxZones"]        = (uint8_t)MAX_ZONES;
    }

    // Noms de zones
    if (_config) {
        JsonArray znames = doc["zoneNames"].to<JsonArray>();
        for (uint8_t z = 0; z < (_config ? _config->nbZones() : NB_ZONES); z++) {
            znames.add(_config->zone(z).name);
        }
    }

    sendJson(req, doc);
}

// ═══════════════════════════════════════════════════════════════
//  POST planning
// ═══════════════════════════════════════════════════════════════

void WebManager::handleSetMode(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone = doc["zone"] | 255;
    uint8_t mode = doc["mode"] | 255;
    if (zone >= MAX_ZONES || mode > 1) { sendError(req, "parametres invalides"); return; }
    _schedule->setMode(zone, mode);                          // RAM
    if (_config) _config->setZoneMode(zone, mode);          // flash
    EventBus::displayDirty = true;
    sendOk(req);
}

void WebManager::handleSetInterval(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone = doc["zone"]     | 255;
    uint8_t days = doc["interval"] | 0;
    if (zone >= MAX_ZONES || days < 1 || days > 30) { sendError(req, "parametres invalides"); return; }
    _schedule->setIntervalDays(zone, days);
    if (_config) _config->setZoneIntervalDays(zone, days);
    sendOk(req);
}

void WebManager::handleSetDaySlot(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone    = doc["zone"]    | 255;
    uint8_t day     = doc["day"]     | 255;
    uint8_t slotIdx = doc["slotIdx"] | 255;
    if (zone >= MAX_ZONES || day >= NB_DAYS || slotIdx >= MAX_SLOTS) {
        sendError(req, "parametres invalides"); return;
    }
    uint8_t  h   = doc["hour"]     | 6;
    uint8_t  m   = doc["minute"]   | 0;
    uint16_t dur = doc["duration"] | 5;
    bool     en  = doc["enabled"]  | false;
    _schedule->setDaySlot(zone, day, slotIdx, h, m, dur, en);
    if (_config) _config->setZoneDaySlot(zone, day, slotIdx, h, m, dur, en);
    EventBus::displayDirty = true;
    sendOk(req);
}

void WebManager::handleSetIntervalSlot(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t  zone    = doc["zone"]    | 255;
    uint8_t  slotIdx = doc["slotIdx"] | 255;
    if (zone >= MAX_ZONES || slotIdx >= MAX_SLOTS) { sendError(req, "parametres invalides"); return; }
    uint8_t  h   = doc["hour"]     | 6;
    uint8_t  m   = doc["minute"]   | 0;
    uint16_t dur = doc["duration"] | 5;
    bool     en  = doc["enabled"]  | false;
    _schedule->setIntervalSlot(zone, slotIdx, h, m, dur, en);
    if (_config) _config->setZoneIntervalSlot(zone, slotIdx, h, m, dur, en);
    EventBus::displayDirty = true;
    sendOk(req);
}

void WebManager::handleSetRain(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone = doc["zone"] | 255;
    if (zone >= MAX_ZONES) { sendError(req, "zone invalide"); return; }
    float   thr  = doc["threshold"] | DEFAULT_RAIN_THRESHOLD;
    uint8_t hrs  = doc["hours"]     | DEFAULT_FORECAST_HOURS;
    _schedule->setRainConfig(zone, thr, hrs);
    if (_config) _config->setZoneRain(zone, thr, hrs);
    sendOk(req);
}

void WebManager::handleManual(AsyncWebServerRequest* req, JsonDocument& doc) {
    uint8_t zone  = doc["zone"]  | 255;
    bool    state = doc["state"] | false;
    if (zone >= MAX_ZONES) { sendError(req, "zone invalide"); return; }
    if (state) _schedule->startManualWatering(zone);
    else       _schedule->stopManualWatering(zone);
    EventBus::displayDirty = true;
    sendOk(req);
}

void WebManager::handleSetManualDuration(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    uint16_t min = doc["minutes"] | 0;
    if (min == 0 || min > 120) { sendError(req, "duree invalide (1-120)"); return; }

    // Ne jamais écrire LittleFS depuis le callback AsyncTCP.
    portENTER_CRITICAL(&_pendingMux);
    _pendingSystem = _config->system();
    _pendingManualDuration = min;
    _pendingManualDurationValid = true;
    _pendingSystemReboot = false;
    _systemSaveAtMs = millis() + 500;
    _systemSavePending = true;
    portEXIT_CRITICAL(&_pendingMux);

    sendOk(req);
}

void WebManager::handleSaveSchedule(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    uint8_t zone = doc["zone"] | 255;
    const uint8_t nbZs = _config ? _config->nbZones() : NB_ZONES;
    if (zone < nbZs) {
        _config->syncZoneFromSchedule(zone, _schedule->getZoneSchedule(zone));
    } else {
        for (uint8_t z = 0; z < nbZs; z++)
            _config->syncZoneFromSchedule(z, _schedule->getZoneSchedule(z));
    }
    sendOk(req);
}

// ═══════════════════════════════════════════════════════════════
//  POST config v2
// ═══════════════════════════════════════════════════════════════

void WebManager::handleSetWifi(AsyncWebServerRequest* req, JsonDocument& doc) {
    const char* ssid = doc["ssid"] | "";
    const char* pwd = doc["pwd"] | (doc["password"] | "");

    // Trim défensif — supprime espaces parasites en début/fin
    // (peuvent venir de l'autocomplete mobile ou du copier-coller)
    char ssidBuf[64]; strlcpy(ssidBuf, ssid, sizeof(ssidBuf));
    char pwdBuf[64];  strlcpy(pwdBuf,  pwd,  sizeof(pwdBuf));
    // Trim début
    auto trimStr = [](char* s) {
        char* start = s;
        while (*start == ' ') start++;
        if (start != s) memmove(s, start, strlen(start) + 1);
        // Trim fin
        int len = strlen(s);
        while (len > 0 && s[len-1] == ' ') s[--len] = '\0';
    };
    trimStr(ssidBuf);
    trimStr(pwdBuf);
    ssid = ssidBuf;
    pwd  = pwdBuf;

    if (strlen(ssid) == 0) { sendError(req, "ssid vide"); return; }

    // Dump diagnostic Serial — visible même sans WiFi
    Serial.printf("[Web] handleSetWifi SSID='%s' PWD_len=%d PWD='%s'\n",
                  ssid, strlen(pwd), pwd);

    if (!_config) {
        sendError(req, "config indisponible");
        EventLog::log(LOG_ERROR, "WiFi: ConfigManager absent — identifiants non sauvegardes");
        return;
    }

    sendOk(req);  // Invariant I10 : répondre AVANT le reboot
    EventLog::log(LOG_INFO, "WiFi: sauvegarde identifiants via NVS");
    _config->setWifi(ssid, pwd);  // inclut save() NVS + ESP.restart()
}

void WebManager::handleSetTouch(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    _config->setTouchCalib(
        doc["xMin"] | (int)TOUCH_X_MIN,
        doc["xMax"] | (int)TOUCH_X_MAX,
        doc["yMin"] | (int)TOUCH_Y_MIN,
        doc["yMax"] | (int)TOUCH_Y_MAX
    );
    sendOk(req);
}

void WebManager::handleSetNtp(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    const char* server = doc["server"] | "pool.ntp.org";
    int32_t gmt = doc["gmtOffset"] | (int32_t)3600;
    int32_t dst = doc["dstOffset"] | (int32_t)3600;
    _config->setNtp(server, gmt, dst);
    // EventBus::configDirty positionné dans ConfigManager::setNtp()
    sendOk(req);
}

void WebManager::handleSetOwm(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    const char* apiKey  = doc["apiKey"]  | "";
    float lat           = doc["lat"]     | 0.0f;
    float lon           = doc["lon"]     | 0.0f;
    const char* units   = doc["units"]   | "metric";
    const char* city    = doc["city"]    | "";
    const char* country = doc["country"] | "FR";
    _config->setOwm(apiKey, lat, lon, units, city, country);
    sendOk(req);
}

void WebManager::handleSetSystem(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }

    // Construire une copie complète, appliquer les seuls champs reçus,
    // puis effectuer UNE sauvegarde LittleFS. Les anciens setters provoquaient
    // jusqu'à trois écritures successives pendant une bascule de zones.
    CfgSystem next = _config->system();

    if (doc["maxWateringMin"].is<uint16_t>() || doc["maxWateringMin"].is<int>()) {
        next.maxWateringMin = doc["maxWateringMin"] | (uint16_t)60;
    }
    if (doc["screenTimeout"].is<uint8_t>() || doc["screenTimeout"].is<int>()) {
        next.screenTimeoutMin = doc["screenTimeout"] | (uint8_t)5;
    }
    if (doc["ledMode"].is<uint8_t>() || doc["ledMode"].is<int>()) {
        next.ledMode = constrain((uint8_t)(doc["ledMode"] | 1), (uint8_t)0, (uint8_t)4);
    }
    if (doc["relayLogic"].is<uint8_t>() || doc["relayLogic"].is<int>()) {
        next.relayLogic = constrain((uint8_t)(doc["relayLogic"] | 1), (uint8_t)0, (uint8_t)1);
    }
    if (doc["relayController"].is<uint8_t>() || doc["relayController"].is<int>()) {
        next.relayController = constrain((uint8_t)(doc["relayController"] | RELAY_CONTROLLER_XL9535),
                                         (uint8_t)RELAY_CONTROLLER_XL9535,
                                         (uint8_t)RELAY_CONTROLLER_MCP23017);
    }

    const uint8_t oldNbZones = _config->nbZones();
    if (doc["nbZones"].is<uint8_t>() || doc["nbZones"].is<int>()) {
        uint8_t requested = constrain((uint8_t)(doc["nbZones"] | oldNbZones),
                                      (uint8_t)1, (uint8_t)MAX_ACTIVE_ZONES);
        if (next.relayController == RELAY_CONTROLLER_XL9535) {
            // Configuration provisoire XL9535 : cartes ajoutées par paires de sorties.
            requested = constrain((uint8_t)((requested + 1U) & 0xFEU), (uint8_t)2, (uint8_t)8);
        }
        next.nbZones = requested;
    }

    // Invariant matériel AquaLook : une zone correspond exactement à une sortie relais.
    next.nbRelaisPhysical = next.nbZones;

    const bool needReboot = (next.nbZones != oldNbZones) ||
                            (next.relayController != _config->system().relayController) ||
                            (next.relayLogic != _config->system().relayLogic);

    uint16_t manualDuration = _config->manual().durationMin;
    bool manualDurationValid = false;
    if (doc["manualDurationMin"].is<uint16_t>() || doc["manualDurationMin"].is<int>()) {
        manualDuration = constrain((uint16_t)(doc["manualDurationMin"] | manualDuration),
                                   (uint16_t)1, (uint16_t)120);
        manualDurationValid = true;
    }

    // Ne jamais écrire LittleFS depuis le callback AsyncTCP. On copie la
    // demande puis on répond immédiatement. update() effectuera la sauvegarde
    // dans la tâche Arduino, après que la réponse HTTP a quitté la pile réseau.
    portENTER_CRITICAL(&_pendingMux);
    _pendingSystem = next;
    _pendingManualDuration = manualDuration;
    _pendingManualDurationValid = manualDurationValid;
    _pendingSystemReboot = needReboot;
    _systemSaveAtMs = millis() + 500;
    _systemSavePending = true;
    portEXIT_CRITICAL(&_pendingMux);

    sendOk(req);
}

void WebManager::handleSetZoneName(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    uint8_t     zone = doc["zone"] | 255;
    const char* name = doc["name"] | "";
    if (zone >= MAX_ZONES || strlen(name) == 0) { sendError(req, "parametres invalides"); return; }
    _config->setZoneName(zone, name);
    // EventBus::displayDirty positionné dans ConfigManager::setZoneName()
    sendOk(req);
}

void WebManager::handleStartCaptive(AsyncWebServerRequest* req) {
    sendOk(req);
    EventBus::captiveRequested = true;  // WiFiManager le consomme dans update()
}

void WebManager::handleResetConfig(AsyncWebServerRequest* req) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    sendOk(req);
    _config->resetPersistent();
    delay(200);
    ESP.restart();
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/display — retourne les tokens de design LCD courants
// ═══════════════════════════════════════════════════════════════
void WebManager::handleGetDisplay(AsyncWebServerRequest* req) {
    if (!_config) { sendError(req, "config indisponible"); return; }
    const CfgDisplay& d = _config->display();
    JsonDocument doc;
    // Couleurs
    doc["cBg"]        = d.cBg;       doc["cSurface"]   = d.cSurface;
    doc["cSurface2"]  = d.cSurface2; doc["cBorder"]    = d.cBorder;
    doc["cText"]      = d.cText;     doc["cText2"]     = d.cText2;
    doc["cMuted"]     = d.cMuted;    doc["cActiveBg"]  = d.cActiveBg;
    doc["cZone0"]     = d.cZone0;    doc["cZone1"]     = d.cZone1;
    doc["cZone2"]     = d.cZone2;    doc["cZone3"]     = d.cZone3;
    // Formes
    doc["rSm"]        = d.rSm;       doc["rMd"]        = d.rMd;
    doc["rLg"]        = d.rLg;       doc["accentBarW"] = d.accentBarW;
    // Timing
    doc["refreshNomMs"] = d.refreshNomMs;
    doc["refreshActMs"] = d.refreshActMs;
    // Layout
    doc["planGap"]    = d.planGap;
    doc["g2Gpad"]     = d.g2Gpad;
    doc["g4Gpad"]     = d.g4Gpad;
    // Options météo LCD
    doc["showWeatherIcon"] = d.showWeatherIcon;
    doc["showWeatherTemp"] = d.showWeatherTemp;
    doc["weatherVisualsEnabled"] = _config->weatherVisualsEnabled();
    doc["weatherTipCondition"] = d.weatherTipCondition;
    doc["weatherTipTemp"]      = d.weatherTipTemp;
    doc["weatherTipRain"]      = d.weatherTipRain;
    doc["weatherTipPop"]       = d.weatherTipPop;
    doc["weatherTipHumidity"]  = d.weatherTipHumidity;
    doc["weatherTipWind"]      = d.weatherTipWind;
    doc["weatherTipGust"]      = d.weatherTipGust;
    doc["weatherTipClouds"]    = d.weatherTipClouds;
    doc["weatherTipPressure"]  = d.weatherTipPressure;
    sendJson(req, doc);
}

// ═══════════════════════════════════════════════════════════════
//  POST /api/display — met à jour les tokens de design LCD
//  Hot-reload : aucun reboot — EventBus::displayDirty déclenche
//  applyDisplayConfig() au prochain cycle DisplayManager::update().
// ═══════════════════════════════════════════════════════════════
void WebManager::handleSetDisplay(AsyncWebServerRequest* req, JsonDocument& doc) {
    if (!_config) { sendError(req, "config indisponible"); return; }

    // Partir de la config courante pour ne patcher que les champs présents
    CfgDisplay d = _config->display();

    // Helper de validation couleur — accepte uniquement #rrggbb (7 chars)
    auto setColor = [](const char* src, char* dst) {
        if (src && src[0] == '#' && strlen(src) == 7) strlcpy(dst, src, 8);
    };
    setColor(doc["cBg"]       | "", d.cBg);
    setColor(doc["cSurface"]  | "", d.cSurface);
    setColor(doc["cSurface2"] | "", d.cSurface2);
    setColor(doc["cBorder"]   | "", d.cBorder);
    setColor(doc["cText"]     | "", d.cText);
    setColor(doc["cText2"]    | "", d.cText2);
    setColor(doc["cMuted"]    | "", d.cMuted);
    setColor(doc["cActiveBg"] | "", d.cActiveBg);
    setColor(doc["cZone0"]    | "", d.cZone0);
    setColor(doc["cZone1"]    | "", d.cZone1);
    setColor(doc["cZone2"]    | "", d.cZone2);
    setColor(doc["cZone3"]    | "", d.cZone3);

    if (doc["rSm"].is<int>())        d.rSm        = constrain((uint8_t)(doc["rSm"]  | 4),  1, 20);
    if (doc["rMd"].is<int>())        d.rMd        = constrain((uint8_t)(doc["rMd"]  | 6),  1, 20);
    if (doc["rLg"].is<int>())        d.rLg        = constrain((uint8_t)(doc["rLg"]  | 10), 1, 30);
    if (doc["accentBarW"].is<int>()) d.accentBarW = constrain((uint8_t)(doc["accentBarW"] | 3), 1, 8);

    if (doc["refreshNomMs"].is<int>()) {
        uint16_t v = doc["refreshNomMs"] | (uint16_t)5000;
        d.refreshNomMs = constrain(v, (uint16_t)500, (uint16_t)30000);
    }
    if (doc["refreshActMs"].is<int>()) {
        uint16_t v = doc["refreshActMs"] | (uint16_t)1000;
        d.refreshActMs = constrain(v, (uint16_t)200, (uint16_t)5000);
    }
    if (doc["planGap"].is<int>()) d.planGap = constrain((uint8_t)(doc["planGap"] | 6), (uint8_t)0, (uint8_t)20);
    if (doc["g2Gpad"].is<int>())  d.g2Gpad  = constrain((uint8_t)(doc["g2Gpad"]  | 1), (uint8_t)0, (uint8_t)8);
    if (doc["g4Gpad"].is<int>())  d.g4Gpad  = constrain((uint8_t)(doc["g4Gpad"]  | 1), (uint8_t)0, (uint8_t)8);
    if (doc["showWeatherIcon"].is<bool>()) d.showWeatherIcon = doc["showWeatherIcon"];
    if (doc["showWeatherTemp"].is<bool>()) d.showWeatherTemp = doc["showWeatherTemp"];
    if (doc["weatherVisualsEnabled"].is<bool>())
        _config->setWeatherVisualsEnabled(doc["weatherVisualsEnabled"]);
    if (doc["weatherTipCondition"].is<bool>()) d.weatherTipCondition = doc["weatherTipCondition"];
    if (doc["weatherTipTemp"].is<bool>())      d.weatherTipTemp      = doc["weatherTipTemp"];
    if (doc["weatherTipRain"].is<bool>())      d.weatherTipRain      = doc["weatherTipRain"];
    if (doc["weatherTipPop"].is<bool>())       d.weatherTipPop       = doc["weatherTipPop"];
    if (doc["weatherTipHumidity"].is<bool>())  d.weatherTipHumidity  = doc["weatherTipHumidity"];
    if (doc["weatherTipWind"].is<bool>())      d.weatherTipWind      = doc["weatherTipWind"];
    if (doc["weatherTipGust"].is<bool>())      d.weatherTipGust      = doc["weatherTipGust"];
    if (doc["weatherTipClouds"].is<bool>())    d.weatherTipClouds    = doc["weatherTipClouds"];
    if (doc["weatherTipPressure"].is<bool>())  d.weatherTipPressure  = doc["weatherTipPressure"];

    _config->setDisplay(d);   // save() + EventBus::displayDirty = true
    sendOk(req);
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/wifi/scan
//
//  Comportement en 2 temps :
//    1er appel  → lance le scan asynchrone, répond {scanning:true}
//    Appels suivants tant que scan en cours → {scanning:true}
//    Quand terminé → répond la liste triée par RSSI, dédoublonnée,
//                    puis libère la mémoire (clearScan)
//
//  Format réponse finale :
//    { scanning:false, networks:[{ssid,rssi,secured}, ...] }
//
//  Le client doit poller /api/wifi/scan toutes les ~500 ms
//  jusqu'à recevoir scanning:false.
// ═══════════════════════════════════════════════════════════════
void WebManager::handleWifiScan(AsyncWebServerRequest* req) {
    if (!_wifi) { sendError(req, "wifi indisponible"); return; }

    int16_t n = _wifi->getScanCount();

    if (n < 0) {
        // Scan en cours (lancé par un appel précédent)
        req->send(200, "application/json", "{\"scanning\":true}");
        return;
    }

    // n == 0 et scan non lancé (getScanCount retourne 0 si !_scanPending)
    if (n == 0) {
        // Aucun scan lancé — on le démarre maintenant
        _wifi->startScan();
        req->send(200, "application/json", "{\"scanning\":true}");
        return;
    }

    // Scan terminé — construire la réponse dédoublonnée, triée par RSSI
    JsonDocument doc;
    doc["scanning"] = false;
    JsonArray networks = doc["networks"].to<JsonArray>();

    // Tri par RSSI décroissant + dédoublonnage SSID
    // On utilise un tableau d'indices triés (N est petit, bubble sort suffit)
    uint8_t count = (uint8_t)n;
    uint8_t order[64];  // max 63 réseaux — limite raisonnable
    if (count > 64) count = 64;
    for (uint8_t i = 0; i < count; i++) order[i] = i;

    // Tri bulle sur RSSI (décroissant = meilleur signal en premier)
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - 1 - i; j++) {
            if (_wifi->getScanEntry(order[j]).rssi < _wifi->getScanEntry(order[j+1]).rssi) {
                uint8_t tmp = order[j]; order[j] = order[j+1]; order[j+1] = tmp;
            }
        }
    }

    // Dédoublonnage : ne garder que la première occurrence de chaque SSID
    bool seen[64] = {};
    for (uint8_t i = 0; i < count; i++) {
        WiFiManager::ScanEntry e = _wifi->getScanEntry(order[i]);
        if (e.ssid[0] == '\0') continue;  // réseau caché — ignorer

        // Vérifier si ce SSID a déjà été ajouté
        bool duplicate = false;
        for (uint8_t j = 0; j < i; j++) {
            if (seen[j] && strcmp(_wifi->getScanEntry(order[j]).ssid, e.ssid) == 0) {
                duplicate = true;
                break;
            }
        }
        seen[i] = !duplicate;
        if (duplicate) continue;

        JsonObject net = networks.add<JsonObject>();
        net["ssid"]    = e.ssid;
        net["rssi"]    = e.rssi;
        net["secured"] = e.secured;
    }

    // Libérer la mémoire driver
    _wifi->clearScan();

    sendJson(req, doc);
}

// ═══════════════════════════════════════════════════════════════
//  GET /api/logs — journal d'événements session courante
//
//  Retourne une page HTML auto-rafraîchie (10s) listant toutes
//  les entrées INFO/WARN/ERROR depuis le démarrage.
//  URL non référencée dans l'interface — accès par lien direct.
//  Lien : http://<ip>/api/logs
// ═══════════════════════════════════════════════════════════════
void WebManager::handleGetLogs(AsyncWebServerRequest* req) {
    // Construire la page HTML dans un String (taille max ~8KB pour 60 entrées)
    String html;
    html.reserve(4096);

    html += F("<!DOCTYPE html><html lang='fr'><head>"
              "<meta charset='UTF-8'>"
              "<meta http-equiv='refresh' content='10'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>AquaLook — Logs</title>"
              "<style>"
              "body{font-family:monospace;background:#0a0a0a;color:#ccc;margin:0;padding:1rem}"
              "h2{color:#4fc3f7;margin-bottom:.5rem}"
              ".sub{color:#556;font-size:.8rem;margin-bottom:1rem}"
              "table{width:100%;border-collapse:collapse;font-size:.85rem}"
              "th{text-align:left;color:#556;padding:.3rem .5rem;border-bottom:1px solid #222}"
              "td{padding:.3rem .5rem;border-bottom:1px solid #181818;vertical-align:top}"
              ".e{color:#f44336}.w{color:#ff9800}.i{color:#9e9e9e}"
              ".tag{display:inline-block;padding:1px 5px;border-radius:3px;font-size:.75rem;margin-right:4px}"
              ".te{background:#f4433622;color:#f44336}"
              ".tw{background:#ff980022;color:#ff9800}"
              ".ti{background:#33333344;color:#9e9e9e}"
              "</style></head><body>");

    html += F("<h2>&#128220; Journal AquaLook</h2>");
    html += "<div class='sub'>Session courante &mdash; ";
    html += EventLog::count();
    html += " entr&eacute;e(s) &mdash; auto-refresh 10s</div>";

    if (EventLog::count() == 0) {
        html += F("<p style='color:#556'>Aucun &eacute;v&eacute;nement enregistr&eacute;.</p>");
    } else {
        html += F("<table><tr><th>T+</th><th>Niveau</th><th>Message</th></tr>");

        for (uint8_t i = 0; i < EventLog::count(); i++) {
            const LogEntry& e = EventLog::get(i);

            // Temps depuis démarrage HH:MM:SS
            char tBuf[10];
            EventLog::msToHms(e.ms, tBuf, sizeof(tBuf));

            const char* lvlClass  = (e.level == LOG_ERROR) ? "e" :
                                    (e.level == LOG_WARN)  ? "w" : "i";
            const char* tagClass  = (e.level == LOG_ERROR) ? "te" :
                                    (e.level == LOG_WARN)  ? "tw" : "ti";

            html += "<tr><td style='color:#556;white-space:nowrap'>";
            html += tBuf;
            html += "</td><td><span class='tag ";
            html += tagClass;
            html += "'>";
            html += EventLog::levelStr(e.level);
            html += "</span></td><td class='";
            html += lvlClass;
            html += "'>";
            html += e.msg;
            html += "</td></tr>";
        }
        html += F("</table>");
    }

    // Uptime + heap en pied de page
    char upBuf[10];
    EventLog::msToHms(millis(), upBuf, sizeof(upBuf));
    html += F("<div style='margin-top:1.5rem;color:#334;font-size:.75rem'>Uptime : ");
    html += upBuf;
    html += F(" &mdash; Heap libre : ");
    html += ESP.getFreeHeap();
    html += F(" octets</div></body></html>");

    req->send(200, "text/html; charset=utf-8", html);
}

// ═══════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════

void WebManager::sendJson(AsyncWebServerRequest* req,
                           const JsonDocument& doc, int code) {
    // Mesurer d'abord pour éviter la double allocation
    size_t len = measureJson(doc);
    AsyncResponseStream* resp = req->beginResponseStream("application/json", len + 1);
    serializeJson(doc, *resp);
    req->send(resp);
}

void WebManager::sendOk(AsyncWebServerRequest* req) {
    req->send(200, "application/json", "{\"ok\":true}");
}

void WebManager::sendError(AsyncWebServerRequest* req,
                            const char* msg, int code) {
    String body = String("{\"error\":\"") + msg + "\"}";
    req->send(code, "application/json", body);
}

void WebManager::addJsonHandler(const char* uri,
                                 ArJsonRequestHandlerFunction handler) {
    auto* h = new AsyncCallbackJsonWebHandler(uri, handler);
    h->setMethod(HTTP_POST);
    _server.addHandler(h);
}
