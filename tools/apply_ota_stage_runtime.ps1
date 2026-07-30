param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$utf8 = [System.Text.UTF8Encoding]::new($false)

function Read-Text([string]$Path) {
    [System.IO.File]::ReadAllText($Path, $utf8)
}

function Write-Text([string]$Path, [string]$Content) {
    [System.IO.File]::WriteAllText($Path, $Content, $utf8)
}

function Replace-Count(
    [string]$Content,
    [string]$Pattern,
    [string]$Replacement,
    [int]$ExpectedCount,
    [string]$Label
) {
    $matches = [regex]::Matches($Content, $Pattern)
    if ($matches.Count -ne $ExpectedCount) {
        throw "$Label : $($matches.Count) occurrence(s), $ExpectedCount attendue(s). Aucun fichier modifie."
    }
    [regex]::Replace($Content, $Pattern, $Replacement)
}

$paths = @{
    RequestH   = Join-Path $RepositoryRoot 'src\MaintenanceRequest.h'
    RequestCpp = Join-Path $RepositoryRoot 'src\MaintenanceRequest.cpp'
    ResultCpp  = Join-Path $RepositoryRoot 'src\MaintenanceResult.cpp'
    BootCpp    = Join-Path $RepositoryRoot 'src\MaintenanceBoot.cpp'
    WebH       = Join-Path $RepositoryRoot 'src\WebManager.h'
}

foreach ($path in $paths.Values) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Fichier requis absent : $path"
    }
}

$original = @{}
foreach ($key in $paths.Keys) { $original[$key] = Read-Text $paths[$key] }
$updated = @{}
foreach ($key in $paths.Keys) { $updated[$key] = $original[$key] }

# MaintenanceRequest.h : conserver toutes les valeurs existantes et ajouter la nouvelle commande en fin.
$updated.RequestH = Replace-Count $updated.RequestH `
    'DOWNLOAD_UPDATE_TEST\s*=\s*6U\s*\r?\n}' `
    "DOWNLOAD_UPDATE_TEST = 6U,`r`n    STAGE_UPDATE_TEST = 7U`r`n}" `
    1 'MaintenanceRequest.h enum'

# MaintenanceRequest.cpp : plage valide et nom persiste.
$updated.RequestCpp = Replace-Count $updated.RequestCpp `
    'MaintenanceRequest::DOWNLOAD_UPDATE_TEST\);' `
    'MaintenanceRequest::STAGE_UPDATE_TEST);' `
    1 'MaintenanceRequest.cpp isValid'
$updated.RequestCpp = Replace-Count $updated.RequestCpp `
    '(case MaintenanceRequest::DOWNLOAD_UPDATE_TEST:\s*return "download_update_test";\r?\n)' `
    '$1        case MaintenanceRequest::STAGE_UPDATE_TEST: return "stage_update_test";`r`n' `
    1 'MaintenanceRequest.cpp name'

# MaintenanceResult.cpp : un staging reussi doit conserver les metadonnees manifeste et ses compteurs.
$updated.ResultCpp = Replace-Count $updated.ResultCpp `
    '(const bool isDownloadTest = strcmp\(result\.command, "download_update_test"\) == 0;\r?\n)' `
    '$1    const bool isStageTest = strcmp(result.command, "stage_update_test") == 0;`r`n' `
    1 'MaintenanceResult.cpp isStageTest'
$updated.ResultCpp = Replace-Count $updated.ResultCpp `
    'if \(!isDownloadTest\) \{' `
    'if (!isDownloadTest && !isStageTest) {' `
    1 'MaintenanceResult.cpp preservation'

# MaintenanceBoot.cpp : inclure le moteur, autoriser la commande et dispatcher explicitement.
$updated.BootCpp = Replace-Count $updated.BootCpp `
    '(#include "OtaDownloadTest\.h"\r?\n)' `
    '$1#include "OtaStageUpdate.h"`r`n' `
    1 'MaintenanceBoot.cpp include'
$updated.BootCpp = Replace-Count $updated.BootCpp `
    '(request != MaintenanceRequest::CHECK_VERSION &&\r?\n\s*request != MaintenanceRequest::DOWNLOAD_UPDATE_TEST)' `
    '$1 &&`r`n        request != MaintenanceRequest::STAGE_UPDATE_TEST' `
    1 'MaintenanceBoot.cpp allowed commands'
$updated.BootCpp = Replace-Count $updated.BootCpp `
    '} else \{\r?\n\s*const MaintenanceResult validatedManifest = MaintenanceResultStore::load\(\);\r?\n\s*const MaintenanceResult result = OtaDownloadTest::run\(validatedManifest\);' `
    '} else if (request == MaintenanceRequest::DOWNLOAD_UPDATE_TEST) {`r`n        const MaintenanceResult validatedManifest = MaintenanceResultStore::load();`r`n        const MaintenanceResult result = OtaDownloadTest::run(validatedManifest);' `
    1 'MaintenanceBoot.cpp download branch'
