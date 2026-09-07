const DAYS_ESP = ['Lun','Mar','Mer','Jeu','Ven','Sam','Dim'];
const DAYS_FULL = ['Lundi','Mardi','Mercredi','Jeudi','Vendredi','Samedi','Dimanche'];
function getNbZones()  { return status?.zones?.length || 2; }
function getZoneName(i){ return status?.zones?.[i]?.name || `Zone ${i+1}`; }
function isZoneActive(i){ return !!(status?.zones?.[i]?.active || status?.zones?.[i]?.relayActive); }
let status      = null;
let adminStatus = null;
let notificationConfig = null;
let modalZone   = -1, modalDay = -1, modalIsInterval = false;
const _zoneSlots = [];
async function loadZoneSlots(z) {
  try {
    const r = await fetch(`/api/zone?z=${z}`);
    if (!r.ok) return null;
    const data = await r.json();
    _zoneSlots[z] = data;
    return data;
  } catch(e) { return null; }
}
async function ensureAllZoneSlots() {
  const nb = getNbZones();
  const missing = [];
  for (let z = 0; z < nb; z++) {
    if (!_zoneSlots[z]) missing.push(z);
  }
  if (missing.length === 0) return;
  await Promise.all(missing.map(z => loadZoneSlots(z)));
}
let _fetching = false;
let _fetchTimer = null;
async function fetchStatus() {
  if (_fetching) { console.log('[fetch] skipped -- already fetching'); return; }
  _fetching = true;
  _fetchTimer = setTimeout(() => {
    console.log('[fetch] watchdog fired -- resetting _fetching');
    _fetching = false;
  }, 6000);
  try {
    console.log('[fetch] start');
    const r = await fetch('/api/status');
    console.log('[fetch] response', r.status);
    status = await r.json();
    console.log('[fetch] ok, zones=', status?.zones?.length);
    renderAll();
  } catch(e) {
    console.log('[fetch] error', e);
    addLog('Erreur connexion');
  } finally {
    clearTimeout(_fetchTimer);
    _fetching = false;
  }
}
async function fetchAdminStatus(isRetry) {
  try {
    const r = await fetch('/api/adminStatus');
    adminStatus = await r.json();
    populateDrawer();
  } catch(e) {
    // Reseau WiFi instable par moments (voir keepalive) : un premier echec
    // silencieux laissait le tiroir de reglages avec des champs vides
    // (impression qu'il fallait tout ressaisir) sans jamais reessayer.
    // Un seul retry rapide couvre l'immense majorite des ratés ponctuels ;
    // au-dela, le journal en informe plutot que de rester muet.
    if (!isRetry) {
      setTimeout(() => fetchAdminStatus(true), 1500);
    } else {
      addLog('Parametres non rafraichis (reseau)');
    }
  }
}
function jsToEsp(d) { return d === 0 ? 6 : d - 1; }
function getTodayEspIdx() { return jsToEsp(new Date().getDay()); }
function todayEpochDay(){return Math.floor(Date.now()/86400000)}
function epochDayToIso(d){return d?new Date(d*86400000).toISOString().slice(0,10):''}
function isoToEpochDay(s){const t=Date.parse(s+'T00:00:00Z');return Number.isFinite(t)?Math.floor(t/86400000):0}
function epochDayLabel(d){return d?new Date(d*86400000).toLocaleDateString('fr-FR',{weekday:'long',day:'numeric',month:'long',year:'numeric',timeZone:'UTC'}):'Non definie'}
function renderAll() {
  if (!status) return;
  document.getElementById('wifi-badge').textContent =
    status.synced ? status.time.slice(11,16) : 'NTP...';
  document.getElementById('update-badge').style.display =
    status.updatePending ? 'inline-block' : 'none';
  renderZones();
  // Sans ce catch, une exception ici (reseau ou JS) laissait la carte
  // Planning definitivement vide et totalement silencieuse — aucune trace
  // nulle part pour comprendre pourquoi. Le cycle suivant (fetchStatus,
  // 8s) retente de lui-meme ; ce catch sert uniquement a ne jamais perdre
  // l'erreur en route.
  ensureAllZoneSlots()
    .then(() => renderPlanning())
    .catch(e => {
      console.error('[renderPlanning]', e);
      addLog('Erreur affichage planning : ' + e.message);
    });
}
// Les noms ci-dessous ne servent plus qu'aux classes CSS de structure
// (fond de ligne active, pastille). La COULEUR reelle d'une zone vient
// desormais de la zone elle-meme -- voir zoneHex().
const ZONE_COLORS = ['green','blue','amber','purple','green','blue','amber','purple',
                     'green','blue','amber','purple','green','blue','amber','purple'];

// Couleur d'identite d'une zone, en "#rrggbb".
//
// Chaque zone porte la sienne depuis le schema NVS 4. Avant cela, quatre
// couleurs etaient partagees par `zone % 4` : deux zones se ressemblaient des
// la cinquieme, qui n'etait pas reglable du tout. Le repli sur l'ancienne
// palette couvre un module dont le firmware ne renvoie pas encore le champ.
const ZONE_FALLBACK_HEX = ['#00fc00', '#0090f8', '#f8a400', '#780078'];
function zoneHex(z, i) {
  if (z && typeof z.color === 'string' && z.color[0] === '#') return z.color;
  const key = ['cZone0','cZone1','cZone2','cZone3'][i % 4];
  return (displayConfig && displayConfig[key]) || ZONE_FALLBACK_HEX[i % 4];
}
let _zonesView = 'normal';
try { _zonesView = localStorage.getItem('zonesView') || 'normal'; } catch(e) {}
function setZonesView(v) {
  _zonesView = v;
  try { localStorage.setItem('zonesView', v); } catch(e) {}
  _applyZonesViewBtn();
  renderZones();
}
function _applyZonesViewBtn() {
  const btn = document.getElementById('btn-zones-view');
  if (!btn) return;
  if (_zonesView === 'dense') {
    btn.textContent    = 'Desactiver';
    btn.style.background    = 'var(--green-dim)';
    btn.style.borderColor   = 'var(--green-mid)';
    btn.style.color         = 'var(--green)';
  } else {
    btn.textContent    = 'Activer';
    btn.style.background    = 'var(--bg2)';
    btn.style.borderColor   = 'var(--border2)';
    btn.style.color         = 'var(--text2)';
  }
}
function renderZones() {
  if (_zonesView === 'dense') renderZonesTable();
  else                        renderZonesGrid();
}
// Tuiles de zone : l'etat des lieux de l'installation d'un coup d'oeil.
//
// Elles ne CONFIGURENT rien. Seules deux actions y figurent, parce que ce
// sont des actions et non des reglages : la marche forcee agit maintenant,
// l'identification aussi. Tout ce qui definit un comportement futur vit
// dans Parametres, et les creneaux dans le planning juste en dessous - ou
// ils s'editent en cliquant une case.
//
// Le mode et le seuil de pluie sont rappeles ici bien qu'ils se reglent
// ailleurs : ils dependent des plantes de chaque zone, et c'est
// precisement l'information qu'on veut voir sans naviguer.
function renderZonesGrid() {
  const el     = document.getElementById('zones-container');
  const manDur = status.manualDurationMin || 10;
  el.className = 'zone-tiles';
  el.style.cssText = '';
  el.innerHTML = status.zones.map((z, i) => {
    const name   = z.name || `Zone ${i+1}`;
    const active = z.relayActive || z.active;
    const color  = ZONE_COLORS[i];
    const modeStr = z.mode === 1
      ? `Intervalle / ${z.intervalDays||z.interval||2}j`
      : 'Jours fixes';
    const threshMm = z.rain?.threshMm ?? z.rainThresh ?? 2;
    const hours    = z.rain?.hours    ?? z.rainHours  ?? 24;
    const reason   = z.reason || z.lastReason || 'En attente';
    // Une zone sans voie physique ne pourra jamais arroser : le dire, plutot
    // que d'afficher un motif sans rapport et un bouton qui n'agira pas.
    const noOut = (z.hasOutput === false);
    return `
    <div class="zone-tile zone-color-${color} ${active ? 'zone-card-active' : ''} ${noOut ? 'zone-unassigned' : ''}" style="--zc:${zoneHex(z, i)}">
      <div class="zt-head">
        <span class="zone-dot zone-dot-${color}"></span>
        <span class="zt-name" title="${name}">${name}</span>
        <span class="zone-badge ${noOut ? 'off' : (active ? 'on' : 'off')}" title="${noOut ? 'Aucune sortie physique affectee' : ''}">${noOut ? 'N/C' : (active ? 'ON' : 'OFF')}</span>
      </div>
      <div class="zt-facts">
        <span class="zt-fact" title="Mode de programmation">&#128197; ${modeStr}</span>
        <span class="zt-fact" title="Arrosage suspendu au-dela de ce cumul de pluie">&#9748; &ge;${threshMm}mm / ${hours}h</span>
      </div>
      <div class="zt-reason" title="${noOut ? 'Aucune sortie physique affectee a cette zone' : reason}">${noOut ? '&#9888; Aucune sortie affectee &mdash; a definir dans le cablage' : reason}</div>
      <div class="zt-actions">
        <button class="btn-run ${active ? 'active' : ''}" ${noOut ? 'disabled' : ''}
                onclick="toggleManual(${i}, ${!active})">
          ${noOut ? 'Sans sortie' : (active ? 'Arreter' : 'Arroser ' + manDur + ' min')}
        </button>
        <button class="btn-identify" title="Fait clignoter en blanc la LED de cette zone, pour la reperer au branchement"
                onclick="identifyZone(${i})">&#128161;</button>
      </div>
    </div>`;
  }).join('');
}
// Identification physique d'une zone : sa LED clignote en blanc sur le
// ruban, le temps de reperer a quel bornier elle correspond. Le blanc est
// la seule couleur qu'aucun etat n'utilise (bleu = arrosage, orange =
// pluie, vert/ambre/violet/rouge = etats du module).
//
// N'agit ni sur les relais ni sur la configuration : rien n'est arrose, et
// le clignotement s'arrete tout seul cote module.
function identifyZone(i) {
  fetch('/api/zoneIdentify', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ zone: i, seconds: 20 })
  }).then(function (r) {
    if (!r.ok) throw new Error('HTTP ' + r.status);
    if (typeof toast === 'function') toast('Zone ' + (i + 1) + ' : LED blanche pendant 20 s');
  }).catch(function (e) {
    if (typeof toast === 'function') toast('Identification impossible : ' + e.message);
  });
}

