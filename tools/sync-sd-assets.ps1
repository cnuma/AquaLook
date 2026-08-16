param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z]:$')]
    [string]$SdDrive,

    [string]$Source = (Join-Path $PSScriptRoot '..\data'),

    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$sourcePath = (Resolve-Path $Source).Path
$driveRoot = "$SdDrive\"
$targetPath = Join-Path $driveRoot 'www'

if (-not (Test-Path $driveRoot -PathType Container)) {
    throw "Lecteur SD introuvable : $driveRoot"
}

if (-not (Test-Path $sourcePath -PathType Container)) {
    throw "Dossier source introuvable : $sourcePath"
}

if ($Clean -and (Test-Path $targetPath)) {
    Write-Host "Suppression de l'ancien contenu : $targetPath"
    Remove-Item $targetPath -Recurse -Force
}

New-Item -ItemType Directory -Path $targetPath -Force | Out-Null

Write-Host "Source : $sourcePath"
Write-Host "Cible  : $targetPath"

$sourceFiles = Get-ChildItem $sourcePath -File -Recurse
if ($sourceFiles.Count -eq 0) {
    throw "Aucun fichier a copier depuis $sourcePath"
}

foreach ($sourceFile in $sourceFiles) {
    $relativePath = $sourceFile.FullName.Substring($sourcePath.Length).TrimStart('\', '/')
    $destinationFile = Join-Path $targetPath $relativePath
    $destinationDirectory = Split-Path $destinationFile -Parent

    New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    Copy-Item $sourceFile.FullName $destinationFile -Force
}

$errors = New-Object System.Collections.Generic.List[string]

foreach ($sourceFile in $sourceFiles) {
    $relativePath = $sourceFile.FullName.Substring($sourcePath.Length).TrimStart('\', '/')
    $destinationFile = Join-Path $targetPath $relativePath

    if (-not (Test-Path $destinationFile -PathType Leaf)) {
        $errors.Add("Fichier absent : $relativePath")
        continue
    }

    $sourceHash = (Get-FileHash $sourceFile.FullName -Algorithm SHA256).Hash
    $destinationHash = (Get-FileHash $destinationFile -Algorithm SHA256).Hash

    if ($sourceHash -ne $destinationHash) {
        $errors.Add("Hash different : $relativePath")
    }
}

if ($errors.Count -gt 0) {
    Write-Host "Echec de validation :" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    exit 1
}

$totalBytes = ($sourceFiles | Measure-Object Length -Sum).Sum
Write-Host "Synchronisation SD validee : $($sourceFiles.Count) fichiers, $totalBytes octets." -ForegroundColor Green
Write-Host "Arborescence attendue par AquaLook : /www/..."

# Horodatage de synchronisation, affiche en pied de page (index.html) pour
# confirmer visuellement qu'on voit bien la derniere version deployee, sans
# avoir a deviner. Genere ici (pas dans data/) : sa valeur n'a de sens qu'au
# moment de CETTE synchronisation, pas comme contenu versionne dans le repo.
$syncedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
$gitSha = $null
try {
    $gitSha = (& git -C $PSScriptRoot rev-parse --short HEAD 2>$null | Out-String).Trim()
} catch {}

$versionInfo = [ordered]@{
    syncedAt  = $syncedAt
    gitSha    = if ($gitSha) { $gitSha } else { $null }
    fileCount = $sourceFiles.Count
}
$versionPath = Join-Path $targetPath 'assets-version.json'
($versionInfo | ConvertTo-Json -Compress) | Set-Content -Path $versionPath -Encoding utf8 -NoNewline
Write-Host "Horodatage ecrit : $versionPath ($syncedAt)"
