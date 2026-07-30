param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Read-Utf8File([string]$Path) {
    return [System.IO.File]::ReadAllText($Path, [System.Text.UTF8Encoding]::new($false))
}

function Write-Utf8File([string]$Path, [string]$Content) {
    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Replace-Exact(
    [string]$Content,
    [string]$Old,
    [string]$New,
    [int]$ExpectedCount,
    [string]$Label
) {
    $count = ([regex]::Matches($Content, [regex]::Escape($Old))).Count
    if ($count -ne $ExpectedCount) {
        throw "$Label : $count occurrence(s) trouvee(s), $ExpectedCount attendue(s). Aucun fichier modifie."
    }
    return $Content.Replace($Old, $New)
}

function Replace-Regex(
    [string]$Content,
    [string]$Pattern,
    [string]$Replacement,
    [int]$ExpectedCount,
    [string]$Label
) {
    $matches = [regex]::Matches($Content, $Pattern)
    if ($matches.Count -ne $ExpectedCount) {
        throw "$Label : $($matches.Count) occurrence(s) trouvee(s), $ExpectedCount attendue(s). Aucun fichier modifie."
    }
    return [regex]::Replace($Content, $Pattern, $Replacement)
}

$trustHeader = Join-Path $RepositoryRoot 'src\OtaTlsTrust.h'
$maintenanceBoot = Join-Path $RepositoryRoot 'src\MaintenanceBoot.cpp'
$downloadTest = Join-Path $RepositoryRoot 'src\OtaDownloadTest.cpp'

foreach ($path in @($trustHeader, $maintenanceBoot, $downloadTest)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Fichier requis absent : $path"
    }
}

$bootOriginal = Read-Utf8File $maintenanceBoot
$downloadOriginal = Read-Utf8File $downloadTest
$newLine = if ($bootOriginal.Contains("`r`n")) { "`r`n" } else { "`n" }

$bootUpdated = Replace-Exact `
    $bootOriginal `
    '#include "OtaBuildIdentity.h"' `
    ('#include "OtaBuildIdentity.h"' + $newLine + '#include "OtaTlsTrust.h"') `
    1 `
    'MaintenanceBoot include'

$bootUpdated = Replace-Exact `
    $bootUpdated `
    '    client.setInsecure();' `
    '    OtaTlsTrust::configure(client);' `
    2 `
    'MaintenanceBoot setInsecure'

$downloadNewLine = if ($downloadOriginal.Contains("`r`n")) { "`r`n" } else { "`n" }
$downloadUpdated = Replace-Exact `
    $downloadOriginal `
    '#include "EventLog.h"' `
    ('#include "EventLog.h"' + $downloadNewLine + '#include "OtaTlsTrust.h"') `
    1 `
    'OtaDownloadTest include'

$trustPattern = '(?m)^\s*// OTA-3\.0 ne réalise aucune écriture flash\. La validation CA/signature\r?\n\s*// reste obligatoire avant STAGE_UPDATE et INSTALL_UPDATE\.\r?\n\s*client\.setInsecure\(\);'
$trustReplacement = '    // La chaine TLS utilise les ancres DigiCert verifiees et generees par outil.' + $downloadNewLine +
                    '    // Ce controle authentifie le serveur mais ne remplace pas la validation SHA-256.' + $downloadNewLine +
                    '    OtaTlsTrust::configure(client);'
$downloadUpdated = Replace-Regex `
    $downloadUpdated `
    $trustPattern `
    $trustReplacement `
    1 `
    'OtaDownloadTest trust block'

if ($bootUpdated -match 'setInsecure\s*\(') {
    throw 'MaintenanceBoot.cpp contient encore setInsecure(). Aucun fichier modifie.'
}
if ($downloadUpdated -match 'setInsecure\s*\(') {
    throw 'OtaDownloadTest.cpp contient encore setInsecure(). Aucun fichier modifie.'
}

Write-Utf8File $maintenanceBoot $bootUpdated
Write-Utf8File $downloadTest $downloadUpdated

Write-Host 'TLS trust migration appliquee :' -ForegroundColor Green
Write-Host '  src/MaintenanceBoot.cpp'
Write-Host '  src/OtaDownloadTest.cpp'
Write-Host 'Controle restant : git diff --check puis compilations Legacy et V4.'
