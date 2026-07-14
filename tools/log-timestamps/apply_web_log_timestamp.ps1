$ErrorActionPreference = 'Stop'

$branch = git branch --show-current
if ($branch -ne 'work/step7-run7-5') {
    throw "Branche attendue: work/step7-run7-5. Branche courante: $branch"
}

$status = git status --short
$unexpected = @($status | Where-Object { $_ -notmatch '^ M src/main\.cpp$' -and $_ -notmatch '^ M \.vscode/launch\.json$' })
if ($unexpected.Count -gt 0) {
    throw "Working tree contient des modifications inattendues:`n$($unexpected -join "`n")"
}

$path = 'src/WebManager.cpp'
$content = Get-Content -Raw -Encoding UTF8 $path

$oldHeader = 'html += F("<table><tr><th>T+</th><th>Niveau</th><th>Message</th></tr>");'
$newHeader = 'html += F("<table><tr><th>Heure / T+</th><th>Niveau</th><th>Message</th></tr>");'
$oldCode = @'
            // Temps depuis démarrage HH:MM:SS
            char tBuf[10];
            EventLog::msToHms(e.ms, tBuf, sizeof(tBuf));
'@
$newCode = @'
            // Heure locale après synchronisation NTP, sinon temps depuis démarrage.
            char tBuf[24];
            EventLog::formatEntryTimestamp(e, tBuf, sizeof(tBuf));
'@

if (-not $content.Contains($oldHeader)) {
    throw 'En-tête T+ attendu introuvable dans WebManager.cpp.'
}
if (-not $content.Contains($oldCode)) {
    throw 'Bloc msToHms attendu introuvable dans WebManager.cpp.'
}

$content = $content.Replace($oldHeader, $newHeader)
$content = $content.Replace($oldCode, $newCode)
Set-Content -Path $path -Value $content -Encoding UTF8 -NoNewline

git diff --check -- $path
if ($LASTEXITCODE -ne 0) { throw 'git diff --check a échoué.' }

git diff -- $path
Write-Host 'Patch journal Web appliqué. Compiler et uploader la V4 avant commit.'