function renderZonesTable() {
  const el     = document.getElementById('zones-container');
  const manDur = status.manualDurationMin || 10;
  el.style.cssText = '';
  const rows = status.zones.map((z, i) => {
    const name   = z.name || `Zone ${i+1}`;
    const active = z.relayActive || z.active;
    const color  = ZONE_COLORS[i];
    const mode   = z.mode === 1
      ? `Intervalle&nbsp;/${z.intervalDays||z.interval||2}j`
      : 'Jours fixes';
    const threshMm = z.rain?.threshMm ?? z.rainThresh ?? 2;
    const hours    = z.rain?.hours    ?? z.rainHours  ?? 24;
    const reason   = z.reason || z.lastReason || 'En attente';
    const noOut    = (z.hasOutput === false);
    return `<tr class="${active ? 'zone-active-'+color : ''} ${z.mode === 1 ? 'zone-interval' : ''} ${noOut ? 'zone-unassigned' : ''}"
                onclick="openZoneConfigModal(${i})"
                title="Configurer ${name}">
      <td class="zt-name zt-name-${color}" style="--zc:${zoneHex(z, i)}">
        <span class="zone-dot zone-dot-${color}"></span>
        ${name}
        <span class="zt-badge ${noOut ? 'off' : (active ? 'on' : 'off')}">${noOut ? 'N/C' : (active ? 'ON' : 'OFF')}</span>
      </td>
      <td class="zt-cell zt-mode">${mode}</td>
      <td class="zt-cell zt-rain">&#9748;&nbsp;&ge;&nbsp;${threshMm}mm&nbsp;/&nbsp;${hours}h</td>
      <td class="zt-cell zt-reason">${noOut ? '&#9888; Aucune sortie affectee' : reason}</td>
      <td class="zt-cell zt-edit-hint">&#9998;</td>
      <td class="zt-action" onclick="event.stopPropagation()">
        <button class="btn-run ${active ? 'active' : ''}" ${noOut ? 'disabled' : ''}
                onclick="toggleManual(${i}, ${!active})">
          ${noOut ? 'Sans&nbsp;sortie' : (active ? 'Arreter' : 'Arroser&nbsp;'+manDur+'&nbsp;min')}
        </button>
      </td>
    </tr>`;
  }).join('');
  el.innerHTML = `<table class="zones-table"><tbody>${rows}</tbody></table>`;
}
function openZoneConfigModal(zoneIdx) {
  const z      = status.zones[zoneIdx];
  const name   = z.name || `Zone ${zoneIdx+1}`;
  const color  = ZONE_COLORS[zoneIdx];
  const zHex   = zoneHex(z, zoneIdx);
  const mode   = z.mode ?? 0;
  const intD   = z.intervalDays || z.interval || 2;
  const anchorDay = z.intervalAnchorDay || 0;
  const anchorIso = epochDayToIso(anchorDay || todayEpochDay());
  const thresh = z.rain?.threshMm ?? z.rainThresh ?? 2;
  const hours  = z.rain?.hours    ?? z.rainHours  ?? 24;
  const notifyStart = z.notifyStart ?? ((z.notificationMask & 1) !== 0);
  const notifyStop  = z.notifyStop  ?? ((z.notificationMask & 2) !== 0);
  document.getElementById('modal-title-text').textContent = `Config -- ${name}`;
  document.getElementById('modal-body').innerHTML = `
    <div class="zone-cfg-modal-header">
      <!-- Meme ordre que la liste "Reglages par zone" : pastille, reference,
           nom. Deux ordres differents pour la meme information se lisent
           comme deux informations differentes. -->
      <span class="zone-dot" id="zcfg-dot"
            style="width:12px;height:12px;background:${zHex}"></span>
      <span class="cfg-zone-ref">Z${zoneIdx + 1}</span>
      <span class="zone-cfg-modal-name">${name}</span>
    </div>
    <div class="zone-cfg-field">
      <label>Nom</label>
      <input type="text" id="zcfg-name" value="${name}" maxlength="20" placeholder="Nom de la zone">
    </div>
    <div class="zone-cfg-field">
      <label>Couleur</label>
      <input type="color" id="zcfg-color" class="cfg-color" value="${zHex}"
             oninput="document.getElementById('zcfg-dot').style.background = this.value">
      <div class="cfg-hint">Identifie la zone sur l&rsquo;&eacute;cran, le planning et le ruban lumineux.</div>
    </div>
    <hr class="zone-cfg-sep">
    <div class="zone-cfg-field">
      <label>Mode planification</label>
      <select id="zcfg-mode" onchange="zcfgToggleInterval()">
        <option value="0" ${mode===0?'selected':''}>Jours fixes</option>
        <option value="1" ${mode===1?'selected':''}>Intervalle</option>
      </select>
    </div>
    <div id="zcfg-interval-row" class="zone-cfg-row"
         style="display:${mode===1?'grid':'none'}">
      <div class="zone-cfg-field" style="margin-bottom:0">
        <label>Intervalle (jours)</label>
        <input type="number" id="zcfg-intd" min="1" max="30" value="${intD}">
      </div>
      <div class="zone-cfg-field" style="margin-bottom:0">
        <label>Date de debut du cycle</label>
        <input type="date" id="zcfg-anchor" value="${anchorIso}">
        <small style="color:var(--muted)">${anchorDay ? 'Cycle actuel : '+epochDayLabel(anchorDay) : 'Aucune date de depart enregistree'}</small>
      </div>
    </div>
    <hr class="zone-cfg-sep">
    <div class="zone-cfg-row">
      <div class="zone-cfg-field">
        <label>&#9748; Seuil pluie (mm)</label>
        <input type="number" id="zcfg-thresh" min="0" max="50" step="0.5" value="${thresh}">
      </div>
      <div class="zone-cfg-field">
        <label>Fenetre (heures)</label>
        <input type="number" id="zcfg-hours" min="1" max="48" value="${hours}">
      </div>
    </div>
    <hr class="zone-cfg-sep">
    <div class="zone-cfg-field">
      <label>Notifications ntfy</label>
      <label><input type="checkbox" id="zcfg-notify-start" ${notifyStart?'checked':''}> Notifier au démarrage</label>
      <label><input type="checkbox" id="zcfg-notify-stop" ${notifyStop?'checked':''}> Notifier à l'arrêt</label>
    </div>
    <button class="btn-full" onclick="saveZoneConfig(${zoneIdx})">Enregistrer</button>
  `;
  document.getElementById('modal').classList.add('open');
}
function zcfgToggleInterval() {
  const isInterval = document.getElementById('zcfg-mode').value === '1';
  document.getElementById('zcfg-interval-row').style.display = isInterval ? 'grid' : 'none';
}
async function saveZoneConfig(zoneIdx) {
  const name   = document.getElementById('zcfg-name').value.trim();
  const mode   = parseInt(document.getElementById('zcfg-mode').value);
  const intD   = parseInt(document.getElementById('zcfg-intd')?.value)   || 2;
  const anchorIso = document.getElementById('zcfg-anchor')?.value || '';
  const anchorDay = isoToEpochDay(anchorIso);
  const thresh = parseFloat(document.getElementById('zcfg-thresh').value) || 0;
  const hours  = parseInt(document.getElementById('zcfg-hours').value)    || 24;
  const notifyStart = document.getElementById('zcfg-notify-start').checked;
  const notifyStop = document.getElementById('zcfg-notify-stop').checked;
  if (mode === 1 && !anchorDay) {
    toast('Date de debut requise', true);
    return;
  }
  const color = document.getElementById('zcfg-color')?.value || '';
  if (name) await api('/api/zoneName', { zone: zoneIdx, name, color });
  await api('/api/rain', { zone: zoneIdx, threshold: thresh, hours });
  await api('/api/zoneNotifications', { zone: zoneIdx, notifyStart, notifyStop });
  if (mode === 1) {
    await api('/api/intervalAnchor', { zone: zoneIdx, anchorDay });
    await api('/api/interval', { zone: zoneIdx, interval: intD });
  }
  await api('/api/mode', { zone: zoneIdx, mode });
  toast('Zone enregistree');
  addLog(`Zone ${zoneIdx+1} config sauvegardee`);
  closeModal();
  fetchStatus();
}
function renderPlanning() {
  const todayEsp = getTodayEspIdx();
  const nb = getNbZones();
  const nbCols = 7;
  const colDays  = Array.from({length:nbCols}, (_,i) => (todayEsp+i)%7);
  const grid     = document.getElementById('planning-grid');
  grid.style.gridTemplateColumns = `72px repeat(${nbCols}, 1fr)`;
  let html       = '';
  html += `<div></div>`;
  colDays.forEach((esp,col) => {
    const dayFull  = DAYS_FULL[esp];
    const dayShort = DAYS_ESP[esp];
    const marker = col===0 ? ' <<' : '';
    const label  = (nb > 8 || window.innerWidth < 700)
      ? dayShort + marker
      : dayFull  + marker;
    html += `<div class="pg-header ${col===0?'today':''}">${label}</div>`;
  });
  const forecast = status.forecast || [];
  const rainBlockedDays = computeRainBlockedDays(colDays, forecast);
  html += `<div></div>`;
  colDays.forEach((esp,col) => {
    const fd = forecast[col];
    if (fd && fd.valid) {
      const rain = parseFloat(fd.rainMm ?? fd.rain)||0;
      const tmax = parseFloat(fd.tempMax), tmin = parseFloat(fd.tempMin);
      const wind = parseFloat(fd.windMaxKmh), windDeg = parseFloat(fd.windDeg);
      const icon = rain>1.0?'&#127783;':'&#9728;';
      const tip  = weatherTooltipHtml(fd, DAYS_FULL[esp], rainBlockedDays[col]);
      const visual = !!(displayConfig && displayConfig.weatherVisualsEnabled);
      const rainPct = visual ? Math.max(0, Math.min(100, rain / 20 * 100)) : 0;
      const windInfo = visual && !isNaN(wind) && wind>0
        ? `<div class="wx-wind"><span class="wx-wind-arrow" style="transform:rotate(${isNaN(windDeg)?0:windDeg}deg)">&#8593;</span>${isNaN(windDeg)?'':weatherWindCardinal(windDeg)+' '}${wind.toFixed(0)}km/h</div>` : '';
      html += `<div class="pg-weather wx-tooltip-host ${col===0?'today-wx':''}">
        <div class="wx-content">
          <div class="wx-icon">${icon}</div>
          ${visual && !isNaN(tmin)?`<span class="wx-temp-pill ${weatherTempClass(tmin)}">${tmin.toFixed(0)}°</span>`:''}
          ${!isNaN(tmax)?`<span class="wx-temp-pill ${weatherTempClass(tmax)}">${tmax.toFixed(0)}°</span>`:''}
          ${windInfo}
          ${rain>0?`<div class="wx-rain">${rain.toFixed(1)}mm</div>`:""}
        </div>
        ${visual?`<div class="wx-rain-gauge" title="Pluie prévue : ${rain.toFixed(1)} mm"><span style="height:${rainPct.toFixed(0)}%"></span></div>`:''}
        ${tip}
      </div>`;
    } else {
      html += `<div class="pg-weather" style="opacity:.25"><div class="wx-icon">--</div></div>`;
    }
  });
  status.zones.forEach((z,zi) => {
    const name = z.name || `Zone ${zi+1}`;
    const rainThresh = z.rain?.threshMm ?? z.rainThresh ?? 2;
    const zoneColor = ZONE_COLORS[zi];  // classe CSS de structure
    const zHex = zoneHex(z, zi);        // couleur propre a cette zone
    if (z.mode === 0) {
      html += `<div class="pg-header zone-hdr pg-zone-label-${zoneColor}" style="color:${zHex};font-size:${nb>8?'9px':'11px'}">${name}</div>`;
      colDays.forEach((espIdx,col) => {
      const slots   = (_zoneSlots[zi]?.daySlots?.[espIdx]) || (z.daySlots && z.daySlots[espIdx]) || [];
        const enabled = slots.filter(s => s.e??s.enabled);
        const hasAny  = enabled.length > 0;
        const fd      = forecast[col];
        const rainMm  = fd ? (parseFloat(fd.rainMm)||0) : 0;
        const rainBlk = hasAny && rainMm >= rainThresh;
        const cls     = !hasAny ? 'off' : rainBlk ? 'rain' : 'on';
        const bgStyle = (hasAny && !rainBlk && zHex) ? ` style="background:${zHex}14"` : '';
        html += `<div class="pg-day ${cls} ${col===0?'today-col':''}"${bgStyle}
                      onclick="openDayModal(${zi},${espIdx})">
          ${hasAny
            ? enabled.map(s=>`<div class="mini-slot">
                ${pad(s.h??s.hour)}:${pad(s.m??s.minute)} ${s.d??s.duration}'</div>`).join('')
            : `<div class="pg-cross">--</div>`}
        </div>`;
      });
    } else {
      const enabled    = (_zoneSlots[zi]?.intervalSlots || z.intervalSlots || []).filter(s=>s.e??s.enabled);
      const intervalD  = z.intervalDays||z.interval||2;
      const anchorDay  = z.intervalAnchorDay||0;
      const todayEpoch = todayEpochDay();
      let nextEpoch = anchorDay;
      if (nextEpoch > 0) while (nextEpoch < todayEpoch) nextEpoch += intervalD;
      const triggerCols = new Set();
      let d = nextEpoch;
      const maxIter = Math.ceil(nbCols / Math.max(1, intervalD)) + 2;
      for (let k = 0; anchorDay > 0 && k < maxIter; k++, d += intervalD) {
        const offset = d - todayEpoch;
        if (offset < 0) continue;
        if (offset >= nbCols) break;
        triggerCols.add(offset);
      }
      html += `<div class="pg-header zone-hdr interval-zone-hdr">
        <span class="pg-zone-label-${zoneColor}" style="color:${zHex}">${name}</span>
        <span style="font-size:9px;color:var(--muted);margin-left:4px">/${intervalD}j</span>
      </div>`;
      colDays.forEach((espIdx, col) => {
        const isTrigger = triggerCols.has(col);
        const fd        = forecast[col] || {};
        const rainMm    = parseFloat(fd.rainMm)||0;
        const rainBlk   = isTrigger && rainMm >= rainThresh;
        const cls       = isTrigger ? (rainBlk?'rain':'interval-on') : 'interval-off';
        const inner     = isTrigger
          ? (enabled.length
              ? enabled.map(s=>`<div class="mini-slot">${pad(s.h??s.hour)}:${pad(s.m??s.minute)} ${s.d??s.duration}'</div>`).join('')
              : `<span class="pg-zone-label-${zoneColor}" style="color:${zHex};font-size:11px">&#8635; /${intervalD}j</span>`)
          : `<div class="pg-cross">--</div>`;
        const bgStyle   = (isTrigger && !rainBlk && zHex) ? ` style="background:${zHex}14"` : '';
        html += `<div class="pg-day ${cls} ${col===0?'today-col':''}"${bgStyle}
                      onclick="openIntervalModal(${zi},${todayEpoch}+${col})" title="/${intervalD}j - cliquer pour consulter ou proposer cette date comme nouveau depart">
          ${inner}
        </div>`;
      });
    }
  });
  grid.innerHTML = html;
  bindWeatherTooltips();
}
function bindWeatherTooltips() {
  document.querySelectorAll('.wx-tooltip-host').forEach(host => {
    const tip = host.querySelector('.wx-tooltip');
    if (!tip) return;
    const place = () => {
      const r = host.getBoundingClientRect();
      const margin = 8;
      tip.style.display = 'block';
      const tw = tip.offsetWidth;
      const th = tip.offsetHeight;
      let left = r.left + r.width / 2 - tw / 2;
      left = Math.max(margin, Math.min(left, window.innerWidth - tw - margin));
      let top = r.bottom + margin;
      if (top + th > window.innerHeight - margin) top = r.top - th - margin;
      top = Math.max(margin, Math.min(top, window.innerHeight - th - margin));
      tip.style.left = `${Math.round(left)}px`;
      tip.style.top  = `${Math.round(top)}px`;
    };
    host.addEventListener('mouseenter', place);
    host.addEventListener('mousemove', place);
    host.addEventListener('mouseleave', () => { tip.style.display = ''; });
    // renderPlanning() reconstruit toute la grille toutes les 8s (fetchStatus) :
    // les nouveaux noeuds ne recoivent ni mouseenter ni mousemove tant que le
    // curseur/doigt ne bouge pas vraiment, meme si :hover matche deja via CSS
    // (le pointeur est toujours au-dessus du meme point ecran). Sans ce
    // repositionnement immediat, la bulle reste affichee (CSS) mais a sa
    // position par defaut (haut-gauche) jusqu'au prochain mouvement.
    if (host.matches(':hover')) place();
  });
}
function escapeHtml(s) {
  return String(s ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}
function weatherTempClass(temp) {
  if (temp < 5) return 'wx-temp-freezing';
  if (temp < 12) return 'wx-temp-cold';
  if (temp < 20) return 'wx-temp-mild';
  if (temp < 27) return 'wx-temp-warm';
  return 'wx-temp-hot';
}
function weatherWindCardinal(deg) {
  return ['N','NE','E','SE','S','SO','O','NO'][Math.round((((deg%360)+360)%360)/45)%8];
}
function computeRainBlockedDays(colDays, forecast) {
  const nb = getNbZones();
  const todayEpoch = todayEpochDay();
  return colDays.map((espIdx, col) => {
    const fd = forecast[col];
    if (!fd || !fd.valid) return false;
    const rainMm = parseFloat(fd.rainMm) || 0;
    for (let zi = 0; zi < nb; zi++) {
      const z = status?.zones?.[zi];
      if (!z) continue;
      const rainThresh = z.rain?.threshMm ?? z.rainThresh ?? 2;
      if (rainMm < rainThresh) continue;
      if (z.mode === 0) {
        const slots = (_zoneSlots[zi]?.daySlots?.[espIdx]) || (z.daySlots && z.daySlots[espIdx]) || [];
        if (slots.some(s => s.e ?? s.enabled)) return true;
      } else {
        const intervalD = z.intervalDays || z.interval || 2;
        const anchorDay = z.intervalAnchorDay || 0;
        if (anchorDay > 0) {
          const targetDay = todayEpoch + col;
          if (targetDay >= anchorDay && (targetDay - anchorDay) % intervalD === 0) {
            const slots = (_zoneSlots[zi]?.intervalSlots || z.intervalSlots || []);
            if (slots.some(s => s.e ?? s.enabled)) return true;
          }
        }
      }
    }
    return false;
  });
}
function weatherTooltipHtml(fd, dayLabel, suspended) {
  const cfg = Object.assign({}, DISP_DEFAULTS, displayConfig || {});
  const lines = [];
  const n = (v, digits=0) => Number.isFinite(Number(v)) ? Number(v).toFixed(digits) : null;
  if (suspended) lines.push(`<strong style="color:var(--amber)">&#9888; Arrosage suspendu (pluie)</strong>`);
  const desc = String(fd.description || '').trim();
  if (cfg.weatherTipCondition && desc) lines.push(`<strong>${escapeHtml(desc.charAt(0).toUpperCase()+desc.slice(1))}</strong>`);
  if (cfg.weatherTipTemp) {
    const tmin=n(fd.tempMin), tmax=n(fd.tempMax), feels=n(fd.feelsLikeMax);
    if (tmin!==null && tmax!==null) lines.push(`Température : ${tmin} à ${tmax} °C`);
    if (feels!==null && Number(feels) > -90) lines.push(`Ressenti max. : ${feels} °C`);
  }
  if (cfg.weatherTipRain) {
    const rain=n(fd.rainMm,1); if (rain!==null) lines.push(`Pluie prévue : ${rain} mm`);
  }
  if (cfg.weatherTipPop) {
    const pop=n(fd.rainProbability); if (pop!==null) lines.push(`Probabilité de pluie : ${pop} %`);
  }
  if (cfg.weatherTipHumidity) {
    const hum=n(fd.humidityMax); if (hum!==null) lines.push(`Humidité max. : ${hum} %`);
  }
  if (cfg.weatherTipWind) {
    const wind=n(fd.windMaxKmh); if (wind!==null) lines.push(`Vent max. : ${wind} km/h`);
  }
  if (cfg.weatherTipGust) {
    const gust=n(fd.gustMaxKmh); if (gust!==null && Number(gust)>0) lines.push(`Rafales max. : ${gust} km/h`);
  }
  if (cfg.weatherTipClouds) {
    const clouds=n(fd.cloudsMax); if (clouds!==null) lines.push(`Couverture nuageuse : ${clouds} %`);
  }
  if (cfg.weatherTipPressure) {
    const pressure=n(fd.pressureAvg); if (pressure!==null && Number(pressure)>0) lines.push(`Pression moyenne : ${pressure} hPa`);
  }
  if (!lines.length) return '';
  return `<div class="wx-tooltip"><div class="wx-tooltip-title">${escapeHtml(dayLabel)}</div>${lines.map(x=>`<div>${x}</div>`).join('')}</div>`;
}
function pad(n) { return String(n||0).padStart(2,'0'); }
async function openDayModal(zone, espDayIdx) {
  modalZone = zone; modalDay = espDayIdx; modalIsInterval = false;
  const name = status.zones[zone]?.name || ('Zone '+(zone+1));
  const dayName = DAYS_FULL[espDayIdx] || DAYS_ESP[espDayIdx];
  document.getElementById('modal-title-text').textContent = `${name} -- ${dayName}`;
  document.getElementById('modal-body').innerHTML =
    '<div style="text-align:center;padding:20px;color:var(--muted)">Chargement...</div>';
  document.getElementById('modal').classList.add('open');
  try {
    const r = await fetch(`/api/zone?z=${zone}`);
    const data = await r.json();
    _zoneSlots[zone] = data;
    document.getElementById('modal-body').innerHTML =
      buildSlotsHTML('ds', data.daySlots?.[espDayIdx] || []);
  } catch(e) {
    document.getElementById('modal-body').innerHTML =
      '<div style="color:var(--red);padding:12px">Erreur de chargement</div>';
  }
}
async function openIntervalModal(zone, proposedDay=0) {
  const z=status.zones[zone], current=z?.intervalAnchorDay||0;
  if(proposedDay&&proposedDay!==current&&confirm(`Definir ${epochDayLabel(proposedDay)} comme nouvelle date de depart ?
Le cycle sera recalcule a partir de cette date.`)){
    const r=await api('/api/intervalAnchor',{zone,anchorDay:proposedDay});
    if(!r.ok){toast('Erreur modification date',true);return}
    toast('Date de depart modifiee');fetchStatus();return;
  }
  modalZone=zone;modalDay=-1;modalIsInterval=true;
  const name=z?.name||('Zone '+(zone+1));
  document.getElementById('modal-title-text').textContent=`${name} -- Intervalle (/${z?.intervalDays||2}j)`;
  document.getElementById('modal-body').innerHTML='<div style="text-align:center;padding:20px;color:var(--muted)">Chargement...</div>';
  document.getElementById('modal').classList.add('open');
  try{
    const r=await fetch(`/api/zone?z=${zone}`),data=await r.json();_zoneSlots[zone]=data;
    document.getElementById('modal-body').innerHTML=`<div class="zone-cfg-field"><label>Date de debut actuelle</label><input type="date" id="interval-anchor-date" value="${epochDayToIso(current||todayEpochDay())}"><small style="color:var(--muted)">${epochDayLabel(current)}</small><button class="btn-full" style="margin-top:8px" onclick="saveIntervalAnchorInput(${zone})">Enregistrer cette date</button></div>`+buildSlotsHTML('is',data.intervalSlots||[])+`<button class="btn-full" style="margin-top:12px;border-color:var(--red);color:var(--red)" onclick="deleteIntervalProgramming(${zone})">Supprimer la programmation intervalle</button>`;
  }catch(e){document.getElementById('modal-body').innerHTML='<div style="color:var(--red);padding:12px">Erreur de chargement</div>'}
}
async function saveIntervalAnchorInput(zone){
  const anchorDay=isoToEpochDay(document.getElementById('interval-anchor-date')?.value||'');
  if(!anchorDay||!confirm(`Definir ${epochDayLabel(anchorDay)} comme nouvelle date de depart ?`))return;
  const r=await api('/api/intervalAnchor',{zone,anchorDay});
  if(!r.ok){toast('Erreur modification date',true);return}
  toast('Date de depart modifiee');closeModal();fetchStatus();
}
async function deleteIntervalProgramming(zone){
  if(!confirm('Supprimer completement la programmation intervalle ?\nHoraires, date de depart et cycle seront effaces.'))return;
  const r=await api('/api/deleteInterval',{zone});
  if(!r.ok){toast('Erreur suppression',true);return}
  _zoneSlots[zone]=null;toast('Programmation intervalle supprimee');closeModal();fetchStatus();
}
function buildSlotsHTML(prefix, slots) {
  let html = `<div class="slot-header">
    <span></span>
    <span colspan="2" style="grid-column:span 2;text-align:center;font-size:9px">
      -- Heure de debut --
    </span>
    <span>Duree</span>
    <span>#</span>
  </div>
  <div class="slot-header" style="margin-top:-4px">
    <span>Actif</span><span>h</span><span>min</span><span>min</span><span></span>
  </div>`;
  slots.forEach((s,si) => {
    const h  = s.h  ?? s.hour;
    const m  = s.m  ?? s.minute;
    const d  = s.d  ?? s.duration;
    const en = s.e  ?? s.enabled;
    html += `<div class="slot-row">
      <button class="slot-toggle ${en?'on':''}" id="${prefix}-t-${si}"
              title="${en?'Cliquer pour desactiver ce slot':'Cliquer pour activer ce slot'}"
              onclick="toggleSlot('${prefix}',${si})">${en?'&#10003;':'&#9675;'}</button>
      <input class="slot-input" type="number" id="${prefix}-h-${si}"
             min="0" max="23" value="${h}" ${!en?'disabled':''}
             title="Heure de debut (0-23)">
      <input class="slot-input" type="number" id="${prefix}-m-${si}"
             min="0" max="59" value="${m}" ${!en?'disabled':''}
             title="Minute de debut (0-59)">
      <input class="slot-input" type="number" id="${prefix}-d-${si}"
             min="1" max="120" value="${d}" ${!en?'disabled':''}
             title="Duree en minutes">
      <span class="slot-num">${si+1}</span>
    </div>`;
  });
  html += `<button class="btn-full" onclick="saveSlots()">Enregistrer</button>`;
  return html;
}
function toggleSlot(prefix, si) {
  const btn  = document.getElementById(`${prefix}-t-${si}`);
  const isOn = btn.classList.toggle('on');
  btn.textContent = isOn ? 'OK' : '?';
  ['h','m','d'].forEach(f =>
    document.getElementById(`${prefix}-${f}-${si}`).disabled = !isOn);
}
async function saveSlots() {
  const prefix   = modalIsInterval ? 'is' : 'ds';
  const endpoint = modalIsInterval ? '/api/intervalslot' : '/api/dayslot';
  const reqs = [];
  for (let si = 0; si < 5; si++) {
    const enabled = document.getElementById(`${prefix}-t-${si}`).classList.contains('on');
    const body = {
      zone: modalZone, slotIdx: si,
      hour:     parseInt(document.getElementById(`${prefix}-h-${si}`).value),
      minute:   parseInt(document.getElementById(`${prefix}-m-${si}`).value),
      duration: parseInt(document.getElementById(`${prefix}-d-${si}`).value),
      enabled
    };
    if (!modalIsInterval) body.day = modalDay;
    reqs.push(api(endpoint, body));
  }
  await Promise.all(reqs);
  toast('Sauvegarde');
  addLog(`Zone ${modalZone+1} -- ${modalIsInterval ? 'intervalle' : DAYS_ESP[modalDay]} sauvegarde`);
  _zoneSlots[modalZone] = null;
  closeModal();
  await loadZoneSlots(modalZone);
  fetchStatus();
}
function closeModalOutside(e) { if (e.target===document.getElementById('modal')) closeModal(); }
function closeModal() { document.getElementById('modal').classList.remove('open'); }
async function toggleManual(zone, state) {
  await api('/api/manual', {zone, state});
  addLog(`Zone ${zone+1} -- ${state?'arrosage demarre':'arret'}`);
  fetchStatus();
}
function openDrawer() {
  document.getElementById('drawer').classList.add('open');
  document.getElementById('drawer-overlay').classList.add('open');
  // Toujours rouvrir sur la liste des rubriques, jamais sur la derniere
  // consultee : on ne se souvient pas d'ou l'on etait, et retomber au milieu
  // d'un ecran de reglages est desorientant.
  buildCfgMenu();
  backToCfgMenu();
  fetchAdminStatus();
  fetchDisplayConfig();  // pre-remplit la section Affichage LCD a chaque ouverture
  fetchNotificationConfig();  // pre-remplit la section Notifications ntfy
}
function closeDrawer() {
  document.getElementById('drawer').classList.remove('open');
  document.getElementById('drawer-overlay').classList.remove('open');
}
// ── Parametres a deux niveaux ────────────────────────────────────
//
// Niveau 1 : la liste des rubriques. Niveau 2 : une rubrique, seule a
// l'ecran. Auparavant les neuf sections etaient empilees et depliables,
// ce qui obligeait a chercher un reglage en parcourant toute la page.
//
// Le regroupement se fait ICI et non dans index.html : une rubrique
// designe une ou plusieurs sections existantes, qui ne bougent pas. On
// range l'interface sans deplacer un seul formulaire - donc sans risquer
// d'en casser un.
//
// Sept rubriques plutot que neuf : "Infobulles meteo" et "Meteo" traitent
// du meme sujet, "WiFi" et "NTP" relevent tous deux du reseau. Les avoir
// separees etait un reliquat de l'empilement, ou l'ordre tenait lieu de
// classement.
const CFG_GROUPS = [
  { id: 'g-zones',   icon: '&#128167;', label: 'Zones',
    sections: ['sec-zones'] },
  { id: 'g-meteo',   icon: '&#127780;', label: 'Météo',
    sections: ['sec-owm', 'sec-weather-tooltips'] },
  { id: 'g-reseau',  icon: '&#128225;', label: 'Réseau',
    sections: ['sec-wifi', 'sec-ntp'] },
  { id: 'g-ecran',   icon: '&#128421;', label: 'Affichage',
    sections: ['sec-display'] },
  { id: 'g-notif',   icon: '&#128276;', label: 'Notifications',
    sections: ['sec-notifications'] },
  { id: 'g-serveur', icon: '&#9729;',   label: 'Serveur',
    sections: ['sec-cloud'] },
  { id: 'g-maj',     icon: '&#11014;',  label: 'Mises à jour',
    sections: ['sec-upd'] },
  { id: 'g-systeme', icon: '&#9881;',   label: 'Système',
    sections: ['sec-system'] },
  { id: 'g-io',      icon: '&#128268;', label: 'Entrées / Sorties',
    sections: ['sec-io'] },
  { id: 'g-relais',  icon: '&#9889;',   label: 'Câblage relais',
    sections: ['sec-topo'] },
];

// Sections presentes dans le DOM mais rattachees a aucune rubrique. Elles
// resteraient inaccessibles sans cette recuperation : une section ajoutee
// a index.html sans etre declaree ci-dessus doit rester joignable, pas
// disparaitre en silence.
function cfgOrphanSections() {
  const claimed = new Set(CFG_GROUPS.flatMap(g => g.sections));
  return Array.from(document.querySelectorAll('#drawer .cfg-section'))
              .map(s => s.id)
              .filter(id => id && !claimed.has(id));
}

function buildCfgMenu() {
  const menu = document.getElementById('cfg-menu');
  if (!menu) return;

  const groups = CFG_GROUPS.filter(g =>
    g.sections.some(id => document.getElementById(id)));

  const orphans = cfgOrphanSections().map(id => {
    const sec = document.getElementById(id);
    const t = sec.querySelector('.cfg-section-title');
    return { id: 'g-' + id, icon: '', label: t ? t.textContent.trim() : id,
             sections: [id] };
  });

  menu.innerHTML = groups.concat(orphans).map(g => `
    <button class="cfg-menu-item" onclick="openCfgPage('${g.id}')">
      <span class="cfg-menu-label"><span class="sec-icon">${g.icon}</span>${g.label}</span>
      <span class="cfg-menu-chevron">&#8250;</span>
    </button>`).join('');
  _cfgGroupIndex = {};
  groups.concat(orphans).forEach(g => { _cfgGroupIndex[g.id] = g; });
}
let _cfgGroupIndex = {};

function openCfgPage(groupId) {
  const g = _cfgGroupIndex[groupId];
  if (!g) return;
  if (g.sections.indexOf('sec-zones') >= 0) buildCfgZoneList();
  if (g.sections.indexOf('sec-cloud') >= 0) populateCloudSync();
  if (g.sections.indexOf('sec-upd')   >= 0) refreshUpdateState();
  if (g.sections.indexOf('sec-io')    >= 0) loadCfgIo();
  if (g.sections.indexOf('sec-topo')  >= 0) loadCfgTopo();
  if (g.sections.indexOf('sec-zones') >= 0) refreshWiringState();

  const drawer = document.getElementById('drawer');
  drawer.classList.add('cfg-detail');
  // Une rubrique qui rassemble PLUSIEURS sections garde leurs en-tetes :
  // ils servent alors de sous-titres et separent les sujets. Seule dans sa
  // rubrique, une section verrait son en-tete repeter le titre du bandeau.
  drawer.classList.toggle('cfg-multi', g.sections.length > 1);

  document.querySelectorAll('#drawer .cfg-section').forEach(sec => {
    const inGroup = g.sections.indexOf(sec.id) >= 0;
    sec.classList.toggle('cfg-current', inGroup);
    // Corps toujours deplie sur sa propre page : le pliage n'avait de sens
    // que lorsque les neuf sections partageaient le meme ecran.
    sec.classList.toggle('open', inGroup);
  });

  document.getElementById('cfg-title').innerHTML = g.label;
  drawer.scrollTop = 0;
}

function backToCfgMenu() {
  const drawer = document.getElementById('drawer');
  drawer.classList.remove('cfg-detail', 'cfg-multi');
  document.querySelectorAll('#drawer .cfg-section').forEach(s => {
    s.classList.remove('cfg-current', 'open');
  });
  document.getElementById('cfg-title').textContent = 'Paramètres';
  drawer.scrollTop = 0;
}

// Conservee : d'anciens appels inline pointent encore dessus, et un clic
// sur l'en-tete d'une rubrique ouverte ne doit pas la replier - elle est
// seule a l'ecran, la replier ne montrerait plus rien.
function toggleSection(id) {
  const drawer = document.getElementById('drawer');
  if (drawer && drawer.classList.contains('cfg-detail')) return;
  document.getElementById(id).classList.toggle('open');
}

// Synchronisation serveur : remplissage du formulaire depuis adminStatus.
//
// Le jeton n'est jamais renvoye en clair par le module - seul un masque
// l'est. Le champ reste donc vide, et un envoi sans jeton laisse celui
// enregistre intact (la route applique la valeur courante par defaut).
function populateCloudSync() {
  if (!adminStatus || !adminStatus.cloudSync) return;
  const c = adminStatus.cloudSync;
  const set = (id, v) => { const el = document.getElementById(id); if (el) el.value = v; };
  const chk = (id, v) => { const el = document.getElementById(id); if (el) el.checked = !!v; };
  chk('cs-enabled', c.enabled);
  set('cs-host', c.host || '');
  set('cs-port', c.port || 443);
  set('cs-interval', c.intervalMinutes || 15);
  chk('cs-https', c.useHttps);
  set('cs-module-id', c.moduleId || '');
  const masked = document.getElementById('cs-token-masked');
  if (masked) masked.textContent = c.tokenMasked || '(aucun)';
}

function saveCloudSync() {
  const val = id => (document.getElementById(id) || {}).value || '';
  const on  = id => !!(document.getElementById(id) || {}).checked;

  const body = {
    enabled: on('cs-enabled'),
    host: val('cs-host').trim(),
    port: parseInt(val('cs-port'), 10) || 443,
    useHttps: on('cs-https'),
    moduleId: val('cs-module-id').trim(),
    intervalMinutes: parseInt(val('cs-interval'), 10) || 15
  };
  // Jeton envoye UNIQUEMENT s'il a ete saisi : le champ est vide par
  // defaut, et transmettre cette chaine vide effacerait le jeton
  // enregistre.
  const tok = val('cs-token').trim();
  if (tok) body.token = tok;

  fetch('/api/cloudSync', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  }).then(r => r.json().then(j => ({ ok: r.ok, j })))
    .then(({ ok, j }) => {
      if (!ok) throw new Error(j.error || 'refus du module');
      toast('Serveur enregistré');
      document.getElementById('cs-token').value = '';
      fetchAdminStatus();
    })
    .catch(e => toast('Enregistrement impossible : ' + e.message, true));
}

