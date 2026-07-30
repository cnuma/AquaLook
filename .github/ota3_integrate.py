from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one anchor, found {count}")
    return text.replace(old, new, 1)


boot_path = Path("src/MaintenanceBoot.cpp")
boot = boot_path.read_text(encoding="utf-8")
boot = replace_once(
    boot,
    '#include "MaintenanceResult.h"\n#include "OtaBuildIdentity.h"',
    '#include "MaintenanceResult.h"\n#include "OtaDownloadTest.h"\n#include "OtaBuildIdentity.h"',
    "MaintenanceBoot include",
)
boot = replace_once(
    boot,
    'if (request != MaintenanceRequest::PROBE_GITHUB &&\n        request != MaintenanceRequest::CHECK_VERSION) {',
    'if (request != MaintenanceRequest::PROBE_GITHUB &&\n        request != MaintenanceRequest::CHECK_VERSION &&\n        request != MaintenanceRequest::DOWNLOAD_UPDATE_TEST) {',
    "MaintenanceBoot accepted commands",
)
boot = replace_once(
    boot,
    '    } else {\n        const String manifestUrl = String("https://") + OtaBuildIdentity::MANIFEST_HOST +',
    '    } else if (request == MaintenanceRequest::CHECK_VERSION) {\n        const String manifestUrl = String("https://") + OtaBuildIdentity::MANIFEST_HOST +',
    "MaintenanceBoot check branch",
)
old_tail = '''        EventLog::log(result.success ? LOG_INFO : LOG_ERROR,
                      "Maintenance: CHECK_VERSION success=%s installed=%s available=%s update=%s detail=%s otaWrite=no",
                      result.success ? "yes" : "no", result.installedVersion,
                      result.availableVersion[0] ? result.availableVersion : "n/a",
                      result.updateAvailable ? "yes" : "no", result.detail);
    }
'''
new_tail = '''        EventLog::log(result.success ? LOG_INFO : LOG_ERROR,
                      "Maintenance: CHECK_VERSION success=%s installed=%s available=%s update=%s detail=%s otaWrite=no",
                      result.success ? "yes" : "no", result.installedVersion,
                      result.availableVersion[0] ? result.availableVersion : "n/a",
                      result.updateAvailable ? "yes" : "no", result.detail);
    } else {
        const MaintenanceResult validatedManifest = MaintenanceResultStore::load();
        const MaintenanceResult result = OtaDownloadTest::run(validatedManifest);
        success = result.success;
        if (!MaintenanceResultStore::save(result)) {
            EventLog::log(LOG_ERROR,
                          "Maintenance: echec sauvegarde resultat DOWNLOAD_UPDATE_TEST");
        }
        EventLog::log(result.success ? LOG_INFO : LOG_ERROR,
                      "Maintenance: DOWNLOAD_UPDATE_TEST success=%s bytes=%lu expected=%lu detail=%s otaWrite=no",
                      result.success ? "yes" : "no",
                      static_cast<unsigned long>(result.downloadedSize),
                      static_cast<unsigned long>(result.firmwareSize), result.detail);
    }
'''
boot = replace_once(boot, old_tail, new_tail, "MaintenanceBoot download branch")
boot_path.write_text(boot, encoding="utf-8")

