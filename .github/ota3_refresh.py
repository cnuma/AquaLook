from pathlib import Path

path = Path("src/WebManager.h")
text = path.read_text(encoding="utf-8")

old = "async function startMaintenance(kind){const check=document.getElementById('check'),download=document.getElementById('download'),probe=document.getElementById('probe'),out=document.getElementById('action');const uri=kind==='check'?'/api/maintenance/check-version':kind==='download'?'/api/maintenance/download-update-test':'/api/maintenance/probe-github';const question=kind==='check'?'AquaLook va redemarrer pour verifier la version disponible. Continuer ?':kind==='download'?'Le firmware complet sera telecharge et verifie sans etre installe. Continuer ?':'AquaLook va redemarrer pour tester GitHub. Continuer ?';if(!confirm(question))return;check.disabled=true;download.disabled=true;probe.disabled=true;out.textContent='Preparation du redemarrage...';try{const x=await fetch(uri,{method:'POST'});const j=await x.json().catch(()=>({}));if(!x.ok){out.textContent=j.error==='watering-active'?'Operation refusee : arrosage en cours.':'Erreur : '+(j.error||x.status);check.disabled=false;download.disabled=false;probe.disabled=false;return}out.textContent='Demande acceptee. Redemarrage en cours...';setTimeout(()=>{out.textContent='Operation en cours. Rechargez cette page apres le retour d AquaLook.'},1500)}catch(_){out.textContent='Connexion interrompue : le module redemarre probablement.'}}loadLast();"

new = "const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));const resultSignature=j=>j&&j.valid?[j.command||'',j.recordedUptimeMs||0,j.detail||''].join(':'):'none';async function readLastResult(){const r=await fetch('/api/maintenance/last-result',{cache:'no-store'});if(!r.ok)throw new Error('http-'+r.status);return r.json()}async function waitForMaintenanceResult(expected,baseline,timeoutMs,out){const deadline=Date.now()+timeoutMs;let offlineSeen=false;while(Date.now()<deadline){await sleep(2000);try{const j=await readLastResult();if(j.valid&&j.command===expected&&resultSignature(j)!==baseline){out.textContent='Operation terminee. Actualisation du resultat...';await sleep(500);location.reload();return}out.textContent=offlineSeen?'AquaLook est revenu. Finalisation de l operation...':'Operation en cours sur AquaLook...'}catch(_){offlineSeen=true;out.textContent='AquaLook redemarre ou traite le firmware...'}}out.textContent='Delai d attente atteint. Rechargez la page pour consulter le resultat.';document.getElementById('check').disabled=false;document.getElementById('download').disabled=false;document.getElementById('probe').disabled=false}async function startMaintenance(kind){const check=document.getElementById('check'),download=document.getElementById('download'),probe=document.getElementById('probe'),out=document.getElementById('action');const uri=kind==='check'?'/api/maintenance/check-version':kind==='download'?'/api/maintenance/download-update-test':'/api/maintenance/probe-github';const expected=kind==='check'?'check_version':kind==='download'?'download_update_test':'probe_github';const question=kind==='check'?'AquaLook va redemarrer pour verifier la version disponible. Continuer ?':kind==='download'?'Le firmware complet sera telecharge et verifie sans etre installe. Continuer ?':'AquaLook va redemarrer pour tester GitHub. Continuer ?';if(!confirm(question))return;check.disabled=true;download.disabled=true;probe.disabled=true;out.textContent='Preparation du redemarrage...';let baseline='none';try{baseline=resultSignature(await readLastResult())}catch(_){}try{const x=await fetch(uri,{method:'POST'});const j=await x.json().catch(()=>({}));if(!x.ok){out.textContent=j.error==='watering-active'?'Operation refusee : arrosage en cours.':'Erreur : '+(j.error||x.status);check.disabled=false;download.disabled=false;probe.disabled=false;return}out.textContent='Demande acceptee. Redemarrage et traitement en cours...'}catch(_){out.textContent='Connexion interrompue : AquaLook redemarre probablement.'}await waitForMaintenanceResult(expected,baseline,kind==='download'?150000:90000,out)}loadLast();"

count = text.count(old)
if count != 1:
    raise RuntimeError(f"OTA page startMaintenance anchor: expected 1 occurrence, found {count}")

text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8")

required = (
    "waitForMaintenanceResult",
    "resultSignature",
    "location.reload()",
    "kind==='download'?150000:90000",
)
for marker in required:
    if marker not in text:
        raise RuntimeError(f"missing marker after transformation: {marker}")