// ── Etat des mises a jour ────────────────────────────────────────────
//
// Deux canaux independants, deux versions distinctes : le programme du
// module (firmware) et les pages Web. Les confondre est la premiere
// source de confusion sur ce sujet — d'ou deux lignes separees, jamais
// un "AquaLook 5.9.x" unique qui ne voudrait rien dire.
//
// La disponibilite vient du dernier resultat de verification conserve
// par le module. Tant qu'aucune verification n'a tourne, on dit
// "jamais verifie" plutot que "a jour" : ne rien savoir n'est pas une
// bonne nouvelle, et l'afficher comme telle serait mentir.
async function refreshUpdateState() {
  const el = document.getElementById('upd-state');
  if (!el) return;

  const line = (titre, installe, dispo, etat, cls) =>
    `<div class="upd-chan">
       <div class="upd-chan-head"><span class="upd-chan-name">${titre}</span>
         <span class="upd-chip ${cls}">${etat}</span></div>
       <div class="upd-chan-vers">
         <span>installé <b>${escapeHtml(installe || '?')}</b></span>
         ${dispo ? `<span class="upd-arrow">→</span><span>disponible <b>${escapeHtml(dispo)}</b></span>` : ''}
       </div>
     </div>`;

  let res = null, web = null;
  try { res = await (await fetch('/api/maintenance/last-result', {cache:'no-store'})).json(); } catch (e) {}
  try { web = await (await fetch('/assets-version.json', {cache:'no-store'})).json(); } catch (e) {}

  const checked = res && res.valid && res.command === 'check_version';

  // Firmware
  let fw;
  if (!checked) {
    fw = line('Programme du module', res && res.installedVersion, null, 'jamais vérifié', 'unknown');
  } else if (res.updateAvailable) {
    fw = line('Programme du module', res.installedVersion, res.availableVersion, 'à installer', 'avail');
  } else {
    fw = line('Programme du module', res.installedVersion, null, 'à jour', 'ok');
  }

  // Pages Web : la version installee vient du fichier depose sur la carte
  // SD, pas du resultat de verification — c'est la seule source qui dise
  // ce qui est REELLEMENT servi en ce moment.
  const webInst = web ? (web.version || web.gitSha) : null;
  let wa;
  if (!checked) {
    wa = line('Pages Web', webInst, null, 'jamais vérifié', 'unknown');
  } else if (res.webAssetsUpdateAvailable) {
    wa = line('Pages Web', webInst || res.webAssetsInstalledVersion,
              res.webAssetsAvailableVersion, 'à installer', 'avail');
  } else {
    wa = line('Pages Web', webInst || res.webAssetsInstalledVersion, null, 'à jour', 'ok');
  }

  el.innerHTML = fw + wa;
}

