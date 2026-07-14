$ErrorActionPreference = 'Stop'

$expectedBranch = 'work/step7-run7-7'
$currentBranch = (git branch --show-current).Trim()
if ($currentBranch -ne $expectedBranch) {
    throw "Branche attendue: $expectedBranch ; branche courante: $currentBranch"
}

$status = git status --short
$unexpected = $status | Where-Object { $_ -notmatch '^ M \.vscode/launch\.json$' }
if ($unexpected) {
    throw "Working tree contient des modifications inattendues:`n$($unexpected -join "`n")"
}

$path = 'src/main.cpp'
$content = (Get-Content -Raw -Encoding UTF8 $path) -replace "`r`n", "`n"

function Insert-AfterOnce([string]$text, [string]$marker, [string]$insert, [string]$label) {
    $index = $text.IndexOf($marker, [System.StringComparison]::Ordinal)
    if ($index -lt 0) { throw "Repere introuvable: $label" }
    $second = $text.IndexOf($marker, $index + $marker.Length, [System.StringComparison]::Ordinal)
    if ($second -ge 0) { throw "Repere non unique: $label" }
    return $text.Substring(0, $index + $marker.Length) + $insert + $text.Substring($index + $marker.Length)
}

if (-not $content.Contains('#include "EquipmentOrchestrator.h"')) {
    $content = Insert-AfterOnce $content '#include "EquipmentRuntimeConfigStore.h"' "`n#include \"EquipmentOrchestrator.h\"" 'include EquipmentOrchestrator'
}

if (-not $content.Contains('AquaLook::Application::EquipmentOrchestrator equipmentOrchestrator;')) {
    $content = Insert-AfterOnce $content 'AquaLook::Runtime::EquipmentRuntimeConfigStore equipmentConfigStore;' "`nAquaLook::Application::EquipmentOrchestrator equipmentOrchestrator;" 'global EquipmentOrchestrator'
}

if (-not $content.Contains('static bool equipmentOrchestratorShadowReady = false;')) {
    $content = Insert-AfterOnce $content 'static bool shadowPumpScenarioReady = false;' "`nstatic bool equipmentOrchestratorShadowReady = false;" 'orchestrator readiness flag'
}

$newRelayBlock = @'
static void onRelayRequest(uint8_t zone, bool state) {
    if (equipmentRuntimeReady) {
        const uint32_t nowMs = millis();

        if (equipmentOrchestratorShadowReady) {
            const AquaLook::Application::EquipmentOrchestrator::Preview orchestratorPreview = state
                ? equipmentOrchestrator.previewStartZone(zone)
                : equipmentOrchestrator.previewStopZone(zone);
            const AquaLook::Application::EquipmentOrchestrator::ObservationStats& orchestratorStats =
                equipmentOrchestrator.stats();
            EventLog::log(
                orchestratorPreview.ready() ? LOG_INFO : LOG_WARN,
                "Orchestrator shadow: zone=%u intent=%s status=%u plan=%u steps=%u pump=%s authority=no stats=%lu/%lu/%lu ready=%lu rejected=%lu pumpPlans=%lu plannedSteps=%lu",
                zone + 1U,
                state ? "START" : "STOP",
                static_cast<unsigned>(orchestratorPreview.status),
                static_cast<unsigned>(orchestratorPreview.planResult),
                static_cast<unsigned>(orchestratorPreview.stepCount),
                orchestratorPreview.requiresPump ? "yes" : "no",
                static_cast<unsigned long>(orchestratorStats.totalRequests),
                static_cast<unsigned long>(orchestratorStats.startRequests),
                static_cast<unsigned long>(orchestratorStats.stopRequests),
                static_cast<unsigned long>(orchestratorStats.readyPlans),
                static_cast<unsigned long>(orchestratorStats.rejectedPlans),
                static_cast<unsigned long>(orchestratorStats.plansWithPump),
                static_cast<unsigned long>(orchestratorStats.plannedSteps)
            );
        }

        const EquipmentManager& shadowPlanManager =
            shadowPumpScenarioReady ? shadowEquipmentMgr : equipmentMgr;
        const EquipmentManager::ZoneExecutionPlan shadowPlan = state
            ? shadowPlanManager.buildZoneStartPlan(zone)
            : shadowPlanManager.buildZoneStopPlan(zone);
        executionShadowRuntime.submit(zone, shadowPlan, state, nowMs);

        const EquipmentManager::ActionResult result = state
            ? equipmentMgr.startZone(zone)
            : equipmentMgr.stopZone(zone);
        if (result == EquipmentManager::ACTION_OK) {
            displayMgr.requestDynamicRefresh();
            return;
        }
        EventLog::log(
            LOG_WARN,
            "Equipment: zone %u echec=%u, fallback adaptateur",
            zone + 1U,
            static_cast<unsigned>(result)
        );
    }

    outputAdapter.setZoneValve(zone, state, millis());
    displayMgr.requestDynamicRefresh();
}
'@

$relayPattern = '(?s)static void onRelayRequest\(uint8_t zone, bool state\) \{.*?\n\}\n\nstatic uint8_t _splashStep'
$relayMatches = [regex]::Matches($content, $relayPattern)
if ($relayMatches.Count -ne 1) {
    throw "Repere structurel onRelayRequest invalide: $($relayMatches.Count) occurrence(s)"
}
$content = [regex]::Replace($content, $relayPattern, $newRelayBlock + "`nstatic uint8_t _splashStep", 1)

if (-not $content.Contains('equipmentOrchestrator.begin(orchestratorShadowManager, nbZones);')) {
    $setupMarker = '    executionShadowRuntime.begin(equipmentRuntimeReady ? nbZones : 0U);'
    $setupInsert = @'
    EquipmentManager* orchestratorShadowManager = shadowPumpScenarioReady
        ? &shadowEquipmentMgr
        : &equipmentMgr;
    equipmentOrchestrator.begin(orchestratorShadowManager, nbZones);
    equipmentOrchestratorShadowReady = equipmentOrchestrator.isInitialized();
    EventLog::log(
        equipmentOrchestratorShadowReady ? LOG_INFO : LOG_WARN,
        "Orchestrator shadow: status=%s source=%s authority=no zones=%u",
        equipmentOrchestratorShadowReady ? "ready" : "unavailable",
        shadowPumpScenarioReady ? "pump_shadow" : "runtime_model",
        static_cast<unsigned>(nbZones)
    );

'@
    $content = $content.Replace($setupMarker, $setupInsert + $setupMarker)
    if (-not $content.Contains('equipmentOrchestrator.begin(orchestratorShadowManager, nbZones);')) {
        throw 'Insertion setup orchestrateur impossible.'
    }
}

$normalizedLines = $content -split "`n" | ForEach-Object { $_.TrimEnd() }
$content = ($normalizedLines -join "`r`n").TrimEnd() + "`r`n"
[System.IO.File]::WriteAllText((Resolve-Path $path), $content, [System.Text.UTF8Encoding]::new($false))

$checkOutput = git diff --check 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host $checkOutput
    throw 'git diff --check a detecte une erreur.'
}

Write-Host 'RUN7.7 applique par reperes structurels.'
Write-Host 'Verifier git diff -- src/main.cpp avant compilation.'
