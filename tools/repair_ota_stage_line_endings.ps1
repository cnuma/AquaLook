param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$utf8 = [System.Text.UTF8Encoding]::new($false)
$literal = '`r`n'
$newLine = "`r`n"
$paths = @(
    'src\MaintenanceRequest.cpp',
    'src\MaintenanceResult.cpp',
    'src\MaintenanceBoot.cpp',
    'src\WebManager.h',
    'src\MaintenanceRequest.h'
)

$total = 0
foreach ($relative in $paths) {
    $path = Join-Path $RepositoryRoot $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Fichier requis absent : $path"
    }

    $content = [System.IO.File]::ReadAllText($path, $utf8)
    $count = ([regex]::Matches($content, [regex]::Escape($literal))).Count
    if ($count -gt 0) {
        $content = $content.Replace($literal, $newLine)
        [System.IO.File]::WriteAllText($path, $content, $utf8)
        Write-Host "REPARÉ $relative : $count occurrence(s)" -ForegroundColor Green
        $total += $count
    } else {
        Write-Host "OK $relative : aucune occurrence" -ForegroundColor DarkGray
    }
}

if ($total -eq 0) {
    throw 'Aucune sequence litterale `r`n trouvee. Aucun fichier modifie.'
}

$remaining = Get-ChildItem (Join-Path $RepositoryRoot 'src') -Recurse -File |
    Select-String -SimpleMatch $literal
if ($remaining) {
    $remaining | ForEach-Object {
        Write-Host "$($_.Path):$($_.LineNumber): $($_.Line.Trim())" -ForegroundColor Red
    }
    throw 'Des sequences litterales `r`n restent dans src.'
}

Write-Host "Correction terminee : $total occurrence(s) remplacee(s)." -ForegroundColor Cyan