function buildCfgZoneList() {
  const el = document.getElementById('cfg-zone-list');
  if (!el) return;
  const zones = (status && status.zones) ? status.zones : [];
  if (!zones.length) {
    el.innerHTML = '<small style="color:var(--muted)">Zones non chargées</small>';
    return;
  }
  el.innerHTML = zones.map((z, i) => {
    const name = z.name || `Zone ${i + 1}`;
    const mode = z.mode === 1
      ? `Intervalle / ${z.intervalDays || z.interval || 2}j`
      : 'Jours fixes';
    // Rappeler la reference : une zone renommee "Potager distant" ne dit plus
    // a quelle sortie elle correspond, et l'utilisateur s'y perd des que les
    // noms ne suivent plus l'ordre.
    return `<button class="cfg-zone-link" onclick="openZoneConfigModal(${i})">
              <span class="zone-dot" style="background:${zoneHex(z, i)}"></span>
              <span class="cfg-zone-ref">Z${i + 1}</span>
              <span class="cfg-zone-link-name">${name}</span>
              <span style="font-family:var(--mono);font-size:10px;color:var(--muted)">${mode}</span>
              <span class="cfg-menu-chevron">&#8250;</span>
            </button>`;
  }).join('');
}

function populateDrawer() {
  if (!adminStatus) return;
  const s = adminStatus;
  if (s.system) {
    const zonesEl = document.getElementById('cfg-nb-zones');
    if (zonesEl) zonesEl.dataset.current = s.system.nbZones ?? 2;
  }
  // header-title (ligne "AQUALOOK") et header-city (sous-titre, voir
  // .logo-city dans index.html) forment deja un affichage sur 2 lignes :
  // pas besoin d'agglomerer aussi la ville dans le titre, ca la doublait.
  const city = s.owm?.city || '';
  document.getElementById('header-city').textContent = city;
  const ssid = s.wifi?.ssid || '--';
  document.getElementById('wifi-info').innerHTML =
    `SSID : <span>${ssid}</span><br>
     IP : <span>${s.wifi?.ip||'--'}</span><br>
     ?tat : <span>${s.wifi?.state||'--'}</span>`;
  const keepaliveHost = s.wifi?.keepaliveHost || '';
  document.getElementById('wifi-keepalive-info').innerHTML =
    `Cible keepalive : <span>${keepaliveHost || '(desactivee)'}</span>`;
  const keepaliveEl = document.getElementById('cfg-keepalive');
  if (keepaliveEl && document.activeElement !== keepaliveEl) {
    keepaliveEl.value = keepaliveHost;
  }
  if (s.ntp) {
    document.getElementById('cfg-ntp-server').value = s.ntp.server || 'pool.ntp.org';
    document.getElementById('cfg-ntp-gmt').value    = s.ntp.gmtOffset ?? 3600;
    document.getElementById('cfg-ntp-dst').value    = s.ntp.dstOffset ?? 3600;
  }
  if (s.bootGuard) {
    const b = document.getElementById('bootguard-banner');
    if (b) b.style.display = s.bootGuard.degraded ? '' : 'none';
  }
  if (s.updateCheck) {
    const u = s.updateCheck;
    const en = document.getElementById('cfg-upd-enabled');
    const tm = document.getElementById('cfg-upd-time');
    const dy = document.getElementById('cfg-upd-days');
    if (en && document.activeElement !== en) en.checked = !!u.enabled;
    if (tm && document.activeElement !== tm) {
      tm.value = String(u.hour ?? 3).padStart(2,'0') + ':' + String(u.minute ?? 30).padStart(2,'0');
    }
    if (dy && document.activeElement !== dy) dy.value = u.intervalDays ?? 1;
    const last = document.getElementById('cfg-upd-last');
    if (last) {
      // lastCheckEpochDay vaut 0 tant qu'aucune echeance n'a ete posee.
      last.textContent = u.lastCheckEpochDay
        ? 'Derniere verification : ' + new Date(u.lastCheckEpochDay*86400000).toLocaleDateString('fr-FR')
        : 'Aucune verification effectuee pour le moment.';
    }
  }
  if (s.owm) {
    document.getElementById('cfg-owm-provider').value = String(s.owm.provider ?? 0);
    toggleOwmProvider();
    document.getElementById('cfg-owm-units').value = s.owm.units || 'metric';
    const hasCity = s.owm.city && s.owm.city.length > 0;
    const mode = hasCity ? 'city' : 'gps';
    document.getElementById('cfg-owm-mode').value = mode;
    if (hasCity) {
      document.getElementById('cfg-owm-city').value    = s.owm.city || '';
      document.getElementById('cfg-owm-country').value = s.owm.country || 'FR';
    } else {
      document.getElementById('cfg-owm-lat').value = s.owm.lat ?? '';
      document.getElementById('cfg-owm-lon').value = s.owm.lon ?? '';
    }
    toggleOwmMode();
    const keyEl = document.getElementById('cfg-owm-key');
    if (s.owm?.hasKey) {
      keyEl.placeholder = '(cle configuree -- laisser vide pour conserver)';
      keyEl.style.borderColor = 'var(--green)';
    } else {
      keyEl.placeholder = 'Cle API OWM';
      keyEl.style.borderColor = '';
    }
  }
  if (s.system) {
    document.getElementById('cfg-maxwater').value       = s.system.maxWateringMin ?? 60;
    document.getElementById('cfg-screen-timeout').value = s.system.screenTimeout  ?? 5;
    document.getElementById('cfg-led-mode').value       = s.system.ledMode        ?? 1;
    updateZoneOptions(s.system.nbZones ?? 2);
  }
  if (status) {
    document.getElementById('cfg-manual-dur').value = status.manualDurationMin ?? 10;
  }
  _applyZonesViewBtn();
  // Le controleur et la logique ne sont plus des reglages globaux : ils
  // appartiennent a chaque carte du cablage. Afficher ici les anciennes
  // valeurs de configuration donnerait une information sans effet reel.
  document.getElementById('sys-info').innerHTML =
    `IP : <span>${s.wifi?.ip||'--'}</span><br>
     RAM libre : <span>${s.heap||'--'} o</span><br>
     Uptime : <span>${fmtUptime(s.uptime)}</span><br>
     NTP : <span>${s.ntp?.synced ? s.ntp.time : 'Non sync'}</span><br>
     OWM : <span>${s.owm?.hasKey ? 'OK Cle configuree' : '? Pas de cle'}</span><br>
     Ville : <span>${s.owm?.city || s.owm?.lat || '--'}</span><br>
     Veille : <span>${s.system?.screenTimeout===0 ? 'Desactivee' : (s.system?.screenTimeout||5)+'min'}</span><br>
     Zones : <span>${s.system?.nbZones||2}</span><br>
     Câblage : <span id="sys-wiring">${wiringLabel()}</span>`;
  // Apres l'insertion : la fiche vient d'etre reconstruite, le span existe.
  refreshWiringState();
}
function fmtUptime(s) {
  if (!s) return '--';
  const h = Math.floor(s/3600), m = Math.floor((s%3600)/60), sec = s%60;
  return `${h}h${String(m).padStart(2,'0')}m${String(sec).padStart(2,'0')}s`;
}
async function saveCfgWifi() {
  const ssid = document.getElementById('cfg-ssid').value.trim();
  const pwd  = document.getElementById('cfg-pwd').value;
  if (!ssid) { toast('SSID requis', true); return; }
  if (!confirm(`Changer le WiFi vers "${ssid}" et redemarrer ?`)) return;
  await api('/api/wifi', {ssid, pwd});
  toast('Redemarrage en cours...');
  closeDrawer();
}
async function saveCfgWifiKeepalive() {
  const host = document.getElementById('cfg-keepalive').value.trim();
  await api('/api/wifiKeepalive', {host});
  toast(host ? 'Cible keepalive enregistree' : 'Sonde keepalive desactivee');
  fetchAdminStatus();
}
async function saveCfgNtp() {
  await api('/api/ntp', {
    server:    document.getElementById('cfg-ntp-server').value.trim() || 'pool.ntp.org',
    gmtOffset: parseInt(document.getElementById('cfg-ntp-gmt').value) || 3600,
    dstOffset: parseInt(document.getElementById('cfg-ntp-dst').value) || 3600
  });
  toast('NTP mis a jour');
}
async function clearBootGuard() {
  // Gabarit (accents graves) et non apostrophes simples : le message tient
  // volontairement sur deux paragraphes, et une chaine simple ne peut pas
  // contenir de saut de ligne brut. Ecrit ainsi le 17 aout 2026, ce
  // confirm() rendait TOUT app.js inanalysable - donc aucune fonction
  // definie, page reduite a sa coquille HTML et menu inerte
  // ("openDrawer is not defined"). Trouve le 29 aout 2026.
  if (!confirm(`Réactiver la météo, les mises à jour et les notifications ?

Si la cause du problème n’est pas résolue, AquaLook peut se remettre à redémarrer.`)) return;
  await api('/api/bootguard/clear', {});
  toast('Fonctions réactivées au prochain démarrage');
  fetchAdminStatus();
}
async function saveCfgUpdateCheck() {
  const enabled = document.getElementById('cfg-upd-enabled').checked;
  const parts   = (document.getElementById('cfg-upd-time').value || '03:30').split(':');
  const days    = parseInt(document.getElementById('cfg-upd-days').value) || 1;
  if (days < 1 || days > 30) { toast('Intervalle attendu entre 1 et 30 jours', true); return; }
  await api('/api/updateCheck', {
    enabled,
    hour:   parseInt(parts[0]) || 0,
    minute: parseInt(parts[1]) || 0,
    intervalDays: days
  });
  toast(enabled ? 'Verification automatique enregistree' : 'Verification automatique desactivee');
  fetchAdminStatus();
}
// Open-Meteo ne demande aucune clef et ne connait que des coordonnees. Masquer
// le champ de clef evite de laisser croire qu'il faut un compte ; l'annonce du
// geocodage evite de laisser croire que le nom de ville ne marche pas.
function toggleOwmProvider() {
  const p = document.getElementById('cfg-owm-provider').value;
  const openMeteo = (p === '1');
  document.getElementById('owm-key-block').style.display = openMeteo ? 'none' : '';
  document.getElementById('owm-provider-hint').innerHTML = openMeteo
    ? 'Modeles Meteo-France (AROME ~1,3 km sur la France), sans cle ni compte. '
      + 'Une ville est convertie en coordonnees au premier releve.'
    : 'Necessite une cle OpenWeatherMap. Modele global, resolution plus grossiere '
      + 'sur la France.';
}

