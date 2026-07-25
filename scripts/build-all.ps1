[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$KeepDist
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$DistDir = Join-Path $ProjectRoot "dist"
$Environments = @(
    "ProgrammeArrosage",
    "ProgrammeArrosage_v4"
)

function Resolve-PlatformIOCommand {
    $pio = Get-Command pio -ErrorAction SilentlyContinue
    if ($pio) { return $pio.Source }

    $platformio = Get-Command platformio -ErrorAction SilentlyContinue
    if ($platformio) { return $platformio.Source }

    $userPio = Join-Path $HOME ".platformio\penv\Scripts\platformio.exe"
    if (Test-Path $userPio) { return $userPio }

    throw "PlatformIO est introuvable. Installe l'extension PlatformIO ou ouvre un terminal PlatformIO dans VS Code."
}

function Invoke-PlatformIO {
    param([Parameter(Mandatory)][string[]]$Arguments)

    & $script:PlatformIOCommand @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "PlatformIO a échoué avec le code $LASTEXITCODE : $($Arguments -join ' ')"
    }
}

Set-Location $ProjectRoot
$script:PlatformIOCommand = Resolve-PlatformIOCommand

Write-Host "=== AquaLook : build-all ==="
Write-Host "Projet      : $ProjectRoot"
Write-Host "PlatformIO  : $script:PlatformIOCommand"
Write-Host "Nettoyage   : $Clean"
Write-Host ""

if ((Test-Path $DistDir) -and -not $KeepDist) {
    Remove-Item $DistDir -Recurse -Force
}
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null

$gitCommit = "unknown"
$gitShortCommit = "unknown"
$gitBranch = "unknown"
$gitDirty = $false

if (Get-Command git -ErrorAction SilentlyContinue) {
    $gitCommit = (git rev-parse HEAD 2>$null)
    $gitShortCommit = (git rev-parse --short HEAD 2>$null)
    $gitBranch = (git branch --show-current 2>$null)
    $gitDirty = [bool](git status --porcelain 2>$null)
}

$results = @()
$totalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

foreach ($environment in $Environments) {
    Write-Host "--- Compilation : $environment ---"

    if ($Clean) {
        Invoke-PlatformIO -Arguments @("run", "-e", $environment, "-t", "clean")
    }

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Invoke-PlatformIO -Arguments @("run", "-e", $environment)
    $stopwatch.Stop()

    $sourceFirmware = Join-Path $ProjectRoot ".pio\build\$environment\firmware.bin"
    if (-not (Test-Path $sourceFirmware)) {
        throw "Le firmware attendu est absent : $sourceFirmware"
    }

    $artifactName = "AquaLook-$environment-$gitShortCommit.bin"
    $artifactPath = Join-Path $DistDir $artifactName
    Copy-Item $sourceFirmware $artifactPath -Force

    $fileInfo = Get-Item $artifactPath
    $hash = Get-FileHash $artifactPath -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant())  $artifactName" |
        Set-Content "$artifactPath.sha256" -Encoding ascii

    $results += [pscustomobject][ordered]@{
        environment = $environment
        durationSeconds = [math]::Round($stopwatch.Elapsed.TotalSeconds, 2)
        artifact = $artifactName
        sizeBytes = $fileInfo.Length
        sha256 = $hash.Hash.ToLowerInvariant()
    }

    Write-Host ("SUCCESS     : {0}" -f $environment)
    Write-Host ("Durée       : {0:N2} s" -f $stopwatch.Elapsed.TotalSeconds)
    Write-Host ("Taille      : {0:N0} octets" -f $fileInfo.Length)
    Write-Host ("Artefact    : {0}" -f $artifactName)
    Write-Host ""
}

$totalStopwatch.Stop()

$summary = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    project = "AquaLook"
    branch = $gitBranch
    commit = $gitCommit
    dirtyWorkingTree = $gitDirty
    cleanBuild = [bool]$Clean
    totalDurationSeconds = [math]::Round($totalStopwatch.Elapsed.TotalSeconds, 2)
    builds = $results
}

$summaryPath = Join-Path $DistDir "build-summary.json"
$summary | ConvertTo-Json -Depth 5 | Set-Content $summaryPath -Encoding utf8

Write-Host "=== Résultat global ==="
$results | Format-Table environment, durationSeconds, sizeBytes, artifact -AutoSize
Write-Host ("Durée totale : {0:N2} s" -f $totalStopwatch.Elapsed.TotalSeconds)
Write-Host "Résumé       : $summaryPath"
Write-Host "Artefacts    : $DistDir"

if ($gitDirty) {
    Write-Warning "Le dépôt contient des modifications non commitées : les binaires sont destinés aux tests, pas à une publication officielle."
}