$stageBranch = @'
    } else {
        const MaintenanceResult validatedManifest = MaintenanceResultStore::load();
        const MaintenanceResult result = OtaStageUpdate::run(validatedManifest);
        success = result.success;
        if (!MaintenanceResultStore::save(result)) {
            EventLog::log(LOG_ERROR,
                          "Maintenance: echec sauvegarde resultat STAGE_UPDATE_TEST");
        }
        EventLog::log(result.success ? LOG_INFO : LOG_ERROR,
                      "Maintenance: STAGE_UPDATE_TEST success=%s bytes=%lu expected=%lu detail=%s otaActivate=no",
                      result.success ? "yes" : "no",
                      static_cast<unsigned long>(result.downloadedSize),
                      static_cast<unsigned long>(result.firmwareSize), result.detail);
'@
$updated.BootCpp = Replace-Count $updated.BootCpp `
    '(\r?\n\s*EventLog::log\(success \? LOG_INFO : LOG_ERROR,\r?\n\s*"Maintenance: resultat command=%s success=%s otaWrite=no")' `
    "`r`n$stageBranch`$1" `
    1 'MaintenanceBoot.cpp stage branch insertion'

# WebManager.h : ajouter un bouton, son etat JS, puis la route POST protegee.
$updated.WebH = Replace-Count $updated.WebH `
    '(<button id="download" onclick="startMaintenance\(''download''\)">Tester le telechargement et le SHA-256</button>)' `
    '$1<button id="stage" onclick="startMaintenance(''stage'')">Ecrire et verifier la partition inactive</button>' `
    1 'WebManager.h stage button'
$updated.WebH = Replace-Count $updated.WebH `
    "const uri=kind==='check'\?'/api/maintenance/check-version':kind==='download'\?'/api/maintenance/download-update-test':'/api/maintenance/probe-github';" `
    "const uri=kind==='check'?'/api/maintenance/check-version':kind==='download'?'/api/maintenance/download-update-test':kind==='stage'?'/api/maintenance/stage-update-test':'/api/maintenance/probe-github';" `
    1 'WebManager.h stage uri'
$updated.WebH = Replace-Count $updated.WebH `
    "const expected=kind==='check'\?'check_version':kind==='download'\?'download_update_test':'probe_github';" `
    "const expected=kind==='check'?'check_version':kind==='download'?'download_update_test':kind==='stage'?'stage_update_test':'probe_github';" `
    1 'WebManager.h stage expected'
$updated.WebH = Replace-Count $updated.WebH `
    "const question=kind==='check'\?'AquaLook va redemarrer pour verifier la version disponible\. Continuer \?':kind==='download'\?'Le firmware complet sera telecharge et verifie sans etre installe\. Continuer \?':'AquaLook va redemarrer pour tester GitHub\. Continuer \?';" `
    "const question=kind==='check'?'AquaLook va redemarrer pour verifier la version disponible. Continuer ?':kind==='download'?'Le firmware complet sera telecharge et verifie sans etre installe. Continuer ?':kind==='stage'?'Le firmware sera ecrit dans la partition inactive sans etre active. Continuer ?':'AquaLook va redemarrer pour tester GitHub. Continuer ?';" `
    1 'WebManager.h stage confirmation'
$updated.WebH = $updated.WebH.Replace("document.getElementById('download').disabled=false;document.getElementById('probe').disabled=false", "document.getElementById('download').disabled=false;document.getElementById('stage').disabled=false;document.getElementById('probe').disabled=false")
$updated.WebH = $updated.WebH.Replace("download.disabled=true;probe.disabled=true", "download.disabled=true;document.getElementById('stage').disabled=true;probe.disabled=true")
$updated.WebH = $updated.WebH.Replace("j.command==='download_update_test'", "(j.command==='download_update_test'||j.command==='stage_update_test')")

$route = @'

        _server.on("/api/maintenance/stage-update-test", HTTP_POST,
            [this](AsyncWebServerRequest* req) {
                if (!_config || !_relais) {
                    req->send(503, "application/json", "{\"ok\":false,\"error\":\"runtime-not-ready\"}");
                    return;
                }
                for (uint8_t zone = 0U; zone < _config->nbZones(); ++zone) {
                    if (_relais->getState(zone)) {
                        EventLog::log(LOG_WARN,
                                      "Maintenance Web: staging refuse, zone %u active",
                                      static_cast<unsigned>(zone + 1U));
                        req->send(409, "application/json", "{\"ok\":false,\"error\":\"watering-active\"}");
                        return;
                    }
                }
                const MaintenanceResult previous = MaintenanceResultStore::load();
                if (!previous.valid || !previous.success || !previous.updateAvailable ||
                    previous.firmwareUrl[0] == '\0' || previous.firmwareSize == 0U ||
                    previous.sha256[0] == '\0') {
                    req->send(409, "application/json", "{\"ok\":false,\"error\":\"check-version-required\"}");
                    return;
                }
                if (!MaintenanceRequestStore::save(MaintenanceRequest::STAGE_UPDATE_TEST)) {
                    req->send(500, "application/json", "{\"ok\":false,\"error\":\"nvs-write-failed\"}");
                    return;
                }
                EventLog::log(LOG_WARN,
                              "Maintenance Web: staging partition inactive demande, redemarrage programme");
                _restartPending = true;
                _restartAtMs = millis() + 750U;
                req->send(202, "application/json",
                          "{\"ok\":true,\"restart\":true,\"command\":\"stage_update_test\"}");
            }
        );
'@
$updated.WebH = Replace-Count $updated.WebH `
    '(\r?\n\s*_server\.on\("/logs", HTTP_GET,)' `
    "$route`$1" `
    1 'WebManager.h stage route'

# Verifications finales avant ecriture.
if ($updated.BootCpp -match 'esp_ota_set_boot_partition') {
    throw 'Activation OTA detectee dans MaintenanceBoot.cpp. Aucun fichier modifie.'
}
if ($updated.WebH -notmatch 'stage_update_test') {
    throw 'Route ou UI stage_update_test absente. Aucun fichier modifie.'
}

foreach ($key in $paths.Keys) { Write-Text $paths[$key] $updated[$key] }

Write-Host 'Raccordement OTA-3.1 applique :' -ForegroundColor Green
foreach ($key in @('RequestH','RequestCpp','ResultCpp','BootCpp','WebH')) {
    Write-Host "  $($paths[$key])"
}
Write-Host 'Aucune activation de partition ajoutee.' -ForegroundColor Green