function toggleOwmMode() {
  const mode = document.getElementById('cfg-owm-mode').value;
  document.getElementById('owm-city-block').style.display = mode==='city' ? '' : 'none';
  document.getElementById('owm-gps-block').style.display  = mode==='gps'  ? '' : 'none';
}
async function saveCfgOwm() {
  const apiKey  = document.getElementById('cfg-owm-key').value.trim();
  const units   = document.getElementById('cfg-owm-units').value;
  const mode    = document.getElementById('cfg-owm-mode').value;
  const provider = parseInt(document.getElementById('cfg-owm-provider').value, 10) || 0;
  const body    = {units, provider};
  if (apiKey) body.apiKey = apiKey;  // ne pas ecraser si vide
  if (mode === 'gps') {
    body.lat = parseFloat(document.getElementById('cfg-owm-lat').value) || 0;
    body.lon = parseFloat(document.getElementById('cfg-owm-lon').value) || 0;
    body.city    = '';
    body.country = '';
  } else {
    const city    = document.getElementById('cfg-owm-city').value.trim();
    const country = document.getElementById('cfg-owm-country').value.trim().toUpperCase();
    if (!city) { toast('Ville requise', true); return; }
    body.city    = city;
    body.country = country || 'FR';
    body.lat = 0;
    body.lon = 0;
  }
  await api('/api/owm', body);
  toast('Meteo enregistree');
}
async function saveCfgRelaySetup() {
  const nbZones = Math.min(8, parseInt(document.getElementById('cfg-nb-zones').value) || 2);
  const maxMin = Math.min(120, Math.max(1, parseInt(document.getElementById('cfg-maxwater').value) || 60));
  const manDur = Math.min(120, Math.max(1, parseInt(document.getElementById('cfg-manual-dur').value) || 10));
  const zonesEl = document.getElementById('cfg-nb-zones');
  const oldNbZones = parseInt(zonesEl.dataset.current || '2');
  const needReboot = nbZones !== oldNbZones;
  if (needReboot) {
    if (!confirm(`Passer a ${nbZones} zone${nbZones>1?'s':''} ? Le module va redémarrer.`)) return;
  }
  // Ni relayController ni relayLogic : ils appartiennent au cablage, carte
  // par carte. La route ne modifie que les champs presents, les envoyer
  // ecraserait le cablage decrit ailleurs.
  const response = await api('/api/system', {
    nbZones,
    maxWateringMin: maxMin,
    manualDurationMin: manDur
  });
  if (!response.ok) {
    toast('Erreur pendant l enregistrement', true);
    return;
  }
  if (needReboot) {
    toast('Configuration enregistrée — redémarrage...');
    closeDrawer();
  } else {
    toast('Configuration enregistrée');
    fetchAdminStatus();
  }
}
// Le nombre de zones est une notion LOGIQUE : combien de zones l'utilisateur
// veut piloter. Il etait auparavant contraint par le controleur (paires de 2
// sur XL9535) parce que le cablage en etait deduit. Le cablage etant
// desormais decrit explicitement, cette contrainte n'a plus de raison
// d'etre : c'est l'editeur de cablage qui dit quelle zone sort ou.
function updateZoneOptions(preferredValue) {
  const select = document.getElementById('cfg-nb-zones');
  if (!select) return;
  const current = Number.isFinite(Number(preferredValue))
    ? Number(preferredValue)
    : (parseInt(select.value) || parseInt(select.dataset.current) || 2);
  const values = [1,2,3,4,5,6,7,8];
  const chosen = values.indexOf(current) >= 0 ? current : 2;
  select.innerHTML = values.map(value =>
    `<option value="${value}">${value} zone${value>1?'s':''}</option>`).join('');
  select.value = String(chosen);
  const hint = document.getElementById('cfg-zones-hint');
  if (hint) hint.textContent =
    'Nombre de zones a piloter. Leur raccordement se decrit dans ' +
    '"Cablage relais".';
}

