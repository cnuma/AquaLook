const esc=v=>String(v??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
let LAST=null,WEB=null,timer=null,poll=null,BUSY=false;

// Les explications d'echec vivent sur la carte SD (/ota-help.json), pas ici.
// Deux raisons : la flash est comptee, et surtout ces textes sont ce qu'on
// corrigera le plus souvent -- les enrichir devient une mise a jour des pages
// Web, sans reflasher le module. Si le fichier manque, la page reste
// utilisable et affiche le code technique brut : aucune fonction ne depend
// de lui, seulement le confort de lecture.
let HELP=null;
// Le module accole parfois du contexte au code :
//   "content-length-mismatch version=5.9.15 fichiers=6/11"
// Le code est le premier mot, le reste est du contexte affichable. La
// recherche par egalite stricte echouait donc sur tout code enrichi, et
// renvoyait "pas encore documente" pour une entree pourtant presente.
function splitDetail(d){
  const s = String(d || '').trim();
  const i = s.indexOf(' ');
  return i < 0 ? { code: s, contexte: '' }
               : { code: s.slice(0, i), contexte: s.slice(i + 1) };
}

// Remplace les reperes du fichier d&rsquo;aide par les valeurs reelles.
// split/join et non une expression reguliere : les accolades y demandent
// un echappement, et un antislash perdu casserait la page en silence.
function subst(s, j){
  return String(s || '')
    .split('{cible}').join(j.target || 'inconnue')
    .split('{version}').join(j.installedVersion || 'inconnue');
}

function explain(j){
  const d = splitDetail(j.detail);
  let e = HELP && HELP.errors && HELP.errors[d.code];
  // Variante selon la cible du build, quand le fichier d&rsquo;aide en
  // propose une. C&rsquo;est a la page de trancher : elle connait la
  // cible, et faire choisir le lecteur entre deux paragraphes revenait a
  // lui refiler le travail.
  if(e && e.cas && j.target && e.cas[j.target]) e = e.cas[j.target];
  // Variante selon l'operation : un meme code (signature-*) vise le
  // catalogue du programme en check_version, celui des pages ailleurs.
  if(e && e.commande && e.commande[j.command]) e = e.commande[j.command];
  if(e) return { titre: e.titre || null, niveau: e.niveau || 'alerte',
                 quoi: subst(e.quoi, j), faire: subst(e.faire, j), contexte: d.contexte };

  const hp = HELP && HELP.http;
  if(hp){
    const m = String(j.httpLine || '').match(/ ([0-9][0-9][0-9])/);
    if(m){
      const k = hp[m[1]] || (m[1].charAt(0) === '5' ? hp['5xx'] : null);
      if(k) return { titre: null, niveau: 'alerte',
                     quoi: subst(k.quoi, j), faire: subst(k.faire, j), contexte: d.contexte };
    }
  }

  // Repli. Il doit encore dire une cause ET une action : un ecran qui
  // laisse deviner ce qui s&rsquo;est passe ne sert a rien.
  return { titre: null, niveau: 'alerte', contexte: d.contexte,
    quoi: d.code
      ? ('Le module a interrompu l&rsquo;operation en signalant : ' + d.code)
      : 'Le module a interrompu l&rsquo;operation sans en preciser la cause.',
    faire: HELP
      ? 'Rien n&rsquo;a ete installe : ce qui fonctionnait avant continue de fonctionner. Ce code n&rsquo;est pas encore traduit en clair ; le detail technique replie plus bas donne la reponse du serveur et l&rsquo;etat exact de l&rsquo;operation. Le bouton Tester la connexion, dans Diagnostics, dit en une fois si le probleme vient du reseau.'
      : 'Rien n&rsquo;a ete installe : ce qui fonctionnait avant continue de fonctionner. Les explications detaillees n&rsquo;ont pas pu etre lues sur la carte SD ; le detail technique replie plus bas reste disponible.' };
}
function whenTxt(j){
  if(!j.recordedEpoch)return "Date inconnue : l'horloge du module n'etait pas reglee au moment de l'operation.";
  const d=new Date(j.recordedEpoch*1000);
  const days=Math.floor((Date.now()-d.getTime())/86400000);
  const rel=days<=0?"aujourd'hui":days===1?'hier':'il y a '+days+' jours';
  return 'Constate le '+d.toLocaleString('fr-FR')+' ('+rel+').';
}
async function readLast(){const r=await fetch('/api/maintenance/last-result',{cache:'no-store'});if(!r.ok)throw new Error('http-'+r.status);return r.json()}
// Memes conditions que celles appliquees par le module (WebManager.h) : la
// page ne doit jamais proposer une etape qui sera refusee apres redemarrage.
const fwReady=j=>!!(j&&j.valid&&j.success&&j.updateAvailable&&j.firmwareUrl&&j.firmwareSize&&j.sha256);
const staged=j=>!!(j&&j.valid&&j.success&&j.command==='stage_update_test'&&j.calculatedSha256&&j.calculatedSha256===j.sha256);

function render(){
  const j=LAST,v=document.getElementById('verdict');
  const checked=j&&j.valid&&j.command==='check_version';
  const failed=j&&j.valid&&!j.success;
  // cloudWebAvailable : version des pages publiee, annoncee par le service en
  // ligne (synchro), plus recente que l'installee -- meme info que la pastille.
  const cloudWeb=(j&&j.cloudWebAvailable)||'';
  const fwAv=!!(j&&j.valid&&j.updateAvailable),webAv=!!(j&&j.valid&&j.webAssetsUpdateAvailable)||!!cloudWeb;

  // -- Verdict : la reponse a "qu&rsquo;est-ce que je fais maintenant ?".
  //
  // Un echec ne disparait JAMAIS parce qu&rsquo;une autre nouvelle serait
  // plus interessante. Quand une mise a jour est disponible ET que la
  // verification a echoue, le verdict porte la nouvelle actionnable et
  // l&rsquo;echec descend dans son propre bloc juste dessous, avec son
  // explication complete et son bouton. Sans cela il ne restait de
  // l&rsquo;echec qu&rsquo;une pastille "non verifiable", sans un mot --
  // exactement ce qu&rsquo;on reprochait a la premiere version de cette page.
  // Le titre nomme l&rsquo;operation qui a echoue. Il annoncait
  // "la verification du programme" quoi qu&rsquo;il arrive, y compris
  // pour une mise a jour des pages Web -- ce qui envoyait chercher la
  // cause du mauvais cote.
  const TITRES = {
    check_version:        'La verification des mises a jour n&rsquo;a pas abouti',
    web_assets_update:    'La mise a jour des pages Web n&rsquo;a pas abouti',
    stage_update_test:    'La preparation du nouveau programme n&rsquo;a pas abouti',
    download_update_test: 'Le test de telechargement n&rsquo;a pas abouti',
    install_update:       'L&rsquo;installation du programme n&rsquo;a pas abouti',
    probe_github:         'Le test de connexion n&rsquo;a pas abouti'
  };
  const relancer = '<button data-go="check">Relancer la verification</button>';
  const failBox = function(){
    const e = explain(j);
    // Un etat normal ne se presente pas comme une panne. "niveau": "info"
    // rend le bloc neutre et retire le bouton de relance : relancer ne
    // changerait rien, et le proposer laisserait croire le contraire.
    const info = e.niveau === 'info';
    alertBox.className = info ? 'info' : '';
    const titre = e.titre || TITRES[j.command] || 'L&rsquo;operation n&rsquo;a pas abouti';
    return '<p class="vt">' + titre + '</p>'
         + '<p>' + esc(e.quoi) + '</p>'
         + (e.contexte ? '<p class="when">Precision du module : ' + esc(e.contexte) + '</p>' : '')
         + '<p class="when">' + esc(whenTxt(j)) + '</p>'
         + '<div class="vw"><b>Que faire :</b> ' + esc(e.faire) + '</div>'
         + (info ? '' : relancer);
  };
  const alertBox = document.getElementById('alert');
  alertBox.innerHTML = '';

  if((!j || !j.valid) && !cloudWeb){
    v.className = 'verdict un';
    v.innerHTML = '<p class="vt">Aucune verification effectuee</p>'
      + '<p>Le module n&rsquo;a encore jamais interroge la source. Il ne sait donc pas s&rsquo;il existe quelque chose a installer &mdash; ce qui n&rsquo;est pas la meme chose qu&rsquo;etre a jour.</p>'
      + '<button data-go="check">Verifier maintenant</button>';
  } else if(fwAv || webAv){
    const quoi = (fwAv && webAv) ? 'Le programme du module et les pages Web'
               : fwAv ? 'Le programme du module' : 'Les pages Web';
    v.className = 'verdict av';
    v.innerHTML = '<p class="vt">&#8593; Une mise a jour est disponible</p>'
      + '<p>' + quoi + ' peuvent etre mis a jour. '
      + (fwAv ? 'Pour le programme, suivez les etapes 1 a 3 ci-dessous.' : 'Un seul bouton suffit, plus bas.') + '</p>'
      + (cloudWeb ? '<p class="when">Pages Web ' + esc(cloudWeb) + ' publiees (annonce du service en ligne).</p>' : '')
      + (j.valid ? '<p class="when">' + esc(whenTxt(j)) + '</p>' : '');
    if(failed) alertBox.innerHTML = failBox();
  } else if(failed){
    // Le niveau vient du fichier d&rsquo;aide : un etat normal ne prend
    // pas les couleurs d&rsquo;une panne, meme quand c&rsquo;est le
    // verdict lui-meme qui le porte.
    const html = failBox();
    v.className = (alertBox.className === 'info') ? 'verdict un' : 'verdict ko';
    alertBox.className = '';
    v.innerHTML = html;
  } else {
    v.className='verdict ok';
    v.innerHTML='<p class="vt">&#10003; Tout est a jour</p><p>Ni le programme du module ni les pages Web n\'ont de version plus recente disponible.</p>'+
      '<p class="when">'+esc(whenTxt(j))+'</p>';
  }

  // ── Les deux canaux, avec des mots et non des chiffres nus.
  const wi=WEB?(WEB.version||WEB.gitSha):null;
  const chan=(n,inst,av,txt,cls,note)=>'<div class="chan"><span class="cn">'+esc(n)+'</span><span class="cv"><i>installee :</i> '+
    esc(inst||'inconnue')+(av?' &nbsp; <i>disponible :</i> '+esc(av):'')+(note?' &nbsp; <i>'+esc(note)+'</i>':'')+
    '</span><span class="chip '+cls+'">'+esc(txt)+'</span></div>';
  let h='';
  // Le programme se juge sur SA propre verification. La ligne affichait
  // "a jour" des que la derniere operation avait reussi, y compris quand
  // c'etait une mise a jour des pages Web -- qui ne dit rien du programme.
  // Le module ne gardant qu'un seul resultat pour deux canaux, la seule
  // reponse honnete hors verification est de ne pas se prononcer.
  const fwVerifie = !!(j && j.valid && j.command === 'check_version');
  if(!j || !j.valid) h += chan('Programme du module', j && j.installedVersion, null, 'jamais verifie', '', '');
  else if(fwAv) h += chan('Programme du module', j.installedVersion, j.availableVersion, 'a installer', 'av', '');
  else if(!fwVerifie) h += chan('Programme du module', j.installedVersion, null, 'non verifie', '', '');
  else if(failed) h += chan('Programme du module', j.installedVersion, null,
        j.target === 'unsupported' ? 'aucune version publiee' : 'non verifiable', 'ko', '');
  else h += chan('Programme du module', j.installedVersion, null, 'a jour', 'up', '');
  // Le canal Web se juge sur SA propre reponse, jamais sur j.success qui
  // est le resultat du canal firmware. Une version disponible renseignee
  // prouve que checkForUpdate() est alle au bout : il ne la remplit
  // qu&rsquo;apres avoir lu et valide le catalogue.
  // Deux preuves valent : une version disponible renseignee (une
  // verification est allee au bout), ou un deploiement reussi -- le
  // firmware efface la version disponible en le consommant, alors que
  // c'est justement le moment ou le canal a le plus travaille.
  const webRepondu = !!(j && (j.webAssetsAvailableVersion ||
        (j.command === 'web_assets_update' && j.success)));
  if(webAv) h += chan('Pages Web', wi || (j && j.webAssetsInstalledVersion), (j && j.webAssetsAvailableVersion) || cloudWeb, 'a installer', 'av', '');
  else if(webRepondu) h += chan('Pages Web', wi, null, 'a jour', 'up', '');
  else h += chan('Pages Web', wi, null, 'jamais verifiees', '', 'sur le module');
  document.getElementById('chans').innerHTML=h;

  // ── Etapes verrouillees, avec la raison du verrouillage.
  const ok2=fwReady(j),ok3=staged(j);
  gate('s2','b-stage','w2',ok2, checked&&j.success&&!j.updateAvailable
    ? "Aucune mise a jour du programme n'est disponible : il n'y a rien a preparer."
    : failed ? "La verification n'a pas abouti (voir en haut de page). Corrigez la cause, puis relancez l'etape 1."
    : "Lancez d'abord l'etape 1. Attention : le module ne conserve qu'un seul resultat, donc une mise a jour des pages Web efface celui de la verification — il faut alors relancer l'etape 1.");
  gate('s3','b-install','w3',ok3,"Lancez d'abord l'etape 2 : rien n'a encore ete prepare et verifie dans la partition de reserve.");
  const wb=document.getElementById('b-web'),ww=document.getElementById('wweb');
  wb.textContent=webAv?('Installer les pages Web '+((j&&j.webAssetsAvailableVersion)||cloudWeb)):'Mettre a jour les pages Web';
  ww.style.display=webAv?'none':'block';
  ww.textContent=webAv?'':"Aucune version plus recente n'a ete reperee. Le bouton reste utilisable : il reinstalle ce que la source publie actuellement, ce qui repare des pages abimees.";
  renderLast();
}
function gate(sid,bid,wid,ok,why){const s=document.getElementById(sid),b=document.getElementById(bid),w=document.getElementById(wid);
  s.className='step '+(ok?'on':'off');b.disabled=!ok||BUSY;w.style.display=ok?'none':'block';w.textContent=ok?'':why}
function renderLast(){const e=document.getElementById('last'),j=LAST;
  if(!j||!j.valid){e.innerHTML='<p class="sub">Aucune operation enregistree.</p>';return}
  const row=(a,b,c)=>'<div class="row"><span class="lb">'+esc(a)+'</span><span class="vl'+(c?' '+c:'')+'">'+esc(b)+'</span></div>';
  const r=[['Operation',j.command||'inconnue'],['Resultat',j.success?'reussie':'echouee',j.success?'v-ok':'v-ko']];
  if(j.recordedEpoch)r.push(['Date',new Date(j.recordedEpoch*1000).toLocaleString('fr-FR')]);
  if(j.httpLine)r.push(['Reponse HTTP',j.httpLine]);
  if(j.detail)r.push(['Code interne',j.detail,j.success?'v-ok':'v-ko']);
  if(j.target)r.push(['Cible attendue',j.target]);
  if(j.board)r.push(['Carte',j.board]);
  if(j.environment)r.push(['Environnement',j.environment]);
  if(j.channel)r.push(['Canal',j.channel]);
  r.push(['Programme installe',j.installedVersion||'-'],['Programme disponible',j.availableVersion||'-'],
         ['Pages installees',j.webAssetsInstalledVersion||'-'],['Pages disponibles',j.webAssetsAvailableVersion||'-']);
  if(j.command==='download_update_test'||j.command==='stage_update_test')
    r.push(['Octets telecharges',j.downloadedSize||0],['Empreinte attendue',j.sha256||'-'],
           ['Empreinte calculee',j.calculatedSha256||'-',j.calculatedSha256&&j.calculatedSha256===j.sha256?'v-ok':'v-ko']);
  r.push(['Duree TLS',(j.tlsDurationMs||0)+' ms'],['Memoire libre minimale',(j.minFreeHeap||0)+' octets']);
  e.innerHTML=r.map(x=>row(x[0],x[1],x[2])).join('')}

async function refresh(){
  if(HELP===null){try{const r=await fetch('/ota-help.json',{cache:'no-store'});HELP=r.ok?await r.json():false}catch(_){HELP=false}}
  try{LAST=await readLast()}catch(_){}
  try{const r=await fetch('/assets-version.json',{cache:'no-store'});WEB=r.ok?await r.json():WEB}catch(_){}
  render()}
// Rafraichissement autonome : apres une verification, l'etat et les etapes se
// mettent a jour seuls. Sans cela il fallait recharger la page a la main pour
// que l'etape suivante se debloque.
function startPoll(){clearInterval(poll);poll=setInterval(()=>{if(!BUSY)refresh()},10000)}

function startWait(k){const b=document.getElementById('wait'),t=document.getElementById('wt'),el=document.getElementById('el');b.classList.add('on');const t0=Date.now();
  const m=k==='web'?['Redemarrage en mode maintenance...','Lecture du catalogue publie...','Telechargement et verification des fichiers...','Bascule des nouvelles pages...','Retour au fonctionnement normal...']
   :k==='install'?['Redemarrage du module...','Activation du nouveau programme...','Verification du demarrage...','Retour au fonctionnement normal...']
   :['Redemarrage du module...','Connexion au reseau...','Operation en cours...','Retour au fonctionnement normal...'];
  let i=0;t.textContent=m[0];el.textContent='Temps ecoule : 0 s';clearInterval(timer);
  timer=setInterval(()=>{const s=Math.floor((Date.now()-t0)/1000);el.textContent='Temps ecoule : '+s+' s';
    const n=Math.min(Math.floor(s/12),m.length-1);if(n!==i){i=n;t.textContent=m[i]}},1000)}
function stopWait(){clearInterval(timer);timer=null;document.getElementById('wait').classList.remove('on')}
const sig=j=>j&&j.valid?[j.command||'',j.recordedUptimeMs||0,j.detail||''].join(':'):'none';
function busy(v){BUSY=v;['b-check','b-stage','b-install','b-web'].forEach(id=>{const e=document.getElementById(id);if(e)e.disabled=v})}

async function go(k){
  const uri={check:'/api/maintenance/check-version',download:'/api/maintenance/download-update-test',stage:'/api/maintenance/stage-update-test',install:'/api/maintenance/install-update',probe:'/api/maintenance/probe-github'}[k];
  const exp={check:'check_version',download:'download_update_test',stage:'stage_update_test',install:'install_update',probe:'probe_github'}[k];
  const q={check:'Le module va redemarrer environ 30 secondes pour verifier les versions disponibles. Rien ne sera installe. Continuer ?',
   download:'Le programme sera telecharge et son empreinte verifiee, sans rien ecrire. Continuer ?',
   stage:'Le programme sera telecharge, verifie, puis ecrit dans la partition de reserve. Il ne sera PAS active. Continuer ?',
   install:'Le programme prepare va etre active et le module redemarrera dessus. En cas de redemarrages repetes sans validation, le programme precedent est restaure automatiquement. Continuer ?',
   probe:'Le module va redemarrer pour tester la connexion a la source. Continuer ?'}[k];
  if(!confirm(q))return;
  const out=document.getElementById('act');busy(true);out.textContent='Preparation du redemarrage...';startWait(k);
  let base='none';try{base=sig(await readLast())}catch(_){}
  try{const x=await fetch(uri,{method:'POST'});const j=await x.json().catch(()=>({}));
    if(!x.ok){out.textContent=j.error==='watering-active'?'Refuse : un arrosage est en cours. Reessayez apres.'
      :j.error==='check-version-required'?"Refuse : relancez l'etape 1, le module n'a plus de verification valide en memoire."
      :j.error==='stage-required'?"Refuse : l'etape 2 n'a pas ete faite."
      :'Erreur : '+(j.error||x.status);
      stopWait();busy(false);await refresh();return}
    out.textContent='Demande acceptee. Redemarrage et traitement en cours...'}
  catch(_){out.textContent='Connexion interrompue : le module redemarre.'}
  await waitResult(exp,base,k==='download'?150000:90000,out)}

async function waitResult(exp,base,ms,out){const dl=Date.now()+ms;let off=false;
  while(Date.now()<dl){await sleep(2000);
    try{const j=await readLast();
      if(j.valid&&j.command===exp&&sig(j)!==base){out.textContent='Operation terminee.';stopWait();busy(false);await refresh();
        setTimeout(()=>{out.textContent=''},6000);return}
      out.textContent=off?'Le module est revenu. Finalisation...':'Operation en cours sur le module...'}
    catch(_){off=true;out.textContent='Le module redemarre...'}}
  stopWait();busy(false);out.textContent='Delai atteint. L\'etat ci-dessus se remet a jour tout seul.';await refresh()}

async function goWeb(){const out=document.getElementById('act');
  if(!confirm("Le module va redemarrer en mode maintenance pour mettre a jour les pages Web. L'arrosage n'est pas affecte. Continuer ?"))return;
  busy(true);out.textContent='Preparation du redemarrage...';startWait('web');
  try{const x=await fetch('/api/webassets/update',{method:'POST'});const j=await x.json().catch(()=>({}));
    if(!x.ok){out.textContent=(j.error||'').indexOf('arrosage')>=0?'Refuse : un arrosage est en cours. Reessayez apres.':'Erreur : '+(j.error||x.status);
      stopWait();busy(false);await refresh();return}
    out.textContent='Demande acceptee. Redemarrage et mise a jour en cours...'}
  catch(_){out.textContent='Connexion interrompue : le module redemarre.'}
  const dl=Date.now()+180000;
  while(Date.now()<dl){await sleep(3000);
    try{const r=await fetch('/assets-version.json',{cache:'no-store'});
      if(r.ok){out.textContent='Mise a jour terminee.';stopWait();busy(false);await refresh();
        setTimeout(()=>{out.textContent=''},6000);return}}catch(_){}}
  stopWait();busy(false);out.textContent="Delai atteint. L'etat ci-dessus se remet a jour tout seul.";await refresh()}

// Boutons : delegation sur data-go, sans attribut onclick (pages sans script
// inline, P3.11). Les boutons du verdict sont recrees par render().
document.addEventListener('click',ev=>{const b=ev.target.closest('[data-go]');if(!b||b.disabled)return;
  const k=b.dataset.go;if(k==='web')goWeb();else go(k)});
refresh();startPoll();
