$ErrorActionPreference = "Stop"

$expectedBranch = "work/run6-22-non-blocking-runtime"
$currentBranch = (git branch --show-current).Trim()

if ($currentBranch -ne $expectedBranch) {
    throw "Branche active '$currentBranch'. Branche attendue: '$expectedBranch'."
}

if ((git status --porcelain).Count -gt 0) {
    throw "Le working tree doit etre propre avant application du Run 6.22."
}

git pull --ff-only

git apply --check tools/run6-22/WebManager.cpp.patch
git apply --check tools/run6-22/DisplayPlanningDecor.cpp.patch
git apply --check tools/run6-22/main.cpp.patch

git apply tools/run6-22/WebManager.cpp.patch
git apply tools/run6-22/DisplayPlanningDecor.cpp.patch
git apply tools/run6-22/main.cpp.patch

git diff --check

git add src/WebManager.cpp src/DisplayPlanningDecor.cpp src/main.cpp
git commit -m "refactor: materialize Run 6.22 runtime integration"
git push origin $expectedBranch

Write-Host "Run 6.22 materialise, commite et pousse sur $expectedBranch."
