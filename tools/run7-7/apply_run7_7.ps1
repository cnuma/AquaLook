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
$content = Get-Content -Raw -Encoding UTF8 $path
$content = $content -replace "`r`n", "`n"

function Replace-Once {
    param(
        [string]$Text,
        [string]$Old,
        [string]$New,
        [string]$Label
    )

    $first = $Text.IndexOf($Old, [System.StringComparison]::Ordinal)
    if ($first -lt 0) {
        throw "Point d'insertion introuvable: $Label"
    }
    $second = $Text.IndexOf($Old, $first + $Old.Length, [System.StringComparison]::Ordinal)
    if ($second -ge 0) {
        throw "Point d'insertion non unique: $Label"
    }
    return $Text.Substring(0, $first) + $New + $Text.Substring($first + $Old.Length)
}

if (-not $content.Contains('#include "EquipmentOrchestrator.h"')) {
    $content = Replace-Once $content `
        '#include "EquipmentRuntimeConfigStore.h"' `
        "#include `"EquipmentRuntimeConfigStore.h`"`n#include `"EquipmentOrchestrator.h`"" `
        'include EquipmentOrchestrator'
}

if (-not $content.Contains('AquaLook::Application::EquipmentOrchestrator equipmentOrchestrator;')) {
    $content = Replace-Once $content `
        'AquaLook::Runtime::EquipmentRuntimeConfigStore equipmentConfigStore;' `
        "AquaLook::Runtime::EquipmentRuntimeConfigStore equipmentConfigStore;`nAquaLook::Application::EquipmentOrchestrator equipmentOrchestrator;" `
        'global EquipmentOrchestrator'
}

if (-not $content.Contains('static bool equipmentOrchestratorShadowReady = false;')) {
    $content = Replace-Once $content `
        "static bool equipmentRuntimeReady = false;`nstatic bool shadowPumpScenarioReady = false;" `
        "static bool equipmentRuntimeReady = false;`nstatic bool shadowPumpScenarioReady = false;`nstatic bool equipmentOrchestratorShadowReady = false;" `
        'orchestrator readiness flag'
}

$oldRelayBlock = @'
static void onRelayRequest(uint8_t zone, bool state) {
    if (equipmentRuntimeReady) {
        const uint32_t nowMs = millis();
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

if ($content.Contains($oldRelayBlock)) {
    $content = Replace-Once $content $oldRelayBlock $newRelayBlock 'onRelayRequest RUN7.7 shadow wiring'
} elseif (-not $content.Contains('orchestratorStats.totalRequests')) {
    throw 'Bloc onRelayRequest compatible introuvable dans src/main.cpp.'
}

$oldSetup = @'
    EventLog::log(
        shadowPumpScenarioReady ? LOG_INFO : (pumpConfigured ? LOG_WARN : LOG_INFO),
        shadowPumpScenarioReady
            ? "Shadow pump: configuration NVS active mode_effectif=shadow passive=yes"
            : (pumpConfigured
                ? "Shadow pump: configuration demandee mais scenario indisponible"
                : "Shadow pump: desactive par configuration NVS")
    );

    executionShadowRuntime.begin(equipmentRuntimeReady ? nbZones : 0U);
'@

$newSetup = @'
    EventLog::log(
        shadowPumpScenarioReady ? LOG_INFO : (pumpConfigured ? LOG_WARN : LOG_INFO),
        shadowPumpScenarioReady
            ? "Shadow pump: configuration NVS active mode_effectif=shadow passive=yes"
            : (pumpConfigured
                ? "Shadow pump: configuration demandee mais scenario indisponible"
                : "Shadow pump: desactive par configuration NVS")
    );

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

    executionShadowRuntime.begin(equipmentRuntimeReady ? nbZones : 0U);
'@

if ($content.Contains($oldSetup)) {
    $content = Replace-Once $content $oldSetup $newSetup 'setup orchestrator shadow initialization'
} elseif (-not $content.Contains('equipmentOrchestrator.begin(orchestratorShadowManager, nbZones);')) {
    throw 'Bloc setup compatible introuvable dans src/main.cpp.'
}

$normalizedLines = $content -split "`n" | ForEach-Object { $_.TrimEnd() }
$content = ($normalizedLines -join "`r`n").TrimEnd() + "`r`n"
[System.IO.File]::WriteAllText(
    (Resolve-Path $path),
    $content,
    [System.Text.UTF8Encoding]::new($false)
)

$checkOutput = git diff --check 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host $checkOutput
    throw 'git diff --check a detecte une erreur.'
}

Write-Host 'RUN7.7 applique et espaces de fin de ligne normalises.'
Write-Host 'Verifier git diff -- src/main.cpp avant compilation.'
