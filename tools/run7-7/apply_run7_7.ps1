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

function Replace-RegexOnce {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Replacement,
        [string]$Label
    )
    $matches = [regex]::Matches($Text, $Pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if ($matches.Count -ne 1) {
        throw "Repere structurel invalide pour $Label : occurrences=$($matches.Count)"
    }
    return [regex]::Replace(
        $Text,
        $Pattern,
        [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $Replacement },
        [System.Text.RegularExpressions.RegexOptions]::Singleline
    )
}

if (-not $content.Contains('#include "EquipmentOrchestrator.h"')) {
    $content = Replace-RegexOnce $content `
        '(?m)^#include "EquipmentRuntimeConfigStore\.h"$' `
        "#include `"EquipmentRuntimeConfigStore.h`"`n#include `"EquipmentOrchestrator.h`"" `
        'include EquipmentOrchestrator'
}

if (-not $content.Contains('AquaLook::Application::EquipmentOrchestrator equipmentOrchestrator;')) {
    $content = Replace-RegexOnce $content `
        '(?m)^AquaLook::Runtime::EquipmentRuntimeConfigStore equipmentConfigStore;$' `
        "AquaLook::Runtime::EquipmentRuntimeConfigStore equipmentConfigStore;`nAquaLook::Application::EquipmentOrchestrator equipmentOrchestrator;" `
        'global EquipmentOrchestrator'
}

if (-not $content.Contains('static bool equipmentOrchestratorShadowReady = false;')) {
    $content = Replace-RegexOnce $content `
        '(?m)^static bool shadowPumpScenarioReady = false;$' `
        "static bool shadowPumpScenarioReady = false;`nstatic bool equipmentOrchestratorShadowReady = false;" `
        'orchestrator readiness flag'
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

if (-not $content.Contains('orchestratorStats.totalRequests')) {
    $content = Replace-RegexOnce $content `
        'static void onRelayRequest\(uint8_t zone, bool state\) \{.*?\n\}\n\nstatic uint8_t _splashStep' `
        ($newRelayBlock + "`nstatic uint8_t _splashStep") `
        'onRelayRequest'
}

if (-not $content.Contains('equipmentOrchestrator.begin(orchestratorShadowManager, nbZones);')) {
    $setupInsertion = @'

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
    $content = Replace-RegexOnce $content `
        '(?m)^(\s*)executionShadowRuntime\.begin\(equipmentRuntimeReady \? nbZones : 0U\);$' `
        ($setupInsertion + "`n    executionShadowRuntime.begin(equipmentRuntimeReady ? nbZones : 0U);") `
        'setup orchestrator initialization'
}

$content = $content -replace "`n", "`r`n"
Set-Content -Path $path -Value $content -Encoding UTF8 -NoNewline

git diff --check
if ($LASTEXITCODE -ne 0) {
    throw 'git diff --check a detecte une erreur.'
}

Write-Host 'RUN7.7 applique avec succes.'
Write-Host 'Verifier git diff -- src/main.cpp avant compilation.'
