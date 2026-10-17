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

// Rappel en tete de page (P3.25) : niveau ok / warn / bad / na, memes
// couleurs que le badge de l'accueil.
function etat(niveau, texte, detail) {
  $('etat').className = 'etat ' + niveau;
  $('etatTxt').textContent = texte;
  $('etatDetail').textContent = detail || '';
}

// Intervalle respecte : dernier succes date de moins d'un intervalle + 2 min.
// Au-dela orange (la synchro attend la fin d'un arrosage, P3.15), rouge
// au-dela de 3 intervalles.
function rythme(c, attente) {
  if (attente || !c.intervalMinutes) return 'na';
  if (!c.lastSuccessEpochSec) return 'bad';
  const sec = Date.now() / 1000 - c.lastSuccessEpochSec;
  const iv = c.intervalMinutes * 60;
  return sec <= iv + 120 ? 'ok' : sec <= 3 * iv ? 'warn' : 'bad';
}

function rendre(d) {
  const c = d.cloudSync;
  if (!c) {
    etat('na', 'Synchro cloud non configurée');
    dire('Synchro cloud non configuree sur ce module.', 'ko');
    $('cycleCard').style.display = 'none';
    $('enabled').textContent = 'Non configuree';
    $('enabled').className = 'v';
    return;
  }

  const en = $('enabled');
  en.textContent = c.enabled ? 'Activée' : 'Désactivée';
  en.className = 'v ' + (c.enabled ? 'ok' : 'bad');

  $('host').textContent = c.host ? (c.host + ':' + c.port) : '—';
  $('moduleId').textContent = c.moduleId || '—';
  $('interval').textContent = c.intervalMinutes ? (c.intervalMinutes + ' min') : '—';

  $('interval').className = 'v na';
  if (!c.enabled) {
    etat('bad', 'Synchro désactivée', "Le module n'échange plus avec le service en ligne.");
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
  // P3.30 : meme couleur que le bandeau, qui dit la meme chose (orange en
  // attente ou echec isole, rouge seulement en panne : c.fault).
  ok.className = 'v ' + (attente ? 'warn' : c.lastSyncOk ? 'ok'
    : (c.lastAttemptEpochSec ? (c.fault === true ? 'bad' : 'warn') : 'na'));

  const ry = rythme(c, attente);
  $('interval').textContent = c.intervalMinutes + ' min' +
    (ry === 'ok' ? ' · respecté' : ry === 'warn' ? ' · en retard' : ry === 'bad' ? ' · non respecté' : '');
  $('interval').className = 'v ' + ry;

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
  cu.className = 'v ' + (attente ? 'na' : c.fault === true ? 'bad' : c.serverCaughtUp ? 'ok' : 'warn');

  // c.fault : meme drapeau que le panneau rouge (FaultId::CLOUD_SYNC,
  // 3 echecs de suite) ; absent d'un ancien firmware -> jamais rouge ici.
  if (c.fault === true) {
    etat('bad', 'Synchro en panne', '3 échecs de suite ou plus ; dernière tentative ' +
      depuisEpochSec(c.lastAttemptEpochSec) + ', dernier succès ' + depuisEpochSec(c.lastSuccessEpochSec) + '.');
  } else if (attente) {
    etat('warn', 'En attente du premier échange', 'Le module vient de redémarrer.');
  } else if (!c.lastSyncOk || !c.serverCaughtUp) {
    etat('warn', 'Transmission en cours', c.lastSyncOk
      ? 'Une modification locale part au prochain échange.'
      : 'Le dernier échange a échoué, le module réessaie.');
  } else {
    const retard = ry === 'warn' || ry === 'bad';
    etat(retard ? 'warn' : 'ok', retard ? 'Synchro OK, mais en retard' : 'Synchro OK',
      'Dernier succès ' + depuisEpochSec(c.lastSuccessEpochSec) + ', serveur à jour (n°' + c.lastSyncedRevision + ').');
  }

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
