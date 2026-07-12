$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$expectedBranch = "work/run6-22-non-blocking-runtime"
$currentBranch = (git branch --show-current).Trim()

if ($LASTEXITCODE -ne 0) {
    throw "Impossible de determiner la branche Git active."
}

if ($currentBranch -ne $expectedBranch) {
    throw "Branche active '$currentBranch'. Branche attendue: '$expectedBranch'."
}

$initialChanges = @(git status --porcelain)
if ($LASTEXITCODE -ne 0) {
    throw "git status a echoue."
}

if ($initialChanges.Count -gt 0) {
    throw "Le working tree doit etre propre avant application du Run 6.22."
}

git pull --ff-only
if ($LASTEXITCODE -ne 0) {
    throw "git pull --ff-only a echoue."
}

python .\tools\run6-22\materialize.py
if ($LASTEXITCODE -ne 0) {
    throw "La materialisation du Run 6.22 a echoue."
}

git diff --check
if ($LASTEXITCODE -ne 0) {
    throw "git diff --check a detecte une erreur."
}

$changes = @(git status --porcelain)
if ($LASTEXITCODE -ne 0) {
    throw "git status a echoue."
}

if ($changes.Count -eq 0) {
    Write-Host "Run 6.22 deja materialise. Aucun commit necessaire."
    exit 0
}

git add `
    src\WebManager.cpp `
    src\DisplayPlanningDecor.cpp `
    src\main.cpp `
    src\WeatherManager.cpp
if ($LASTEXITCODE -ne 0) {
    throw "git add a echoue."
}

git commit -m "fix: stabilize Run 6.22 runtime and weather retries"
if ($LASTEXITCODE -ne 0) {
    throw "git commit a echoue."
}

git push origin $expectedBranch
if ($LASTEXITCODE -ne 0) {
    throw "git push a echoue."
}

Write-Host "Run 6.22 materialise, commite et pousse sur $expectedBranch."