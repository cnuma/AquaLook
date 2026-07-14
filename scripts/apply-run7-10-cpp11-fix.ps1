$ErrorActionPreference = 'Stop'

$path = Join-Path $PSScriptRoot '..\src\main.cpp'
$path = [System.IO.Path]::GetFullPath($path)

if (-not (Test-Path -LiteralPath $path)) {
    throw "Fichier introuvable: $path"
}

$content = [System.IO.File]::ReadAllText($path)

$old = @'
struct OrchestratorAuthorityDecision {
    bool useOrchestrator = false;
    OrchestratorFallbackReason fallbackReason =
        OrchestratorFallbackReason::AuthorityDisabled;
};
'@

$new = @'
struct OrchestratorAuthorityDecision {
    bool useOrchestrator;
    OrchestratorFallbackReason fallbackReason;

    OrchestratorAuthorityDecision(
        bool use = false,
        OrchestratorFallbackReason reason =
            OrchestratorFallbackReason::AuthorityDisabled
    ) : useOrchestrator(use),
        fallbackReason(reason) {}
};
'@

$occurrences = ([regex]::Matches($content, [regex]::Escape($old))).Count
if ($occurrences -ne 1) {
    throw "Bloc source attendu trouve $occurrences fois au lieu de 1. Aucun fichier modifie."
}

$updated = $content.Replace($old, $new)
if ($updated -eq $content) {
    throw 'Aucune modification produite. Aucun fichier modifie.'
}

[System.IO.File]::WriteAllText(
    $path,
    $updated,
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host 'Correctif RUN7.10 C++11 applique avec succes.'
Write-Host "Fichier: $path"
