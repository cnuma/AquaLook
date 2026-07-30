from pathlib import Path

path = Path("src/WebManager.h")
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, found {count}")
    text = text.replace(old, new, 1)

replace_once(
    "#action{min-height:24px;margin-top:14px;font-weight:700}",
    "#action{min-height:24px;margin-top:14px;font-weight:700}.waitbox{display:none;margin-top:14px;padding:14px;border:1px solid #385064;border-radius:8px;background:#101820}.waitbox.active{display:block}.waitline{display:flex;align-items:center;gap:12px}.spinner{width:24px;height:24px;border:3px solid #385064;border-top-color:#4fc3f7;border-radius:50%;animation:spin .9s linear infinite}.waittext{flex:1}.elapsed{margin-top:8px;color:#91aabd;font-variant-numeric:tabular-nums}.progress{height:7px;margin-top:12px;overflow:hidden;border-radius:5px;background:#263b4b}.progress span{display:block;width:35%;height:100%;background:#4fc3f7;animation:travel 1.5s ease-in-out infinite}@keyframes spin{to{transform:rotate(360deg)}}@keyframes travel{0%{transform:translateX(-120%)}100%{transform:translateX(360%)}}",
    "OTA wait CSS",
)

replace_once(
    '<div id="action"></div><a href="/index.html">',
    '<div id="action"></div><div id="waitbox" class="waitbox"><div class="waitline"><span class="spinner"></span><span id="waittext" class="waittext">Operation en cours...</span></div><div id="elapsed" class="elapsed">Temps ecoule : 0 s</div><div class="progress"><span></span></div></div><a href="/index.html">',
    "OTA wait markup",
)

replace_once(
    "const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));",
    "const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));let waitTimer=null;function startWaitUi(kind){const box=document.getElementById('waitbox'),text=document.getElementById('waittext'),elapsed=document.getElementById('elapsed');box.classList.add('active');const started=Date.now();const messages=kind==='download'?['Redemarrage du module...','Connexion au reseau...','Telechargement du firmware...','Calcul et verification du SHA-256...','Finalisation et retour au fonctionnement normal...']:['Redemarrage du module...','Connexion au reseau...','Execution de la verification...','Finalisation et retour au fonctionnement normal...'];let index=0;text.textContent=messages[0];elapsed.textContent='Temps ecoule : 0 s';clearInterval(waitTimer);waitTimer=setInterval(()=>{const seconds=Math.floor((Date.now()-started)/1000);elapsed.textContent='Temps ecoule : '+seconds+' s';const next=Math.min(Math.floor(seconds/12),messages.length-1);if(next!==index){index=next;text.textContent=messages[index]}},1000)}function stopWaitUi(){clearInterval(waitTimer);waitTimer=null;document.getElementById('waitbox').classList.remove('active')}",
    "OTA wait helpers",
)

replace_once(
    "if(!confirm(question))return;check.disabled=true;download.disabled=true;probe.disabled=true;out.textContent='Preparation du redemarrage...';",
    "if(!confirm(question))return;check.disabled=true;download.disabled=true;probe.disabled=true;out.textContent='Preparation du redemarrage...';startWaitUi(kind);",
    "OTA wait start",
)

replace_once(
    "check.disabled=false;download.disabled=false;probe.disabled=false;return}",
    "check.disabled=false;download.disabled=false;probe.disabled=false;stopWaitUi();return}",
    "OTA wait error stop",
)

replace_once(
    "out.textContent='Delai d attente atteint. Rechargez la page pour consulter le resultat.';document.getElementById('check').disabled=false;",
    "stopWaitUi();out.textContent='Delai d attente atteint. Rechargez la page pour consulter le resultat.';document.getElementById('check').disabled=false;",
    "OTA wait timeout stop",
)

assert 'class="spinner"' in text
assert "startWaitUi(kind)" in text
assert "Temps ecoule" in text
assert "@keyframes travel" in text

path.write_text(text, encoding="utf-8")