async function saveCfgSystem() {
  const timeout = parseInt(document.getElementById('cfg-screen-timeout').value) || 5;
  const ledMode = parseInt(document.getElementById('cfg-led-mode').value) || 1;
  await api('/api/system', { screenTimeout: timeout, ledMode });
  toast('Systeme enregistre');
}
async function fetchNotificationConfig() {
  try {
    const r = await fetch('/api/notifications');
    notificationConfig = r.ok ? await r.json() : null;
  } catch(e) { notificationConfig = null; }
  populateNotificationSection();
}
function populateNotificationSection() {
  const d = notificationConfig;
  const infoEl = document.getElementById('ntfy-info');
  const enabledEl = document.getElementById('cfg-ntfy-enabled');
  if (!d || !enabledEl) { if (infoEl) infoEl.textContent = 'État indisponible'; return; }
  enabledEl.checked = !!d.enabled;
  document.getElementById('cfg-ntfy-server').value = d.server || 'http://ntfy.sh';
  document.getElementById('cfg-ntfy-topic').value  = d.topic  || '';
  document.getElementById('cfg-ntfy-token').value  = '';
  infoEl.innerHTML =
    `Configuré : <span>${d.configured ? 'oui' : 'non'}</span><br>
     Jeton : <span>${d.tokenConfigured ? 'présent' : 'absent'}</span><br>
     Dernier résultat : <span>${d.lastResult || '--'}</span> — HTTP : <span>${d.lastHttpCode || '--'}</span>`;
}
function validNtfyTopic(topic) {
  return topic.length >= 8 && /^[A-Za-z0-9_-]+$/.test(topic);
}
async function saveCfgNotifications() {
  const server = document.getElementById('cfg-ntfy-server').value.trim() || 'http://ntfy.sh';
  const topic  = document.getElementById('cfg-ntfy-topic').value.trim();
  const token  = document.getElementById('cfg-ntfy-token').value;
  let enabled  = document.getElementById('cfg-ntfy-enabled').checked && topic.length > 0;
  if (!server.startsWith('http://')) { toast('Le serveur doit commencer par http://', true); return; }
  if (enabled && !validNtfyTopic(topic)) {
    toast('Sujet invalide (8 caracteres min., lettres/chiffres/-/_ uniquement)', true);
    return;
  }
  if (!topic) enabled = false;
  const response = await api('/api/notifications/config', {enabled, server, topic, token, preserveToken: true});
  if (!response.ok) { toast('Configuration refusee par le module', true); return; }
  toast('Notifications enregistrees');
  await fetchNotificationConfig();
}
async function testCfgNotifications() {
  try {
    const r = await fetch('/api/notifications/test', {method:'POST'});
    if (!r.ok) throw new Error('HTTP ' + r.status);
    toast('Test envoye');
  } catch(e) { toast('Test impossible : ' + e.message, true); }
  await fetchNotificationConfig();
}
async function resetCfgNotifications() {
  if (!confirm("Desactiver ntfy et effacer definitivement le sujet et le jeton enregistres en NVS ?")) return;
  const response = await api('/api/notifications/config',
    {enabled:false, server:'http://ntfy.sh', topic:'', token:'', preserveToken:false});
  if (!response.ok) { toast('Reinitialisation impossible', true); return; }
  toast('Notifications reinitialisees');
  await fetchNotificationConfig();
}
async function launchCaptive() {
  if (!confirm('Lancer le portail captif ? Le module passera en mode AP.')) return;
  await fetch('/api/captive', {method:'POST'});
  toast('Portail captif active');
  closeDrawer();
}
async function resetConfig() {
  if (!confirm('Reinitialiser toute la configuration et redemarrer ?')) return;
  await fetch('/api/resetConfig', {method:'POST'});
  toast('Reinitialisation...');
  closeDrawer();
}
async function api(endpoint, body) {
  return fetch(endpoint, {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify(body)
  });
}
let toastTimer;
function toast(msg, error=false) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.style.color = error ? 'var(--red)' : 'var(--green)';
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), 2500);
}
const logEntries = [];
function addLog(msg) {
  const now = new Date().toLocaleTimeString('fr-FR');
  logEntries.unshift(`[${now}] ${msg}`);
  if (logEntries.length > 20) logEntries.pop();
  const logsEl = document.getElementById('logs');
  if (logsEl) logsEl.innerHTML =
    logEntries.map(l=>`<div class="log-entry">${l}</div>`).join('');
}
function toggleActivity() {
  const card = document.getElementById('activity-card');
  const btn  = document.getElementById('btn-toggle-activity');
  const hidden = card.style.display === 'none';
  card.style.display = hidden ? '' : 'none';
  btn.textContent = hidden ? 'Masquer' : 'Afficher';
  try { localStorage.setItem('showActivity', hidden?'1':'0'); } catch(e){}
}
(function() {
  try {
    const pref = localStorage.getItem('showActivity');
    if (pref === '0') {
      const card = document.getElementById('activity-card');
      const btn  = document.getElementById('btn-toggle-activity');
      if (card) card.style.display = 'none';
      if (btn)  btn.textContent = 'Afficher';
    }
  } catch(e){}
  _applyZonesViewBtn();
})();
fetchStatus();
fetchAdminStatus();  // charge ville + config systeme au demarrage
fetchDisplayConfig(); // charge les tokens de design LCD et applique les couleurs de zone web
fetchAssetsVersion(); // pied de page : date/heure de la derniere synchro SD (voir tools/sync-sd-assets.ps1)
async function fetchAssetsVersion() {
  const el = document.getElementById('assets-version');
  if (!el) return;
  try {
    const r = await fetch('/assets-version.json', {cache:'no-store'});
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const v = await r.json();
    el.textContent = `Pages synchronisees le ${v.syncedAt}` + (v.gitSha ? ` (${v.gitSha})` : '');
  } catch(e) {
    el.textContent = '';  // ancienne synchro sans ce fichier -- rien a afficher, pas d'erreur genante
  }
}
setInterval(fetchStatus, 8000);        // 8s -- moins agressif pour l'ESP32
setInterval(fetchAdminStatus, 60000);  // 1min -- rarement necessaire
let displayConfig = null;
// cZone0..3 ne figurent plus ici : leurs champs ont quitte le menu Zones,
// la couleur etant desormais reglee zone par zone. Les valeurs restent en
// configuration et servent de repli pour une zone jamais configuree ; ne pas
// les envoyer les laisse intactes (voir copyColor cote firmware).
const DISP_COLOR_FIELDS   = ['cBg','cSurface','cSurface2','cBorder',
                              'cText','cText2','cMuted','cActiveBg'];
const DISP_NUMERIC_FIELDS = ['rSm','rMd','rLg','accentBarW',
                              'refreshNomMs','refreshActMs',
                              'planGap','g2Gpad','g4Gpad',
                              'showWeatherIcon','showWeatherTemp','weatherVisualsEnabled',
                              'weatherTipCondition','weatherTipTemp','weatherTipRain',
                              'weatherTipPop','weatherTipHumidity','weatherTipWind',
                              'weatherTipGust','weatherTipClouds','weatherTipPressure'];
const DISP_DEFAULTS = {
  cBg:'#101818', cSurface:'#182420', cSurface2:'#283028', cBorder:'#384c40',
  cText:'#f8fcf8', cText2:'#c0d0c8', cMuted:'#789c80', cActiveBg:'#382020',
  cZone0:'#00fc00', cZone1:'#0090f8', cZone2:'#f8a400', cZone3:'#780078',
  rSm:4, rMd:6, rLg:10, accentBarW:3,
  refreshNomMs:5000, refreshActMs:1000,
  planGap:6, g2Gpad:1, g4Gpad:1,
  showWeatherIcon:true, showWeatherTemp:false, weatherVisualsEnabled:false,
  weatherTipCondition:true, weatherTipTemp:true, weatherTipRain:true,
  weatherTipPop:true, weatherTipHumidity:true, weatherTipWind:true,
  weatherTipGust:true, weatherTipClouds:false, weatherTipPressure:false
};
async function fetchDisplayConfig() {
  try {
    var r = await fetch('/api/display');
    if (r.ok) {
      var data = await r.json();
      displayConfig = Object.assign({}, DISP_DEFAULTS, data);
    } else {
      displayConfig = Object.assign({}, DISP_DEFAULTS);
    }
  } catch(e) {
    displayConfig = Object.assign({}, DISP_DEFAULTS);
  }
  populateDisplaySection();
}
function populateDisplaySection() {
  if (!displayConfig) return;
  var BOOL_FIELDS = ['showWeatherIcon','showWeatherTemp','weatherVisualsEnabled','weatherTipCondition','weatherTipTemp','weatherTipRain','weatherTipPop','weatherTipHumidity','weatherTipWind','weatherTipGust','weatherTipClouds','weatherTipPressure'];
  DISP_COLOR_FIELDS.forEach(function(f) {
    var el = document.getElementById('disp-' + f);
    if (el && displayConfig[f]) el.value = displayConfig[f];
  });
  DISP_NUMERIC_FIELDS.forEach(function(f) {
    if (BOOL_FIELDS.indexOf(f) >= 0) return;  // traités séparément
    var el = document.getElementById('disp-' + f);
    if (el && displayConfig[f] !== undefined) el.value = displayConfig[f];
  });
  BOOL_FIELDS.forEach(function(f) {
    var el = document.getElementById('disp-' + f);
    if (el) el.checked = !!displayConfig[f];
  });
  applyWebZoneColors(displayConfig);
}
function applyWebZoneColors(cfg) {
  if (!cfg) return;
  var id = 'aqualook-zone-theme';
  var el = document.getElementById(id);
  if (!el) {
    el = document.createElement('style');
    el.id = id;
    document.head.appendChild(el);
  }
  // Cette feuille appliquait la palette de quatre couleurs par-dessus tout,
  // en !important. Depuis que chaque zone porte la sienne, ces regles
  // battaient le style pose sur l'element : la couleur choisie par
  // l'utilisateur n'apparaissait nulle part. Il n'y a donc plus rien a
  // generer ici -- la couleur voyage avec la zone, par --zc pour les liseres
  // et par `color` pour les libelles de planning.
  //
  // La fonction est conservee : d'autres reglages d'affichage pourront y
  // revenir, et la vider est plus sur que d'en retirer les appels.
  el.textContent = '';
}
var _dispSaveTimer = null;
function onDispColorChange() {
  var tmpCfg = Object.assign({}, displayConfig || DISP_DEFAULTS);
  DISP_COLOR_FIELDS.forEach(function(f) {
    var el = document.getElementById('disp-' + f);
    if (el) tmpCfg[f] = el.value;
  });
  applyWebZoneColors(tmpCfg);
  clearTimeout(_dispSaveTimer);
  _dispSaveTimer = setTimeout(saveCfgDisplay, 600);
}
function onDispNumericChange() {
  clearTimeout(_dispSaveTimer);
  _dispSaveTimer = setTimeout(saveCfgDisplay, 300);
}
async function saveCfgDisplay() {
  var BOOL_FIELDS = ['showWeatherIcon','showWeatherTemp','weatherVisualsEnabled','weatherTipCondition','weatherTipTemp','weatherTipRain','weatherTipPop','weatherTipHumidity','weatherTipWind','weatherTipGust','weatherTipClouds','weatherTipPressure'];
  var body = {};
  DISP_COLOR_FIELDS.forEach(function(f) {
    var el = document.getElementById('disp-' + f);
    if (el) body[f] = el.value;
  });
  DISP_NUMERIC_FIELDS.forEach(function(f) {
    if (BOOL_FIELDS.indexOf(f) >= 0) return;  // traités séparément
    var el = document.getElementById('disp-' + f);
    if (el) body[f] = parseInt(el.value, 10) || 0;
  });
  BOOL_FIELDS.forEach(function(f) {
    var el = document.getElementById('disp-' + f);
    if (el) body[f] = el.checked;
  });
  var r = await api('/api/display', body);
  if (r && r.ok) {
    toast('Affichage LCD mis a jour');
    displayConfig = body;
  } else {
    toast('Erreur sauvegarde affichage', true);
  }
}
async function resetCfgDisplay() {
  if (!confirm('Reinitialiser les valeurs par defaut de l affichage LCD ?')) return;
  var defaults = {
    cBg:'#101818', cSurface:'#182420', cSurface2:'#283028', cBorder:'#384c40',
    cText:'#f8fcf8', cText2:'#c0d0c8', cMuted:'#789c80', cActiveBg:'#382020',
    cZone0:'#00fc00', cZone1:'#0090f8', cZone2:'#f8a400', cZone3:'#780078',
    rSm:4, rMd:6, rLg:10, accentBarW:3,
    refreshNomMs:5000, refreshActMs:1000,
    planGap:6, g2Gpad:1, g4Gpad:1,
    showWeatherIcon:true, showWeatherTemp:false, weatherVisualsEnabled:false,
    weatherTipCondition:true, weatherTipTemp:true, weatherTipRain:true,
    weatherTipPop:true, weatherTipHumidity:true, weatherTipWind:true,
    weatherTipGust:true, weatherTipClouds:false, weatherTipPressure:false
  };
  var r = await api('/api/display', defaults);
  if (r && r.ok) {
    displayConfig = defaults;
    populateDisplaySection();
    toast('Valeurs par defaut restaurees');
  } else {
    toast('Erreur reinitialisation', true);
  }
}

