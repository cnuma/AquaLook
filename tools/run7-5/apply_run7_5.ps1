$ErrorActionPreference = 'Stop'

$expectedBranch = 'work/step7-run7-5'
$currentBranch = (git branch --show-current).Trim()
if ($currentBranch -ne $expectedBranch) {
    throw "Branche incorrecte: '$currentBranch'. Attendu: '$expectedBranch'."
}

$status = git status --porcelain
if ($status) {
    throw "Working tree non propre. Sauvegarder ou restaurer les modifications avant RUN7.5.`n$status"
}

$path = 'src/main.cpp'
$content = Get-Content -Raw -Encoding UTF8 $path

function Replace-ExactlyOnce {
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

$content = Replace-ExactlyOnce $content `
    '#include "EquipmentRuntimeConfigStore.h"' `
    "#include `"EquipmentRuntimeConfigStore.h`"`r`n#include `"EquipmentOrchestrator.h`"" `
    'include EquipmentOrchestrator'

$content = Replace-ExactlyOnce $content `
    'AquaLook::Runtime::EquipmentRuntimeConfigStore equipmentConfigStore;' `
    "AquaLook::Runtime::EquipmentRuntimeConfigStore equipmentConfigStore;`r`nAquaLook::Application::EquipmentOrchestrator equipmentOrchestrator;" `
    'global EquipmentOrchestrator'

$content = Replace-ExactlyOnce $content `
    "static bool equipmentRuntimeReady = false;`r`nstatic bool shadowPumpScenarioReady = false;" `
    "static bool equipmentRuntimeReady = false;`r`nstatic bool shadowPumpScenarioReady = false;`r`nstatic bool equipmentOrchestratorShadowReady = false;" `
    'orchestrator readiness flag'

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
            const AquaLook::Application::EquipmentOrchestrator::Preview preview = state
                ? equipmentOrchestrator.previewStartZone(zone)
                : equipmentOrchestrator.previewStopZone(zone);
            EventLog::log(
                preview.ready() ? LOG_INFO : LOG_WARN,
                "Orchestrator shadow: zone=%u intent=%s status=%u plan=%u steps=%u pump=%s authority=no",
                zone + 1U,
                state ? "START" : "STOP",
                static_cast<unsigned>(preview.status),
                static_cast<unsigned>(preview.planResult),
                static_cast<unsigned>(preview.stepCount),
                preview.requiresPump ? "yes" : "no"
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

$content = Replace-ExactlyOnce $content $oldRelayBlock $newRelayBlock 'onRelayRequest shadow wiring'

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

$content = Replace-ExactlyOnce $content $oldSetup $newSetup 'setup orchestrator shadow initialization'

Set-Content -Path $path -Value $content -Encoding UTF8 -NoNewline

Write-Host 'RUN7.5 applique dans src/main.cpp.'
Write-Host 'Verification du diff:'
git diff --check
if ($LASTEXITCODE -ne 0) {
    throw 'git diff --check a detecte une erreur.'
}
git diff -- $path
Write-Host ''
Write-Host 'Le patch est applique mais non commite. Compiler et valider avant commit.'
