from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if new in text:
        print(f"[OK] deja materialise: {path.relative_to(ROOT)}")
        return
    count = text.count(old)
    if count != 1:
        raise RuntimeError(
            f"Bloc source inattendu dans {path.relative_to(ROOT)}: occurrences={count}"
        )
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"[OK] modifie: {path.relative_to(ROOT)}")


main_path = ROOT / "src" / "main.cpp"
main_text = main_path.read_text(encoding="utf-8")
if '#include "RuntimeProfiler.h"' not in main_text:
    replace_once(
        main_path,
        '#include "SystemDiagnostics.h"\n',
        '#include "SystemDiagnostics.h"\n#include "RuntimeProfiler.h"\n',
    )

old_loop = '''void loop() {
    SystemDiagnostics::loopEnter();

    FaultManager::update();
    storageMgr.update();

    wifiMgr.update();
    const bool connected = wifiMgr.isConnected();

    if (connected) {
        ntpMgr.update();
        weatherMgr.update(true);
    }

    if (ntpMgr.isSynced()) {
        scheduleMgr.update(
            ntpMgr.getHour(),
            ntpMgr.getMinute(),
            ntpMgr.getWeekday(),
            ntpMgr.getEpochDay(),
            weatherMgr.getRainMm()
        );
    }

    executionShadowRuntime.update(millis());
    relaisMgr.update();
    webMgr.update();
    displayMgr.update();
    displayPlanningDecorDraw(displayMgr);

    FaultManager::update();
    yield();
    SystemDiagnostics::loopExit();
}
'''

new_loop = '''void loop() {
    SystemDiagnostics::loopEnter();

    uint32_t startedUs = RuntimeProfiler::start();
    FaultManager::update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::FAULTS_PRE, startedUs);

    startedUs = RuntimeProfiler::start();
    storageMgr.update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::STORAGE, startedUs);

    startedUs = RuntimeProfiler::start();
    wifiMgr.update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::WIFI, startedUs);
    const bool connected = wifiMgr.isConnected();

    if (connected) {
        startedUs = RuntimeProfiler::start();
        ntpMgr.update();
        RuntimeProfiler::stop(RuntimeProfiler::Component::NTP, startedUs);

        startedUs = RuntimeProfiler::start();
        weatherMgr.update(true);
        RuntimeProfiler::stop(RuntimeProfiler::Component::WEATHER, startedUs);
    }

    if (ntpMgr.isSynced()) {
        startedUs = RuntimeProfiler::start();
        scheduleMgr.update(
            ntpMgr.getHour(),
            ntpMgr.getMinute(),
            ntpMgr.getWeekday(),
            ntpMgr.getEpochDay(),
            weatherMgr.getRainMm()
        );
        RuntimeProfiler::stop(RuntimeProfiler::Component::SCHEDULE, startedUs);
    }

    startedUs = RuntimeProfiler::start();
    executionShadowRuntime.update(millis());
    RuntimeProfiler::stop(RuntimeProfiler::Component::EQUIPMENT_SHADOW, startedUs);

    startedUs = RuntimeProfiler::start();
    relaisMgr.update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::RELAY, startedUs);

    startedUs = RuntimeProfiler::start();
    webMgr.update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::WEB, startedUs);

    startedUs = RuntimeProfiler::start();
    displayMgr.update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::DISPLAY_MANAGER, startedUs);

    startedUs = RuntimeProfiler::start();
    displayPlanningDecorDraw(displayMgr);
    RuntimeProfiler::stop(RuntimeProfiler::Component::PLANNING_DECOR, startedUs);

    startedUs = RuntimeProfiler::start();
    FaultManager::update();
    RuntimeProfiler::stop(RuntimeProfiler::Component::FAULTS_POST, startedUs);

    startedUs = RuntimeProfiler::start();
    yield();
    RuntimeProfiler::stop(RuntimeProfiler::Component::YIELD, startedUs);

    SystemDiagnostics::loopExit();
}
'''
replace_once(main_path, old_loop, new_loop)

web_path = ROOT / "src" / "WebManager.cpp"
web_text = web_path.read_text(encoding="utf-8")
if '#include "TimeUtils.h"' not in web_text:
    replace_once(
        web_path,
        '#include "SystemDiagnostics.h"\n',
        '#include "SystemDiagnostics.h"\n#include "TimeUtils.h"\n',
    )

old_web_update = '''void WebManager::update() {
    if (!_systemSavePending) return;
    if ((int32_t)(millis() - _systemSaveAtMs) < 0) return;
'''
new_web_update = '''void WebManager::update() {
    const uint32_t nowMs = millis();

    if (_restartPending && AquaLook::Time::deadlineReached(nowMs, _restartAtMs)) {
        _restartPending = false;
        ESP.restart();
        return;
    }

    if (!_systemSavePending) return;
    if (!AquaLook::Time::deadlineReached(nowMs, _systemSaveAtMs)) return;
'''
replace_once(web_path, old_web_update, new_web_update)

