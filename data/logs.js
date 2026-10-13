const $=id=>document.getElementById(id);
let journalRefreshRunning=false;
// Explications des codes courts ("[ORCH-PREVIEW]" etc.), chargees une fois
// au demarrage -- jamais dans le poll rapide du journal (voir /api/log-messages).
let catalogue={};
async function refreshCatalogue(){
 try{const d=await getJson('/api/log-messages');(d.entries||[]).forEach(e=>{catalogue[e.code]=e.texte});}catch(e){}
}
// Capture le code juste apres l'horodatage et le niveau, ex.
// "[14:32:10] [INFO] [ORCH-PREVIEW] zone=1 START pret=non" -> "ORCH-PREVIEW".
const CODE_RE=/^(\[[\d:.]+\]\s\[(?:INFO|WARN|ERR)\s?\]\s)(\[[A-Z0-9-]+\])/;
function renderJournal(text){
 const esc=s=>s.replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
 $('journal').innerHTML=text.split('\n').map(line=>{
  const cls=line.includes('[ERR')?'log-err':line.includes('[WARN]')?'log-wrn':line.includes('[INFO]')?'log-inf':'';
  let body=esc(line);
  const m=line.match(CODE_RE);
  if(m){
   const code=m[2].slice(1,-1);
   const detail=catalogue[code];
   if(detail){
    body=esc(m[1])+`<span class="log-code" title="${esc(detail)}">${esc(m[2])}</span>`+esc(line.slice(m[0].length));
   }
  }
  return cls?`<span class="${cls}">${body}</span>`:body;
 }).join('\n');
}
async function getJson(url,opt){const r=await fetch(url,{cache:'no-store',...(opt||{})});let d={};try{d=await r.json()}catch(e){}if(!r.ok)throw new Error(d.error||('HTTP '+r.status));return d}
async function refreshJournal(forceBottom=false){
 if(journalRefreshRunning)return;
 journalRefreshRunning=true;
 const box=$('journal'),state=$('journal-state');
 const wasNearBottom=box.scrollHeight-box.scrollTop-box.clientHeight<48;
 try{
  const r=await fetch('/api/logs.txt?_='+Date.now(),{cache:'no-store',headers:{'Accept':'text/plain; charset=utf-8'}});
  if(!r.ok)throw new Error('HTTP '+r.status);
  renderJournal(await r.text());
  state.textContent='Actualisé à '+new Date().toLocaleTimeString('fr-FR');
  if(forceBottom||wasNearBottom)box.scrollTop=box.scrollHeight;
 }catch(e){state.textContent='Journal indisponible : '+e.message}
 finally{journalRefreshRunning=false}
}
async function refreshFaults(){const btn=$('ack-btn'),state=$('state');try{const d=await getJson('/api/faults');state.className='';
 const labels=(d.labels||[]).join(', ');
 const detail=d.lastErrorMessage?' — '+d.lastErrorMessage+(labels?' ('+labels+')':''):(labels?' — '+labels:'');
 if(d.unacknowledged){btn.disabled=false;btn.textContent='Acquitter les erreurs';state.textContent=(d.active?'Erreur active non acquittée':'Erreur mémorisée non acquittée')+detail;state.classList.add('active')}else{btn.disabled=true;btn.textContent='Erreurs acquittées';state.textContent=(d.active?'Défaut toujours présent':'Aucune alarme non acquittée')+detail;state.classList.add('acked')}}catch(e){state.textContent='État indisponible'}}
async function ackErrors(){await fetch('/api/logs/ack',{method:'POST',cache:'no-store'});await Promise.all([refreshJournal(true),refreshFaults()])}
async function refreshIncident(){const box=$('incident'),btn=$('incident-ack');try{const d=await getJson('/api/incidents/storage-sd');const active=d.state==='active',none=d.state==='none',acked=d.state==='acknowledged';box.innerHTML='<b class="'+(active?'danger':'ok')+'">État : '+d.state+'</b><br>Occurrences : '+d.occurrences+'<br>Dernière cause : '+(d.lastReason||'-')+'<br>Notifications en attente : 0x'+Number(d.pendingNotifications||0).toString(16).padStart(2,'0');btn.disabled=active||none||acked;btn.textContent=active?'Incident actif - acquittement bloqué':(acked?'Incident acquitté':'Acquitter l\'incident')}catch(e){box.textContent='Incident indisponible';btn.disabled=true}}
async function ackIncident(){try{await getJson('/api/incidents/storage-sd/ack',{method:'POST'});await refreshIncident()}catch(e){alert('Acquittement impossible : '+e.message)}}
async function refreshLogConfig(){try{const d=await getJson('/api/logConfig');$('timing-logs').checked=!!d.timingLogsEnabled}catch(e){}}
async function setTimingLogs(enabled){try{await fetch('/api/logConfig',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({timingLogsEnabled:enabled})})}catch(e){alert('Enregistrement impossible : '+e.message)}}
async function refreshAll(){await Promise.all([refreshFaults(),refreshIncident(),refreshJournal(false)])}
refreshCatalogue().then(refreshAll);setInterval(refreshAll,2000);
refreshLogConfig();
// Gestionnaires poses ici et non en attributs on*= (pages sans script
// inline, regle des pages Web d'AGENTS.md).
$('ack-btn').addEventListener('click',ackErrors);
$('incident-ack').addEventListener('click',ackIncident);
$('journal-refresh').addEventListener('click',()=>refreshJournal(true));
$('timing-logs').addEventListener('change',e=>setTimingLogs(e.target.checked));
