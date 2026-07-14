$ErrorActionPreference = 'Stop'

$expectedBranch = 'work/step7-run7-8'
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

if ($content.Contains('Orchestrator audit: zone=%u intent=%s match=%s')) {
    Write-Host 'RUN7.8 deja applique.'
    exit 0
}

$auditBlock = @'

            const EquipmentManager& auditPlanManager =
                shadowPumpScenarioReady ? shadowEquipmentMgr : equipmentMgr;
            const EquipmentManager::ZoneExecutionPlan auditShadowPlan = state
                ? auditPlanManager.buildZoneStartPlan(zone)
                : auditPlanManager.buildZoneStopPlan(zone);
            const bool auditMatch =
                orchestratorPreview.ready() == auditShadowPlan.valid() &&
                orchestratorPreview.planResult == auditShadowPlan.result &&
                orchestratorPreview.stepCount == auditShadowPlan.stepCount &&
                orchestratorPreview.requiresPump == auditShadowPlan.requiresPump;
            EventLog::log(
                auditMatch ? LOG_INFO : LOG_WARN,
                "Orchestrator audit: zone=%u intent=%s match=%s preview=%u/%u/%s shadow=%u/%u/%s authority=no",
                zone + 1U,
                state ? "START" : "STOP",
                auditMatch ? "yes" : "no",
                static_cast<unsigned>(orchestratorPreview.planResult),
                static_cast<unsigned>(orchestratorPreview.stepCount),
                orchestratorPreview.requiresPump ? "pump" : "no_pump",
                static_cast<unsigned>(auditShadowPlan.result),
                static_cast<unsigned>(auditShadowPlan.stepCount),
                auditShadowPlan.requiresPump ? "pump" : "no_pump"
            );
'@

# Repere structurel stable situe a la fin du log RUN7.7, dans la portee de orchestratorPreview.
$pattern = '(?s)(\s*static_cast<unsigned long>\(orchestratorStats\.plannedSteps\)\s*\n\s*\);)(\s*\n\s*\})'
$matches = [regex]::Matches($content, $pattern)
if ($matches.Count -ne 1) {
    throw "Repere structurel RUN7.8 invalide: $($matches.Count) occurrence(s)."
}

$content = [regex]::Replace(
    $content,
    $pattern,
    { param($m) $m.Groups[1].Value + $auditBlock + $m.Groups[2].Value },
    1
)

$normalizedLines = $content -split "`n" | ForEach-Object { $_.TrimEnd() }
$content = ($normalizedLines -join "`r`n").TrimEnd() + "`r`n"
[System.IO.File]::WriteAllText((Resolve-Path $path), $content, [System.Text.UTF8Encoding]::new($false))

$checkOutput = git diff --check 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host $checkOutput
    throw 'git diff --check a detecte une erreur.'
}

Write-Host 'RUN7.8 applique: audit passif ajoute dans la portee du preview RUN7.7.'