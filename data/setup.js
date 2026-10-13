const $=id=>document.getElementById(id);
function message(text,color){$('msg').textContent=text;$('msg').style.color=color||'#eee'}
function poll(){fetch('/api/wifi/scan').then(r=>r.json()).then(d=>d.scanning?setTimeout(poll,600):done(d)).catch(fail)}
function done(d){
  const s=$('networks');
  const n=d.networks||[];
  s.disabled=false;
  if(!n.length){s.innerHTML='<option value="">-- Aucun réseau trouvé --</option>';message('Aucun réseau trouvé. Saisis le SSID manuellement.','#ff9800');return}
  s.innerHTML='<option value="">-- Sélectionner un réseau (' + n.length + ' trouvé(s)) --</option>';
  n.forEach(x=>{const o=document.createElement('option');o.value=x.ssid;o.textContent=x.ssid+' ('+x.rssi+' dBm)'+(x.secured?' 🔒':'');s.appendChild(o)});
  message(n.length+' réseau(x) trouvé(s)','#81c784')
}
function fail(){$('networks').innerHTML='<option value="">-- Résultat indisponible --</option>';message('Scan indisponible. Saisis le SSID manuellement.','#f66')}
function pick(v){if(v){$('ssid').value=v;$('pwd').focus()}}
async function saveWifi(){
  const ssid=$('ssid').value.trim(),pwd=$('pwd').value.trim();
  if(!ssid){message('SSID requis','#f66');return}
  message('Enregistrement…','#4fc3f7');
  try{
    const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pwd})});
    message(r.ok?'✓ Enregistré — redémarrage…':'Erreur serveur',r.ok?'#81c784':'#f66')
  }catch(e){message('Erreur réseau','#f66')}
}
$('networks').addEventListener('change',e=>pick(e.target.value));
$('save').addEventListener('click',saveWifi);
poll();