// ── Couche E/S TOR (MCP23017) — editeur configurable ───────────────────────
// Convention : guillemets simples en JS, doubles en HTML, &rsquo; pour
// l&rsquo;apostrophe. Aucun echappement fragile.

let ioBoards = [];   // { addr }
let ioBinds  = [];   // { board, pin, dir, role, zone, active, pullup, state, missing, command }
let ioEnabled = false;
let ioPoll = 5;

const IO_ROLES_IN  = [[1, 'Presence vanne'], [2, 'Entree TOR']];
const IO_ROLES_OUT = [[3, 'Eclairage'], [4, 'Ventilation'], [5, 'Sortie TOR']];

function ioZoneCount() {
  const s = adminStatus && adminStatus.system;
  const n = s && Number(s.nbZones);
  return (n && n > 0) ? n : 8;
}

async function loadCfgIo() {
  let d;
  try { d = await (await fetch('/api/io')).json(); }
  catch (e) { document.getElementById('io-editor').innerHTML =
    '<div class="cfg-hint">Lecture impossible : ' + e.message + '</div>'; return; }
  ioEnabled = !!d.enabled;
  ioPoll = d.pollSeconds || 5;
  ioBoards = (d.boards || []).filter(b => b.enabled).map(b => ({ addr: b.addr, ready: b.ready }));
  ioBinds = (d.bindings || []).filter(b => b.enabled).map(b => ({
    board: b.board, pin: b.pin, dir: b.dir, role: b.role,
    zone: (b.zone == null ? 255 : b.zone), active: b.activeLevel,
    pullup: !!b.pullup, state: b.state, missing: b.missing, command: b.command
  }));
  document.getElementById('io-enabled').checked = ioEnabled;
  document.getElementById('io-poll').value = ioPoll;
  renderIoEditor();
}

function ioBoardOptions(sel) {
  if (!ioBoards.length) return '<option value="0">(aucune carte)</option>';
  return ioBoards.map((b, i) =>
    '<option value="' + i + '"' + (i === sel ? ' selected' : '') + '>Carte ' + i +
    ' (0x' + b.addr.toString(16) + ')</option>').join('');
}

function ioRoleOptions(dir, sel) {
  const list = (dir === 1) ? IO_ROLES_OUT : IO_ROLES_IN;
  return list.map(r => '<option value="' + r[0] + '"' +
    (r[0] === sel ? ' selected' : '') + '>' + r[1] + '</option>').join('');
}

function ioZoneOptions(sel) {
  let h = '<option value="255"' + (sel === 255 ? ' selected' : '') + '>aucune</option>';
  for (let z = 0; z < ioZoneCount(); z++)
    h += '<option value="' + z + '"' + (z === sel ? ' selected' : '') + '>Zone ' + (z + 1) + '</option>';
  return h;
}

function ioPinOptions(sel) {
  let h = '';
  for (let p = 0; p < 16; p++) {
    const lbl = (p < 8) ? ('A' + p) : ('B' + (p - 8));
    h += '<option value="' + p + '"' + (p === sel ? ' selected' : '') + '>' + lbl + '</option>';
  }
  return h;
}

function ioStateBadge(b) {
  if (b.dir === 1) return b.command ? '<span class="io-on">ON</span>' : 'OFF';
  if (b.state === 'present') return '<span class="io-ok">presente</span>';
  if (b.state === 'absent')  return '<span class="io-ko">ABSENTE</span>';
  if (b.state === 'actif')   return '<span class="io-ok">actif</span>';
  if (b.state === 'inactif') return 'inactif';
  return '<span class="cfg-muted">&mdash;</span>';
}

function renderIoEditor() {
  const el = document.getElementById('io-editor');
  let h = '<div class="cfg-subsection-title">Cartes MCP23017</div>';
  if (!ioBoards.length) h += '<div class="cfg-hint">Aucune carte declaree.</div>';
  ioBoards.forEach((b, i) => {
    h += '<div class="io-row"><span class="io-lbl">Carte ' + i + '</span>'
      + '<label>Adresse</label><select data-io="baddr" data-i="' + i + '">';
    for (let a = 0x20; a <= 0x27; a++)
      h += '<option value="' + a + '"' + (a === b.addr ? ' selected' : '') + '>0x' + a.toString(16) + '</option>';
    h += '</select><span class="io-st">' + (b.ready ? '<span class="io-ok">prete</span>' : '<span class="io-ko">absente</span>') + '</span>'
      + '<button class="io-del" onclick="ioRemoveBoard(' + i + ')">&#10007;</button></div>';
  });
  h += '<button class="btn-cfg" onclick="ioAddBoard()" style="margin:6px 0">+ Ajouter une carte</button>';

  h += '<div class="cfg-subsection-title" style="margin-top:12px">Broches</div>';
  h += '<div class="cfg-hint">Chaque broche : carte, broche, sens, role, zone, niveau actif. '
     + 'La presence de vanne n&rsquo;est lue qu&rsquo;au repos de sa zone.</div>';
  if (!ioBinds.length) h += '<div class="cfg-hint">Aucune broche declaree.</div>';
  ioBinds.forEach((b, i) => {
    h += '<div class="io-bind">'
      + '<select data-io="board" data-i="' + i + '">' + ioBoardOptions(b.board) + '</select>'
      + '<select data-io="pin" data-i="' + i + '">' + ioPinOptions(b.pin) + '</select>'
      + '<select data-io="dir" data-i="' + i + '" onchange="ioOnDirChange(' + i + ')">'
      + '<option value="0"' + (b.dir === 0 ? ' selected' : '') + '>Entree</option>'
      + '<option value="1"' + (b.dir === 1 ? ' selected' : '') + '>Sortie</option></select>'
      + '<select data-io="role" data-i="' + i + '">' + ioRoleOptions(b.dir, b.role) + '</select>'
      + '<select data-io="zone" data-i="' + i + '">' + ioZoneOptions(b.zone) + '</select>'
      + '<select data-io="active" data-i="' + i + '" title="niveau logique actif/present">'
      + '<option value="1"' + (b.active === 1 ? ' selected' : '') + '>actif=1</option>'
      + '<option value="0"' + (b.active === 0 ? ' selected' : '') + '>actif=0</option></select>'
      + '<label class="io-pu"><input type="checkbox" data-io="pullup" data-i="' + i + '"' + (b.pullup ? ' checked' : '') + '>pull-up</label>'
      + '<span class="io-st">' + ioStateBadge(b) + '</span>'
      + '<button class="io-del" onclick="ioRemoveBind(' + i + ')">&#10007;</button></div>';
  });
  h += '<button class="btn-cfg" onclick="ioAddBind()" style="margin:6px 0">+ Ajouter une broche</button>';
  el.innerHTML = h;
}

// Recopie l'etat des selects/checkbox dans les tableaux avant tout re-render.
function ioGatherFromDom() {
  document.querySelectorAll('#io-editor [data-io]').forEach(elm => {
    const k = elm.dataset.io, i = Number(elm.dataset.i);
    const val = (elm.type === 'checkbox') ? elm.checked : Number(elm.value);
    if (k === 'baddr' && ioBoards[i]) ioBoards[i].addr = val;
    else if (ioBinds[i]) {
      if (k === 'board') ioBinds[i].board = val;
      else if (k === 'pin') ioBinds[i].pin = val;
      else if (k === 'dir') ioBinds[i].dir = val;
      else if (k === 'role') ioBinds[i].role = val;
      else if (k === 'zone') ioBinds[i].zone = val;
      else if (k === 'active') ioBinds[i].active = val;
      else if (k === 'pullup') ioBinds[i].pullup = val;
    }
  });
}

function ioAddBoard() { ioGatherFromDom(); if (ioBoards.length < 8) { ioBoards.push({ addr: 0x21, ready: false }); renderIoEditor(); } }
function ioRemoveBoard(i) { ioGatherFromDom(); ioBoards.splice(i, 1); renderIoEditor(); }
function ioAddBind() { ioGatherFromDom(); if (ioBinds.length < 32) { ioBinds.push({ board: 0, pin: 0, dir: 0, role: 1, zone: 255, active: 0, pullup: true }); renderIoEditor(); } }
function ioRemoveBind(i) { ioGatherFromDom(); ioBinds.splice(i, 1); renderIoEditor(); }

// Quand le sens change, le role doit rester coherent (entree<->sortie).
function ioOnDirChange(i) {
  ioGatherFromDom();
  const b = ioBinds[i];
  b.role = (b.dir === 1) ? 3 : 1;
  renderIoEditor();
}

async function saveCfgIo() {
  ioGatherFromDom();
  const body = {
    enabled: document.getElementById('io-enabled').checked,
    pollSeconds: Math.min(60, Math.max(1, Number(document.getElementById('io-poll').value) || 5)),
    boards: ioBoards.map((b, i) => ({ i: i, enabled: true, addr: b.addr })),
    bindings: ioBinds.map((b, i) => ({
      i: i, enabled: true, board: b.board, pin: b.pin, dir: b.dir,
      role: b.role, zone: b.zone, activeLevel: b.active, pullup: b.pullup
    }))
  };
  try {
    const r = await fetch('/api/io/config', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body)
    });
    const d = await r.json().catch(() => ({}));
    if (!r.ok) { toast((d && d.error) || ('Erreur ' + r.status), true); return; }
    toast('Configuration E/S enregistree');
    await loadCfgIo();
  } catch (e) { toast('Erreur reseau', true); }
}

// ── Topologie relais (cablage) — editeur configurable ──────────────────────
// Convention : guillemets simples en JS, doubles en HTML, &rsquo; pour
// l&rsquo;apostrophe. Aucun echappement fragile.

let topoBoards = [];   // { i, controller, addr, channels, logic }
let topoAssign = [];   // { i, role, target, board, channel }
let topoSource = 'legacy';
let topoPendingReboot = false;

const TOPO_CTRL  = [[0, 'XL9535'], [1, 'MCP23017']];
const TOPO_LOGIC = [[1, 'directe'], [0, 'inversee']];
const TOPO_ROLES = [[1, 'Vanne de zone'], [2, 'Pompe'], [3, 'Auxiliaire'],
                    [4, 'Ventilation serre'], [5, 'Eclairage']];
// Valeurs acceptees par isSupportedChannelCount() : 1, 2, 4, 8. Proposer 16
// fabriquait un choix que l'API refusait ensuite.
const TOPO_CHANCOUNT = [1, 2, 4, 8];

// Seul l'I2C local dispose d'un pilote. Les autres sont montres pour dire ou
// va l'architecture, mais desactives : mieux vaut une porte visiblement
// fermee qu'un choix qui echoue a l'enregistrement.
const TOPO_TRANSPORTS = [[0, 'I2C local'], [1, 'RS485'], [2, 'Reseau IP'], [3, 'LoRa']];
const TOPO_TRANSPORT_DISPO = [0];

function topoZoneCount() {
  const s = adminStatus && adminStatus.system;
  const n = s && Number(s.nbZones);
  return (n && n > 0) ? n : 8;
}

