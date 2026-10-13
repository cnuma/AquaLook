const $ = (id) => document.getElementById(id);

function dire(t, e) {
  const m = $('msg');
  m.textContent = t || '';
  m.className = e || '';
}

async function poster(url) {
  const r = await fetch(url, { method: 'POST' });
  const d = await r.json().catch(function () { return {}; });
  return { ok: r.ok && d.ok !== false, data: d };
}

async function leverDegrade() {
  const { ok, data } = await poster('/api/bootguard/clear');
  dire(ok ? 'Mode degrade leve : les fonctions suspendues reprennent immediatement.'
          : ('Echec : ' + (data.error || 'raison inconnue')), ok ? 'ok' : 'ko');
  charger();
}

async function acquitter(sdNeedsAck) {
  const { ok, data } = await poster('/api/logs/ack');
  if (ok && sdNeedsAck) await poster('/api/incidents/storage-sd/ack');
  dire(ok ? 'Acquitte.' : ('Echec : ' + (data.error || 'raison inconnue')), ok ? 'ok' : 'ko');
  charger();
}

function rendreActions(d) {
  const sdNeedsAck = d.storageSd && d.storageSd.state === 'recovered-unacknowledged';
  let h = '';
  if (d.overall === 'degrade') {
    h += '<button class="principal" data-go="lever">Lever le mode degrade</button>';
  } else if (d.unacknowledged || sdNeedsAck) {
    h += '<button data-go="acquitter" data-sd="' + (sdNeedsAck ? '1' : '') + '">Acquitter</button>';
  }
  $('actions').innerHTML = h;
}

function rendre(d) {
  const ov = $('overall');
  const overallLabel = { degrade: 'DEGRADE', essai: 'A l’essai', normal: 'Normal' }[d.overall] || d.overall;
  const overallClass = { degrade: 'bad', essai: 'warn', normal: 'ok' }[d.overall] || '';
  ov.textContent = overallLabel;
  ov.className = 'v ' + overallClass;

  const bl = d.bootLoop || {};
  $('boots').textContent = (bl.suspectBoots || 0) + ' / ' + (bl.threshold || 4);

  const sd = d.storageSd || {};
  const sdEl = $('sd');
  if (!sd.state || sd.state === 'none') {
    sdEl.textContent = 'Aucun incident'; sdEl.className = 'v ok';
  } else if (sd.state === 'active') {
    sdEl.textContent = 'En panne'; sdEl.className = 'v bad';
  } else if (sd.state === 'recovered-unacknowledged') {
    sdEl.textContent = 'Retablie (a acquitter)'; sdEl.className = 'v warn';
  } else {
    sdEl.textContent = 'OK'; sdEl.className = 'v ok';
  }

  const no = $('notif');
  no.textContent = (d.notifications && d.notifications.enabled) ? 'Actives' : 'Coupees';
  no.className = 'v ' + ((d.notifications && d.notifications.enabled) ? 'ok' : '');

  const faults = (d.faults || []);
  $('faults').innerHTML = faults.length
    ? faults.map(function (f) { return '<li>' + f.label + '</li>'; }).join('')
    : '<li class="vide" style="color:var(--muted)">Aucun</li>';

  rendreActions(d);
}

async function charger() {
  try {
    const r = await fetch('/api/health', { cache: 'no-store' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    rendre(await r.json());
  } catch (e) {
    dire('Sante injoignable (' + e.message + ').', 'ko');
  }
}

function ecrire(id, t, c) {
  const el = $(id);
  el.textContent = t;
  el.className = 'v ' + (c || '');
}

async function lireJson(url) {
  try {
    const r = await fetch(url, { cache: 'no-store' });
    return r.ok ? await r.json() : null;
  } catch (e) { return null; }
}

// Une seule fois : rien de tout cela ne change sans redemarrage. Sources :
// /api/diagnostics.build (meme macros que le LCD), /assets-version.json
// (pages installees), /api/maintenance/last-result (meme lecture que /ota).
async function chargerAPropos() {
  const [diag, web, res] = await Promise.all([
    lireJson('/api/diagnostics'),
    lireJson('/assets-version.json'),
    lireJson('/api/maintenance/last-result')
  ]);
  const b = (diag && diag.build) || null;
  if (b) {
    ecrire('ap-produit', 'AquaLook - ' + (b.productLine || '?'));
    ecrire('ap-version', b.version || '?');
    ecrire('ap-build', (b.number || '?') + ' (SHA ' + (b.gitSha || '?') + ')');
    ecrire('ap-branche', b.gitBranch || '?');
    ecrire('ap-date', (b.compiledDate || '') + ' ' + (b.compiledTime || ''));
  } else {
    ['ap-produit', 'ap-version', 'ap-build', 'ap-branche', 'ap-date']
      .forEach(function (id) { ecrire(id, 'injoignable', 'warn'); });
  }
  const wv = web && (web.version || web.gitSha);
  ecrire('ap-web', wv || 'inconnue', wv ? '' : 'warn');

  if (!(res && res.valid && res.command === 'check_version')) {
    ecrire('ap-majfw', 'pas de verif. recente');
    ecrire('ap-majweb', 'pas de verif. recente');
    return;
  }
  if (res.updateAvailable && res.availableVersion) {
    ecrire('ap-majfw', res.availableVersion + ' disponible', 'warn');
  } else {
    ecrire('ap-majfw', 'a jour', 'ok');
  }
  if (res.webAssetsUpdateAvailable && res.webAssetsAvailableVersion) {
    ecrire('ap-majweb', res.webAssetsAvailableVersion + ' disponible', 'warn');
  } else {
    ecrire('ap-majweb', 'a jour', 'ok');
  }
}

// Boutons : delegation sur data-go, sans attribut onclick (pages sans script
// inline, regle des pages Web d'AGENTS.md).
document.addEventListener('click', function (ev) {
  const b = ev.target.closest('[data-go]');
  if (!b || b.disabled) return;
  if (b.dataset.go === 'lever') leverDegrade();
  else if (b.dataset.go === 'acquitter') acquitter(b.dataset.sd === '1');
});

charger();
chargerAPropos();
setInterval(charger, 4000);
