const $ = (id) => document.getElementById(id);

function dire(t, e) {
  const m = $('msg');
  m.textContent = t || '';
  m.className = e || '';
}

// Meme logique que depuisEpochSec() dans app.js -- page autonome, pas de
// dependance partagee, mais le meme texte pour ne pas dire deux choses
// differentes du meme instant selon la page ouverte.
function depuisEpochSec(epochSec) {
  if (!epochSec) return 'jamais';
  const sec = Math.max(0, Date.now() / 1000 - epochSec);
  if (sec < 90) return 'il y a ' + Math.round(sec) + ' s';
  if (sec < 5400) return 'il y a ' + Math.round(sec / 60) + ' min';
  if (sec < 172800) return 'il y a ' + Math.round(sec / 3600) + ' h';
  return 'il y a ' + Math.round(sec / 86400) + ' j';
}

function rendre(d) {
  const c = d.cloudSync;
  if (!c) {
    dire('Synchro cloud non configuree sur ce module.', 'ko');
    $('cycleCard').style.display = 'none';
    $('enabled').textContent = 'Non configuree';
    $('enabled').className = 'v';
    return;
  }

  const en = $('enabled');
  en.textContent = c.enabled ? 'Activee' : 'Desactivee';
  en.className = 'v ' + (c.enabled ? 'ok' : 'warn');

  $('host').textContent = c.host ? (c.host + ':' + c.port) : '—';
  $('moduleId').textContent = c.moduleId || '—';
  $('interval').textContent = c.intervalMinutes ? (c.intervalMinutes + ' min') : '—';

  if (!c.enabled) {
    $('cycleCard').style.display = 'none';
    $('cloudLink').style.display = 'none';
    $('syncNow').style.display = 'none';
    return;
  }
  $('cycleCard').style.display = '';
  // Le module reste juge du rythme (une tentative par minute au plus) :
  // le bouton ne fait que demander, la reponse dit pourquoi si c'est non.
  const enCours = c.syncInProgress === true || c.forcePending === true;
  const sn = $('syncNow');
  sn.style.display = (c.syncInProgress === undefined) ? 'none' : '';
  sn.disabled = enCours;
  sn.textContent = enCours ? 'Echange en cours...' : 'Synchroniser maintenant';

  // resultSinceBoot === false : redemarre, aucun cycle encore. La date de
  // tentative vient de la NVS (avant le boot), le reste de la RAM : ne pas
  // en conclure un echec. Absent (ancien firmware) : comportement d'avant.
  const attente = c.resultSinceBoot === false;
  const ok = $('lastOk');
  ok.textContent = attente ? 'En attente du 1er cycle depuis le redemarrage'
    : c.lastSyncOk ? 'OK' : (c.lastAttemptEpochSec ? 'Échec' : 'Aucun cycle encore');
  ok.className = 'v ' + (attente ? '' : c.lastSyncOk ? 'ok' : (c.lastAttemptEpochSec ? 'bad' : ''));

  $('lastAttempt').textContent = depuisEpochSec(c.lastAttemptEpochSec) +
    (attente && c.lastAttemptEpochSec ? ' (avant redemarrage)' : '');
  $('lastSuccess').textContent = attente ? 'pas encore depuis le redemarrage'
    : depuisEpochSec(c.lastSuccessEpochSec);

  // 4294967295 = sentinelle "jamais confirmee" (CloudSyncScheduler::
  // _lastSyncedRevision avant le premier envoi reussi) -- ne pas l'afficher
  // comme un vrai numero de revision.
  const hasRevision = c.lastSyncedRevision != null && c.lastSyncedRevision !== 4294967295;
  $('revision').textContent = hasRevision ? ('n°' + c.lastSyncedRevision) : '—';

  const cu = $('caughtUp');
  cu.textContent = attente ? 'Inconnu (pas encore echange)' : c.serverCaughtUp ? 'À jour' : 'En retard';
  cu.className = 'v ' + (attente ? '' : c.serverCaughtUp ? 'ok' : 'warn');

  const link = $('cloudLink');
  if (c.host) {
    link.href = 'https://' + c.host + '/app.html';
    link.style.display = '';
  }
}

// N'efface que son propre message d'erreur : la reponse du bouton doit
// rester lisible jusqu'au rafraichissement suivant.
let erreurEtat = false;
async function charger() {
  try {
    const r = await fetch('/api/adminStatus', { cache: 'no-store' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    if (erreurEtat) { dire(''); erreurEtat = false; }
    rendre(await r.json());
  } catch (e) {
    dire('Etat injoignable (' + e.message + ').', 'ko');
    erreurEtat = true;
  }
}

async function forcer() {
  const b = $('syncNow');
  b.disabled = true;
  try {
    const r = await fetch('/api/cloudSyncNow', { method: 'POST' });
    const d = await r.json().catch(() => ({}));
    if (r.ok) {
      dire('Synchronisation lancee, configuration complete comprise : resultat ci-dessus dans quelques secondes.');
    } else {
      dire((d.detail || ('HTTP ' + r.status))
        + (d.retryInSec ? ' : reessayer dans ' + d.retryInSec + ' s.' : '.'), 'ko');
    }
  } catch (e) {
    dire('Demande impossible (' + e.message + ').', 'ko');
  }
  setTimeout(charger, 2000);
  setTimeout(charger, 12000);
}
$('syncNow').onclick = forcer;

charger();
setInterval(charger, 10000);