async function loadCfgTopo() {
  let d;
  try { d = await (await fetch('/api/relay/topology')).json(); }
  catch (e) {
    document.getElementById('topo-editor').innerHTML =
      '<div class="cfg-hint">Lecture impossible : ' + e.message + '</div>';
    return;
  }
  topoSource = d.source || 'legacy';
  topoPendingReboot = false;
  topoBoards = (d.boards || []).map(b => ({
    i: b.i, controller: b.controller, addr: b.addr,
    channels: b.channels, logic: b.logic, transport: b.transport || 0
  }));
  topoAssign = (d.assignments || []).map(a => ({
    i: a.i, role: a.role, target: a.target, board: a.board, channel: a.channel
  }));
  renderTopoEditor();
}

// Chaque champ porte son etiquette plutot qu'un en-tete de colonne : les
// lignes passent a la ligne selon la largeur, et des en-tetes fixes se
// desaligneraient. Un utilisateur s'est deja trompe de configuration faute
// de savoir ce que designaient les champs.
function topoField(caption, inner) {
  return '<label class="topo-field"><span>' + caption + '</span>' + inner + '</label>';
}

function topoOptions(list, sel) {
  return list.map(o => '<option value="' + o[0] + '"' +
    (o[0] === sel ? ' selected' : '') + '>' + o[1] + '</option>').join('');
}

function topoNumOptions(list, sel) {
  return list.map(v => '<option value="' + v + '"' +
    (v === sel ? ' selected' : '') + '>' + v + '</option>').join('');
}

function topoAddrOptions(sel) {
  let h = '';
  for (let a = 0x20; a <= 0x27; a++)
    h += '<option value="' + a + '"' + (a === sel ? ' selected' : '') +
         '>0x' + a.toString(16) + '</option>';
  return h;
}

function topoBoardOptions(sel) {
  if (!topoBoards.length) return '<option value="0">(aucune carte)</option>';
  return topoBoards.map(b => '<option value="' + b.i + '"' +
    (b.i === sel ? ' selected' : '') + '>Carte ' + b.i +
    ' (0x' + b.addr.toString(16) + ')</option>').join('');
}

function topoChannelOptions(boardIdx, sel) {
  const b = topoBoards.find(x => x.i === boardIdx);
  const n = b ? b.channels : 8;
  let h = '';
  for (let c = 0; c < n; c++)
    h += '<option value="' + c + '"' + (c === sel ? ' selected' : '') + '>' + c + '</option>';
  return h;
}

// La cible depend du role : une vanne vise une zone, les autres un index libre.
function topoTargetOptions(role, sel) {
  let h = '';
  if (role !== 1) {
    for (let t = 0; t < 8; t++)
      h += '<option value="' + t + '"' + (t === sel ? ' selected' : '') + '>' + t + '</option>';
    return h;
  }
  for (let z = 0; z < topoZoneCount(); z++)
    h += '<option value="' + z + '"' + (z === sel ? ' selected' : '') +
         '>Zone ' + (z + 1) + '</option>';
  return h;
}

function renderTopoEditor() {
  const el = document.getElementById('topo-editor');
  const src = (topoSource === 'nvs')
    ? '<span class="io-ok">enregistree</span>'
    : '<span class="cfg-muted">aucun c&acirc;blage enregistr&eacute;</span>';
  let h = '<div class="cfg-hint">Source en vigueur : ' + src +
          '. Toute modification s&rsquo;applique au prochain redemarrage.</div>';
  if (topoPendingReboot) {
    h += '<div class="cfg-hint io-ok">Enregistre. Ce qui est affiche ci-dessous '
       + 'est votre saisie, en attente du prochain redemarrage &mdash; la '
       + 'configuration en vigueur reste celle d&rsquo;avant.</div>';
  }

  h += '<div class="cfg-subsection-title">Cartes relais</div>';
  if (!topoBoards.length) h += '<div class="cfg-hint">Aucune carte.</div>';
  topoBoards.forEach((b, n) => {
    h += '<div class="io-row"><span class="io-lbl">Carte ' + b.i + '</span>'
      + topoField('Controleur', '<select data-topo="ctrl" data-n="' + n + '">' + topoOptions(TOPO_CTRL, b.controller) + '</select>')
      + topoField('Adresse I2C', '<select data-topo="addr" data-n="' + n + '">' + topoAddrOptions(b.addr) + '</select>')
      + topoField('Nb de voies', '<select data-topo="chan" data-n="' + n + '" title="nombre de relais physiques sur la carte">' + topoNumOptions(TOPO_CHANCOUNT, b.channels) + '</select>')
      + topoField('Logique', '<select data-topo="logic" data-n="' + n + '" title="directe = 1 ouvre le relais ; inversee = 0 ouvre">' + topoOptions(TOPO_LOGIC, b.logic) + '</select>')
      + topoField('Transport', '<select data-topo="transport" data-n="' + n + '" title="ou vit la carte : bus local, ou lien distant">'
      + TOPO_TRANSPORTS.map(o => '<option value="' + o[0] + '"'
          + (o[0] === (b.transport || 0) ? ' selected' : '')
          + (TOPO_TRANSPORT_DISPO.indexOf(o[0]) < 0 ? ' disabled' : '')
          + '>' + o[1] + (TOPO_TRANSPORT_DISPO.indexOf(o[0]) < 0 ? ' (a venir)' : '')
          + '</option>').join('') + '</select>')
      + '<button class="io-del" onclick="topoRemoveBoard(' + n + ')">&#10007;</button></div>';
  });
  h += '<button class="btn-cfg" onclick="topoAddBoard()" style="margin:6px 0">+ Ajouter une carte</button>';
  // Le backend V4 ne dispose aujourd'hui que d'un pilote XL9535 : une carte
  // MCP23017 fonctionne en profil historique mais serait impilotable en V4.
  // On le dit ici plutot que de laisser decouvrir la panne apres un flash.
  if (topoBoards.some(b => b.controller === 1)) {
    h += '<div class="cfg-hint io-ko">Attention : le pilotage MCP23017 '
       + 'n&rsquo;existe que dans le moteur historique. Le moteur V4 ne sait '
       + 'piloter que des cartes XL9535.</div>';
  }

  h += '<div class="cfg-subsection-title" style="margin-top:12px">Affectations</div>';
  h += '<div class="cfg-hint">Chaque affectation relie un role (vanne de zone, pompe&hellip;) '
     + 'a une voie physique : carte + canal.</div>';
  if (!topoAssign.length) h += '<div class="cfg-hint">Aucune affectation.</div>';
  topoAssign.forEach((a, n) => {
    h += '<div class="io-bind">'
      + topoField('Role', '<select data-topo="role" data-n="' + n + '" onchange="topoOnRoleChange(' + n + ')">' + topoOptions(TOPO_ROLES, a.role) + '</select>')
      + topoField('Pilote', '<select data-topo="target" data-n="' + n + '" title="ce que cette voie commande">' + topoTargetOptions(a.role, a.target) + '</select>')
      + topoField('Sur la carte', '<select data-topo="board" data-n="' + n + '" onchange="topoOnBoardChange(' + n + ')">' + topoBoardOptions(a.board) + '</select>')
      + topoField('Canal (relais)', '<select data-topo="channel" data-n="' + n + '" title="numero du relais sur cette carte, a partir de 0">' + topoChannelOptions(a.board, a.channel) + '</select>')
      + '<button class="io-del" onclick="topoRemoveAssign(' + n + ')">&#10007;</button></div>';
  });
  h += '<button class="btn-cfg" onclick="topoAddAssign()" style="margin:6px 0">+ Ajouter une affectation</button>';
  el.innerHTML = h;
}

// Recopie l'etat des selects dans les tableaux avant tout re-render.
function topoGatherFromDom() {
  document.querySelectorAll('#topo-editor [data-topo]').forEach(elm => {
    const k = elm.dataset.topo, n = Number(elm.dataset.n);
    const val = Number(elm.value);
    if (k === 'ctrl' && topoBoards[n]) topoBoards[n].controller = val;
    else if (k === 'addr' && topoBoards[n]) topoBoards[n].addr = val;
    else if (k === 'chan' && topoBoards[n]) topoBoards[n].channels = val;
    else if (k === 'logic' && topoBoards[n]) topoBoards[n].logic = val;
    else if (k === 'transport' && topoBoards[n]) topoBoards[n].transport = val;
    else if (topoAssign[n]) {
      if (k === 'role') topoAssign[n].role = val;
      else if (k === 'target') topoAssign[n].target = val;
      else if (k === 'board') topoAssign[n].board = val;
      else if (k === 'channel') topoAssign[n].channel = val;
    }
  });
}

function topoNextIndex(list, max) {
  for (let i = 0; i < max; i++) if (!list.some(x => x.i === i)) return i;
  return -1;
}

function topoAddBoard() {
  topoGatherFromDom();
  const i = topoNextIndex(topoBoards, 8);
  if (i < 0) { toast('8 cartes au maximum', true); return; }
  topoBoards.push({ i: i, controller: 0, addr: 0x20, channels: 8, logic: 1 });
  renderTopoEditor();
}
function topoRemoveBoard(n) { topoGatherFromDom(); topoBoards.splice(n, 1); renderTopoEditor(); }

function topoAddAssign() {
  topoGatherFromDom();
  const i = topoNextIndex(topoAssign, 20);
  if (i < 0) { toast('20 affectations au maximum', true); return; }
  const board = topoBoards.length ? topoBoards[0].i : 0;
  topoAssign.push({ i: i, role: 1, target: 0, board: board, channel: 0 });
  renderTopoEditor();
}
function topoRemoveAssign(n) { topoGatherFromDom(); topoAssign.splice(n, 1); renderTopoEditor(); }

// Changer de role change la nature de la cible ; changer de carte peut rendre
// le canal hors bornes. Dans les deux cas on repart d'une valeur sure.
function topoOnRoleChange(n) { topoGatherFromDom(); topoAssign[n].target = 0; renderTopoEditor(); }
function topoOnBoardChange(n) { topoGatherFromDom(); topoAssign[n].channel = 0; renderTopoEditor(); }

async function saveCfgTopo() {
  topoGatherFromDom();
  const body = {
    boards: topoBoards.map(b => ({
      i: b.i, controller: b.controller, addr: b.addr,
      channels: b.channels, logic: b.logic, transport: b.transport || 0
    })),
    assignments: topoAssign.map(a => ({
      i: a.i, role: a.role, target: a.target, board: a.board, channel: a.channel
    }))
  };
  try {
    const r = await fetch('/api/relay/topology', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    const d = await r.json().catch(() => ({}));
    if (!r.ok) { toast((d && d.error) || ('Erreur ' + r.status), true); return; }
    // Surtout NE PAS relire ici : /api/relay/topology renvoie la topologie
    // EN VIGUEUR, pas celle qu'on vient d'enregistrer -- elle ne s'applique
    // qu'au redemarrage. Relire effacait donc a l'ecran la saisie que l'on
    // venait de sauvegarder, en laissant croire a une perte.
    topoPendingReboot = true;
    toast('Topologie enregistree — active au prochain redemarrage');
    renderTopoEditor();
  } catch (e) { toast('Erreur reseau', true); }
}


// Effacer le cablage ne "revient" plus a rien : il ne reste aucune
// deduction de secours derriere. Le module cessera simplement de piloter
// quoi que ce soit, ce que la confirmation doit dire sans detour.
async function topoEraseWiring() {
  if (!confirm('Effacer le cablage enregistre ? Le module ne pilotera plus '
             + 'aucune sortie tant qu un nouveau cablage n aura pas ete decrit.')) return;
  try {
    const r = await fetch('/api/relay/topology/reset', { method: 'POST' });
    if (!r.ok) { toast('Effacement refuse', true); return; }
    toast('Cablage efface, effectif au prochain redemarrage');
    await loadCfgTopo();
    await refreshWiringState();
  } catch (e) { toast('Erreur reseau', true); }
}

// Etat du cablage, partage par le bandeau du menu Zones et par la fiche
// systeme. Un module sans cablage ne peut rien piloter : plutot que de le
// laisser deviner devant huit tuiles hachurees, on nomme l'etat et on ouvre
// la page qui le corrige.
let _wiring = null;

function wiringLabel() {
  if (!_wiring) return '--';
  if (!_wiring.wired) return 'non configuré';
  const b = _wiring.boards || 0, c = _wiring.channels || 0;
  return `${b} carte${b>1?'s':''}, ${c} voie${c>1?'s':''}`;
}

function openTopoEditor() { openCfgPage('g-relais'); }

async function refreshWiringState() {
  try {
    const d = await (await fetch('/api/relay/topology')).json();
    const boards = d.boards || [];
    _wiring = {
      wired: d.wired === true,
      boards: boards.length,
      channels: boards.reduce((n, b) => n + (b.channels || 0), 0)
    };
  } catch (e) { _wiring = null; }
  const note = document.getElementById('relay-unwired');
  if (note) note.style.display = (_wiring && !_wiring.wired) ? 'block' : 'none';
  const span = document.getElementById('sys-wiring');
  if (span) span.textContent = wiringLabel();
}