old_restart = '''    if (rebootAfter) {
        EventLog::log(LOG_INFO, "Systeme: redemarrage apres sauvegarde");
        delay(100);
        ESP.restart();
    }
'''
new_restart = '''    if (rebootAfter) {
        EventLog::log(LOG_INFO, "Systeme: redemarrage programme apres sauvegarde");
        _restartPending = true;
        _restartAtMs = millis() + 100U;
    }
'''
replace_once(web_path, old_restart, new_restart)

decor_path = ROOT / "src" / "DisplayPlanningDecor.cpp"
replace_once(
    decor_path,
    'uint32_t s_lastDisplayUpdate = UINT32_MAX;\nuint32_t s_lastDecorMs = 0;\n',
    'uint32_t s_lastDisplayUpdate = UINT32_MAX;\n',
)
old_decor = '''    const uint32_t now = millis();
    const bool displayChanged =
        s_lastDisplayUpdate != display._lastUpdate ||
        s_lastScreen != display._screen ||
        s_lastMode != display._homeMode ||
        s_lastGrid4View != display._grid4View;
    const bool periodicRefresh = now - s_lastDecorMs >= 750;

    if (!displayChanged && !periodicRefresh) return;

    s_lastDisplayUpdate = display._lastUpdate;
    s_lastScreen = display._screen;
    s_lastMode = display._homeMode;
    s_lastGrid4View = display._grid4View;
    s_lastDecorMs = now;

    if (displayChanged) {
        switch (display._homeMode) {
            case HomeMode::LIST:  redrawListWeather(display);  break;
            case HomeMode::GRID2: redrawGrid2Weather(display); break;
            case HomeMode::GRID4: break;
        }
    }
'''
new_decor = '''    const bool displayChanged =
        s_lastDisplayUpdate != display._lastUpdate ||
        s_lastScreen != display._screen ||
        s_lastMode != display._homeMode ||
        s_lastGrid4View != display._grid4View;

    if (!displayChanged) return;

    s_lastDisplayUpdate = display._lastUpdate;
    s_lastScreen = display._screen;
    s_lastMode = display._homeMode;
    s_lastGrid4View = display._grid4View;

    switch (display._homeMode) {
        case HomeMode::LIST:  redrawListWeather(display);  break;
        case HomeMode::GRID2: redrawGrid2Weather(display); break;
        case HomeMode::GRID4: break;
    }
'''
replace_once(decor_path, old_decor, new_decor)

weather_path = ROOT / "src" / "WeatherManager.cpp"
old_due = '''    const uint32_t now = millis();
    const bool due =
        !_fetched ||
        _forceFetch ||
        (now - _lastCheck >= OWM_CHECK_INTERVAL_MS);

    if (!due) return;
'''
new_due = '''    const uint32_t now = millis();
    const bool retryDue =
        _nextFetchAt == 0U ||
        static_cast<int32_t>(now - _nextFetchAt) >= 0;
    const bool due =
        _forceFetch ||
        (!_fetched && retryDue) ||
        (_fetched && (now - _lastCheck >= OWM_CHECK_INTERVAL_MS));

    if (!due) return;
'''
replace_once(weather_path, old_due, new_due)

old_parse = '''            if (result.httpCode == HTTP_CODE_OK) {
                String payload = http.getString();

                JsonDocument doc;
                const DeserializationError err = deserializeJson(doc, payload);
'''
new_parse = '''            if (result.httpCode == HTTP_CODE_OK) {
                result.payloadSize = http.getSize();

                JsonDocument doc;
                WiFiClient& stream = http.getStream();
                const DeserializationError err = deserializeJson(doc, stream);
'''
replace_once(weather_path, old_parse, new_parse)

old_failure = '''    if (!result.success) {
        EventLog::log(
            LOG_WARN,
            "Meteo: fetch echoue code=%d erreur=%s",
            static_cast<int>(result.httpCode),
            result.error[0] ? result.error : "inconnue"
        );
        return;
    }
'''
new_failure = '''    if (!result.success) {
        _nextFetchAt = millis() + FETCH_RETRY_DELAY_MS;
        EventLog::log(
            LOG_WARN,
            "Meteo: fetch echoue code=%d taille=%ld erreur=%s retry=%lus",
            static_cast<int>(result.httpCode),
            static_cast<long>(result.payloadSize),
            result.error[0] ? result.error : "inconnue",
            static_cast<unsigned long>(FETCH_RETRY_DELAY_MS / 1000UL)
        );
        return;
    }
'''
replace_once(weather_path, old_failure, new_failure)

old_success = '''    _rainExpected = result.rainExpected;
    _fetched = true;

    EventBus::displayDirty = true;
'''
new_success = '''    _rainExpected = result.rainExpected;
    _fetched = true;
    _nextFetchAt = 0U;

    EventBus::displayDirty = true;
'''
replace_once(weather_path, old_success, new_success)

print("Run 6.22 materialise avec succes.")