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

$old = @'
        EventLog::log(
            LOG_INFO,
            "Orchestrator shadow: zone=%u intent=%s status=%u plan=%u steps=%u pump=%s authority=no",
            zone + 1U,
            state ? "START" : "STOP",
            static_cast<unsigned>(orchestratorPreview.status),
            static_cast<unsigned>(orchestratorPreview.planResult),
            static_cast<unsigned>(orchestratorPreview.stepCount),
            orchestratorPreview.requiresPump ? "yes" : "no"
        );
'@

$new = @'
        const AquaLook::Application::EquipmentOrchestrator::ObservationStats& orchestratorStats =
            equipmentOrchestrator.stats();
        EventLog::log(
            LOG_INFO,
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
'@

if (-not $content.Contains($old)) {
    throw 'Bloc de log orchestrateur RUN7.5 attendu introuvable dans src/main.cpp.'
}

$content = $content.Replace($old, $new)
Set-Content -Path $path -Value $content -Encoding UTF8

git diff --check
Write-Host 'RUN7.7 applique. Verifier git diff -- src/main.cpp avant compilation.'
