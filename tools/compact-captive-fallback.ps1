param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$target = Join-Path $ProjectRoot 'src\WebManager.cpp'
if (-not (Test-Path $target -PathType Leaf)) {
    throw "Fichier cible introuvable : $target"
}

$content = Get-Content $target -Raw -Encoding UTF8
$pattern = '(?s)static const char CAPTIVE_HTML\[\] PROGMEM = R"rawhtml\(.*?\)rawhtml";'
$matches = [regex]::Matches($content, $pattern)
if ($matches.Count -ne 1) {
    throw "Bloc CAPTIVE_HTML attendu une fois, trouve $($matches.Count) fois. Aucune modification effectuee."
}

$replacement = @'
static const char CAPTIVE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AquaLook - WiFi</title><style>body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;background:#1a1a2e;color:#eee;font-family:sans-serif}.card{box-sizing:border-box;width:90%;max-width:360px;padding:2rem;background:#16213e;border-radius:12px;box-shadow:0 4px 20px #0006}h2{margin:0 0 .5rem;text-align:center;color:#4fc3f7}p{margin:0 0 1.4rem;text-align:center;color:#90caf9;font-size:.9rem;line-height:1.4}label{display:block;margin:.8rem 0 .3rem;color:#90caf9;font-size:.9rem}input,button{box-sizing:border-box;width:100%;padding:.75rem;border-radius:6px;font-size:1rem}input{border:1px solid #334;background:#0f3460;color:#eee}button{margin-top:1.2rem;border:0;background:#4fc3f7;color:#000;font-weight:700}#msg{min-height:1.2rem;margin-top:1rem;text-align:center;font-size:.9rem}</style></head><body><main class="card"><h2>&#127807; Configuration WiFi</h2><p>Mode de secours : saisissez manuellement le nom exact de votre reseau.</p><label for="ssid">Reseau WiFi (SSID)</label><input id="ssid" autocomplete="off" placeholder="Nom du reseau"><label for="pwd">Mot de passe</label><input id="pwd" type="password" placeholder="Mot de passe"><button type="button" onclick="saveWifi()">Enregistrer et connecter</button><div id="msg"></div></main><script>const $=x=>document.getElementById(x);async function saveWifi(){const s=$('ssid').value.trim(),p=$('pwd').value.trim(),m=$('msg');if(!s){m.textContent='SSID requis';m.style.color='#f66';return}m.textContent='Enregistrement...';m.style.color='#4fc3f7';try{const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,pwd:p})});m.textContent=r.ok?'Enregistre - redemarrage...':'Erreur serveur';m.style.color=r.ok?'#81c784':'#f66'}catch(e){m.textContent='Erreur reseau';m.style.color='#f66'}}</script></body></html>
)rawhtml";
'@

$updated = [regex]::Replace($content, $pattern, [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $replacement }, 1)
if ($updated -eq $content) {
    throw 'La transformation n a produit aucune modification.'
}

$backup = "$target.captive-backup"
Copy-Item $target $backup -Force

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($target, $updated, $utf8NoBom)

$remainingRichMarkers = @('startScanBar', 'pollScan', 'scanFill', 'btnScan')
foreach ($marker in $remainingRichMarkers) {
    if ($updated.Contains($marker)) {
        Copy-Item $backup $target -Force
        throw "Validation echouee : marqueur riche encore present ($marker). Fichier restaure."
    }
}

if (-not $updated.Contains('Mode de secours')) {
    Copy-Item $backup $target -Force
    throw 'Validation echouee : fallback manuel absent. Fichier restaure.'
}

Remove-Item $backup -Force
Write-Host 'CAPTIVE_HTML remplace par le fallback manuel embarque.' -ForegroundColor Green
Write-Host "Fichier modifie : $target"
Write-Host 'Le portail complet reste dans data\setup.html et sera servi depuis la SD.'