web_path = Path("src/WebManager.h")
web = web_path.read_text(encoding="utf-8")
web = replace_once(
    web,
    '                    doc["firmwareSize"] = result.firmwareSize;\n',
    '                    doc["firmwareSize"] = result.firmwareSize;\n                    doc["downloadedSize"] = result.downloadedSize;\n                    doc["downloadDurationMs"] = result.downloadDurationMs;\n',
    "Web JSON sizes",
)
web = replace_once(
    web,
    '                    doc["sha256"] = result.sha256;\n',
    '                    doc["sha256"] = result.sha256;\n                    doc["calculatedSha256"] = result.calculatedSha256;\n',
    "Web JSON SHA",
)
web = replace_once(
    web,
    '<button id="check" onclick="startMaintenance(\'check\')">Verifier la version disponible</button><button id="probe" class="secondary"',
    '<button id="check" onclick="startMaintenance(\'check\')">Verifier la version disponible</button><button id="download" onclick="startMaintenance(\'download\')">Tester le telechargement et le SHA-256</button><button id="probe" class="secondary"',
    "Web download button",
)
web = replace_once(
    web,
    "if(j.command==='check_version'){rows.push(",
    "if(j.command==='check_version'||j.command==='download_update_test'){rows.push(",
    "Web result condition",
)
web = replace_once(
    web,
    "if(j.detail)rows.push(['Detail',j.detail]);",
    "if(j.command==='download_update_test'){rows.push(['Octets telecharges',(j.downloadedSize||0)+' octets'],['Duree telechargement',(j.downloadDurationMs||0)+' ms'],['SHA-256 calcule',j.calculatedSha256||'-']);}if(j.detail)rows.push(['Detail',j.detail]);",
    "Web result details",
)
old_js = "const check=document.getElementById('check'),probe=document.getElementById('probe'),out=document.getElementById('action');const isCheck=kind==='check';const uri=isCheck?'/api/maintenance/check-version':'/api/maintenance/probe-github';const question=isCheck?'AquaLook va redemarrer pour verifier la version disponible. Continuer ?':'AquaLook va redemarrer pour tester GitHub. Continuer ?';if(!confirm(question))return;check.disabled=true;probe.disabled=true;"
new_js = "const check=document.getElementById('check'),download=document.getElementById('download'),probe=document.getElementById('probe'),out=document.getElementById('action');const uri=kind==='check'?'/api/maintenance/check-version':kind==='download'?'/api/maintenance/download-update-test':'/api/maintenance/probe-github';const question=kind==='check'?'AquaLook va redemarrer pour verifier la version disponible. Continuer ?':kind==='download'?'Le firmware complet sera telecharge et verifie sans etre installe. Continuer ?':'AquaLook va redemarrer pour tester GitHub. Continuer ?';if(!confirm(question))return;check.disabled=true;download.disabled=true;probe.disabled=true;"
web = replace_once(web, old_js, new_js, "Web action selector")
web = web.replace(
    'check.disabled=false;probe.disabled=false;',
    'check.disabled=false;download.disabled=false;probe.disabled=false;',
)
route_marker = '''        _server.on("/logs", HTTP_GET,
'''
route = '''        _server.on("/api/maintenance/download-update-test", HTTP_POST,
            [this](AsyncWebServerRequest* req) {
                if (!_config || !_relais) {
                    req->send(503, "application/json", "{\\"ok\\":false,\\"error\\":\\"runtime-not-ready\\"}");
                    return;
                }
                for (uint8_t zone = 0U; zone < _config->nbZones(); ++zone) {
                    if (_relais->getState(zone)) {
                        EventLog::log(LOG_WARN,
                                      "Maintenance Web: test telechargement refuse, zone %u active",
                                      static_cast<unsigned>(zone + 1U));
                        req->send(409, "application/json", "{\\"ok\\":false,\\"error\\":\\"watering-active\\"}");
                        return;
                    }
                }
                const MaintenanceResult previous = MaintenanceResultStore::load();
                if (!previous.valid || !previous.success || !previous.updateAvailable ||
                    previous.firmwareUrl[0] == '\\0' || previous.firmwareSize == 0U ||
                    previous.sha256[0] == '\\0') {
                    req->send(409, "application/json", "{\\"ok\\":false,\\"error\\":\\"check-version-required\\"}");
                    return;
                }
                if (!MaintenanceRequestStore::save(MaintenanceRequest::DOWNLOAD_UPDATE_TEST)) {
                    req->send(500, "application/json", "{\\"ok\\":false,\\"error\\":\\"nvs-write-failed\\"}");
                    return;
                }
                EventLog::log(LOG_WARN,
                              "Maintenance Web: test telechargement demande, redemarrage programme");
                _restartPending = true;
                _restartAtMs = millis() + 750U;
                req->send(202, "application/json", "{\\"ok\\":true,\\"restart\\":true,\\"command\\":\\"download_update_test\\"}");
            }
        );

'''
web = replace_once(web, route_marker, route + route_marker, "Web download route")
web_path.write_text(web, encoding="utf-8")

assert '#include "OtaDownloadTest.h"' in boot
assert 'MaintenanceRequest::DOWNLOAD_UPDATE_TEST' in boot
assert 'id="download"' in web
assert '/api/maintenance/download-update-test' in web
