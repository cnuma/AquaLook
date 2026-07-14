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

$marker = @'
            );
        }

        const EquipmentManager& shadowPlanManager =
'@

$replacement = @'
            );

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
        }

        const EquipmentManager& shadowPlanManager =
'@

$first = $content.IndexOf($marker, [System.StringComparison]::Ordinal)
if ($first -lt 0) {
    throw 'Point insertion RUN7.8 introuvable dans src/main.cpp.'
}
$second = $content.IndexOf($marker, $first + $marker.Length, [System.StringComparison]::Ordinal)
if ($second -ge 0) {
    throw 'Point insertion RUN7.8 non unique dans src/main.cpp.'
}

$content = $content.Substring(0, $first) + $replacement + $content.Substring($first + $marker.Length)
$normalizedLines = $content -split "`n" | ForEach-Object { $_.TrimEnd() }
$content = ($normalizedLines -join "`r`n").TrimEnd() + "`r`n"
[System.IO.File]::WriteAllText((Resolve-Path $path), $content, [System.Text.UTF8Encoding]::new($false))

$checkOutput = git diff --check 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host $checkOutput
    throw 'git diff --check a detecte une erreur.'
}

Write-Host 'RUN7.8 applique: audit passif ajoute dans onRelayRequest().'