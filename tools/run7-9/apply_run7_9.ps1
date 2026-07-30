$ErrorActionPreference = 'Stop'

$expectedBranch = 'work/step7-run7-9'
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

if ($content.Contains('source=orchestrator authority=no')) {
    Write-Host 'RUN7.9 deja applique.'
    exit 0
}

$newRelayBlock = @'
static void onRelayRequest(uint8_t zone, bool state) {
    if (equipmentRuntimeReady) {
        const uint32_t nowMs = millis();
        EquipmentManager::ZoneExecutionPlan shadowPlan;
        bool shadowPlanFromOrchestrator = false;

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
            shadowPlan = orchestratorPreview.plan;
            shadowPlanFromOrchestrator = true;
        } else {
            const EquipmentManager& shadowPlanManager =
                shadowPumpScenarioReady ? shadowEquipmentMgr : equipmentMgr;
            shadowPlan = state
                ? shadowPlanManager.buildZoneStartPlan(zone)
                : shadowPlanManager.buildZoneStopPlan(zone);
        }

        EventLog::log(
            shadowPlan.valid() ? LOG_INFO : LOG_WARN,
            "Orchestrator handoff: zone=%u intent=%s source=%s result=%u steps=%u pump=%s authority=no",
            zone + 1U,
            state ? "START" : "STOP",
            shadowPlanFromOrchestrator ? "orchestrator" : "legacy_shadow_builder",
            static_cast<unsigned>(shadowPlan.result),
            static_cast<unsigned>(shadowPlan.stepCount),
            shadowPlan.requiresPump ? "yes" : "no"
        );
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

$pattern = '(?s)static void onRelayRequest\(uint8_t zone, bool state\) \{.*?\n\}\s*\nstatic uint8_t _splashStep'
$matches = [regex]::Matches($content, $pattern)
if ($matches.Count -ne 1) {
    throw "Repere structurel onRelayRequest invalide: $($matches.Count) occurrence(s)"
}
$content = [regex]::Replace($content, $pattern, $newRelayBlock + "`nstatic uint8_t _splashStep", 1)

$normalizedLines = $content -split "`n" | ForEach-Object { $_.TrimEnd() }
$content = ($normalizedLines -join "`r`n").TrimEnd() + "`r`n"
[System.IO.File]::WriteAllText((Resolve-Path $path), $content, [System.Text.UTF8Encoding]::new($false))

$checkOutput = git diff --check 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host $checkOutput
    throw 'git diff --check a detecte une erreur.'
}

Write-Host 'RUN7.9 applique: le moteur shadow consomme le plan transporte par l orchestrateur.'
Write-Host 'Verifier git diff -- src/main.cpp avant compilation.'
