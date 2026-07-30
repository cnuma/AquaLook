const DAYS_ESP = ['Lun','Mar','Mer','Jeu','Ven','Sam','Dim'];
const DAYS_FULL = ['Lundi','Mardi','Mercredi','Jeudi','Vendredi','Samedi','Dimanche'];
function getNbZones()  { return status?.zones?.length || 2; }
function getZoneName(i){ return status?.zones?.[i]?.name || `Zone ${i+1}`; }
function isZoneActive(i){ return !!(status?.zones?.[i]?.active || status?.zones?.[i]?.relayActive); }
let status      = null;
let adminStatus = null;
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
async function fetchAdminStatus() {
  try {
    const r = await fetch('/api/adminStatus');
    adminStatus = await r.json();
    populateDrawer();
  } catch(e) { /* silencieux */ }
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
  renderZones();
  ensureAllZoneSlots().then(() => renderPlanning());
}
const ZONE_COLORS = ['green','blue','amber','purple','green','blue','amber','purple',
                     'green','blue','amber','purple','green','blue','amber','purple'];
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
function renderZonesGrid() {
  const el     = document.getElementById('zones-container');
  const manDur = status.manualDurationMin || 10;
  el.style.display = 'block';
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
    return `
    <div class="zone-card zone-color-${color} ${active ? 'zone-card-active' : ''} ${z.mode === 1 ? 'zone-interval' : ''}"
         onclick="openZoneConfigModal(${i})" title="Configurer ${name}">
      <div class="zone-card-header">
        <div class="zone-name">
          <span class="zone-dot zone-dot-${color}"></span>
          ${name}
          <span class="zone-badge ${active ? 'on' : 'off'}">${active ? 'ON' : 'OFF'}</span>
        </div>
        <div class="zone-reason">${reason}</div>
        <button class="btn-run ${active ? 'active' : ''}"
                onclick="event.stopPropagation(); toggleManual(${i}, ${!active})">
          ${active ? 'Arreter' : 'Arroser ' + manDur + ' min'}
        </button>
      </div>
      <div class="zone-card-meta">
        <span class="zone-meta-item">&#128197; ${modeStr}</span>
        <span class="zone-meta-item">&#9748; &ge;${threshMm}mm / ${hours}h</span>
        <span class="zone-meta-edit">&#9998; Configurer</span>
      </div>
    </div>`;
  }).join('');
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
    return `<tr class="${active ? 'zone-active-'+color : ''} ${z.mode === 1 ? 'zone-interval' : ''}"
                onclick="openZoneConfigModal(${i})"
                title="Configurer ${name}">
      <td class="zt-name zt-name-${color}">
        <span class="zone-dot zone-dot-${color}"></span>
        ${name}
        <span class="zt-badge ${active ? 'on' : 'off'}">${active ? 'ON' : 'OFF'}</span>
      </td>
      <td class="zt-cell zt-mode">${mode}</td>
      <td class="zt-cell zt-rain">&#9748;&nbsp;&ge;&nbsp;${threshMm}mm&nbsp;/&nbsp;${hours}h</td>
      <td class="zt-cell zt-reason">${reason}</td>
      <td class="zt-cell zt-edit-hint">&#9998;</td>
      <td class="zt-action" onclick="event.stopPropagation()">
        <button class="btn-run ${active ? 'active' : ''}"
                onclick="toggleManual(${i}, ${!active})">
          ${active ? 'Arreter' : 'Arroser&nbsp;'+manDur+'&nbsp;min'}
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
      <span class="zone-dot zone-dot-${color}" style="width:12px;height:12px"></span>
      <span class="zone-cfg-modal-name">${name}</span>
    </div>
    <div class="zone-cfg-field">
      <label>Nom</label>
      <input type="text" id="zcfg-name" value="${name}" maxlength="20" placeholder="Nom de la zone">
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
  if (name) await api('/api/zoneName', { zone: zoneIdx, name });
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
  html += `<div></div>`;
  colDays.forEach((esp,col) => {
    const fd = forecast[col];
    if (fd && fd.valid) {
      const rain = parseFloat(fd.rainMm ?? fd.rain)||0;
      const tmax = parseFloat(fd.tempMax), tmin = parseFloat(fd.tempMin);
      const wind = parseFloat(fd.windMaxKmh), windDeg = parseFloat(fd.windDeg);
      const icon = rain>1.0?'&#127783;':'&#9728;';
      const tip  = weatherTooltipHtml(fd, DAYS_FULL[esp]);
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
    const zoneColor = ZONE_COLORS[zi];  // identite couleur de cette zone
    const colorKeyMap = {green:'cZone0', blue:'cZone1', amber:'cZone2', purple:'cZone3'};
    const zHex = (displayConfig && displayConfig[colorKeyMap[zoneColor]]) || null;
    if (z.mode === 0) {
      html += `<div class="pg-header zone-hdr pg-zone-label-${zoneColor}" style="font-size:${nb>8?'9px':'11px'}">${name}</div>`;
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
            ? enabled.map(s=>`<div class="mini-slot ${rainBlk?'amber':zoneColor}">
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
        <span class="pg-zone-label-${zoneColor}">${name}</span>
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
              ? enabled.map(s=>`<div class="mini-slot ${rainBlk?'amber':zoneColor}">${pad(s.h??s.hour)}:${pad(s.m??s.minute)} ${s.d??s.duration}'</div>`).join('')
              : `<span class="pg-zone-label-${zoneColor}" style="font-size:11px">&#8635; /${intervalD}j</span>`)
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
function weatherTooltipHtml(fd, dayLabel) {
  const cfg = Object.assign({}, DISP_DEFAULTS, displayConfig || {});
  const lines = [];
  const n = (v, digits=0) => Number.isFinite(Number(v)) ? Number(v).toFixed(digits) : null;
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
  fetchAdminStatus();
  fetchDisplayConfig();  // pre-remplit la section Affichage LCD a chaque ouverture
}
function closeDrawer() {
  document.getElementById('drawer').classList.remove('open');
  document.getElementById('drawer-overlay').classList.remove('open');
}
function toggleSection(id) {
  document.getElementById(id).classList.toggle('open');
}
function populateDrawer() {
  if (!adminStatus) return;
  const s = adminStatus;
  if (s.system) {
    const zonesEl = document.getElementById('cfg-nb-zones');
    const ctrlEl  = document.getElementById('cfg-relay-controller');
    const logicEl = document.getElementById('cfg-relay-logic');
    if (zonesEl) zonesEl.dataset.current = s.system.nbZones ?? 2;
    if (ctrlEl)  ctrlEl.dataset.current  = s.system.relayController ?? 0;
    if (logicEl) logicEl.dataset.current = s.system.relayLogic ?? 0;
  }
  const city = s.owm?.city || '';
  document.getElementById('header-city').textContent = city;
  if (city) {
    document.getElementById('header-title').textContent = 'AQUALOOK -- ' + city.toUpperCase();
  }
  const ssid = s.wifi?.ssid || '--';
  document.getElementById('wifi-info').innerHTML =
    `SSID : <span>${ssid}</span><br>
     IP : <span>${s.wifi?.ip||'--'}</span><br>
     ?tat : <span>${s.wifi?.state||'--'}</span>`;
  if (s.ntp) {
    document.getElementById('cfg-ntp-server').value = s.ntp.server || 'pool.ntp.org';
    document.getElementById('cfg-ntp-gmt').value    = s.ntp.gmtOffset ?? 3600;
    document.getElementById('cfg-ntp-dst').value    = s.ntp.dstOffset ?? 3600;
  }
  if (s.owm) {
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
    const nbZ = s.system.nbZones ?? 2;
    const rcSel = document.getElementById('cfg-relay-controller');
    if (rcSel) rcSel.value = s.system.relayController ?? 0;
    updateZoneOptions(nbZ);
    const rlSel = document.getElementById('cfg-relay-logic');
    if (rlSel) rlSel.value = s.system.relayLogic ?? 0;
  }
  if (status) {
    document.getElementById('cfg-manual-dur').value = status.manualDurationMin ?? 10;
  }
  _applyZonesViewBtn();
  const rlLabel = (s.system?.relayLogic === 1) ? 'Directe (bit=1 ON)' : 'Inverse (bit=0 ON)';
  const rcLabel = (s.system?.relayController === 1) ? 'MCP23017' : 'XL9535';
  document.getElementById('sys-info').innerHTML =
    `IP : <span>${s.wifi?.ip||'--'}</span><br>
     RAM libre : <span>${s.heap||'--'} o</span><br>
     Uptime : <span>${fmtUptime(s.uptime)}</span><br>
     NTP : <span>${s.ntp?.synced ? s.ntp.time : 'Non sync'}</span><br>
     OWM : <span>${s.owm?.hasKey ? 'OK Cle configuree' : '? Pas de cle'}</span><br>
     Ville : <span>${s.owm?.city || s.owm?.lat || '--'}</span><br>
     Veille : <span>${s.system?.screenTimeout===0 ? 'Desactivee' : (s.system?.screenTimeout||5)+'min'}</span><br>
     Zones : <span>${s.system?.nbZones||2}</span><br>
     Contrôleur relais : <span>${rcLabel}</span><br>
     Logique relais : <span>${rlLabel}</span>`;
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
async function saveCfgNtp() {
  await api('/api/ntp', {
    server:    document.getElementById('cfg-ntp-server').value.trim() || 'pool.ntp.org',
    gmtOffset: parseInt(document.getElementById('cfg-ntp-gmt').value) || 3600,
    dstOffset: parseInt(document.getElementById('cfg-ntp-dst').value) || 3600
  });
  toast('NTP mis a jour');
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
  const body    = {units};
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
  const controller = parseInt(document.getElementById('cfg-relay-controller').value) || 0;
  const relayLogic = parseInt(document.getElementById('cfg-relay-logic').value) || 0;
  const nbZones = Math.min(8, parseInt(document.getElementById('cfg-nb-zones').value) || 2);
  const maxMin = Math.min(120, Math.max(1, parseInt(document.getElementById('cfg-maxwater').value) || 60));
  const manDur = Math.min(120, Math.max(1, parseInt(document.getElementById('cfg-manual-dur').value) || 10));
  const ctrlEl = document.getElementById('cfg-relay-controller');
  const logicEl = document.getElementById('cfg-relay-logic');
  const zonesEl = document.getElementById('cfg-nb-zones');
  const oldController = parseInt(ctrlEl.dataset.current || '0');
  const oldRelayLogic = parseInt(logicEl.dataset.current || '0');
  const oldNbZones = parseInt(zonesEl.dataset.current || '2');
  const needReboot = controller !== oldController || relayLogic !== oldRelayLogic || nbZones !== oldNbZones;
  if (needReboot) {
    const controllerLabel = controller === 1 ? 'MCP23017' : 'XL9535';
    const logicLabel = relayLogic === 0 ? 'inverse' : 'directe';
    const message = `Appliquer ${controllerLabel}, ${nbZones} zone${nbZones>1?'s':''}, logique ${logicLabel} ? Le module va redémarrer.`;
    if (!confirm(message)) return;
  }
  const response = await api('/api/system', {
    relayController: controller,
    relayLogic,
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
function updateZoneOptions(preferredValue) {
  const controller = parseInt(document.getElementById('cfg-relay-controller')?.value || '0');
  const select = document.getElementById('cfg-nb-zones');
  if (!select) return;
  const current = Number.isFinite(Number(preferredValue))
    ? Number(preferredValue)
    : (parseInt(select.value) || parseInt(select.dataset.current) || 2);
  const values = controller === 1 ? [1,2,3,4,5,6,7,8] : [2,4,6,8];
  const chosen = values.reduce((best, value) =>
    Math.abs(value-current) < Math.abs(best-current) ? value : best, values[0]);
  select.innerHTML = values.map(value =>
    `<option value="${value}">${value} zone${value>1?'s':''}</option>`).join('');
  select.value = String(chosen);
  const hint = document.getElementById('cfg-zones-hint');
  if (hint) hint.textContent = controller === 1
    ? 'MCP23017 : choix libre de 1 a 8 zones. 1 zone pilote 1 sortie.'
    : 'XL9535 : choix provisoire par paires de 2, de 2 a 8 zones.';
}
async function saveCfgSystem() {
  const timeout = parseInt(document.getElementById('cfg-screen-timeout').value) || 5;
  const ledMode = parseInt(document.getElementById('cfg-led-mode').value) || 1;
  await api('/api/system', { screenTimeout: timeout, ledMode });
  toast('Systeme enregistre');
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
setInterval(fetchStatus, 8000);        // 8s -- moins agressif pour l'ESP32
setInterval(fetchAdminStatus, 60000);  // 1min -- rarement necessaire
let displayConfig = null;
const DISP_COLOR_FIELDS   = ['cBg','cSurface','cSurface2','cBorder',
                              'cText','cText2','cMuted','cActiveBg',
                              'cZone0','cZone1','cZone2','cZone3'];
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
  var z0 = cfg.cZone0 || '#00fc00';
  var z1 = cfg.cZone1 || '#0090f8';
  var z2 = cfg.cZone2 || '#f8a400';
  var z3 = cfg.cZone3 || '#780078';
  el.textContent = [
    '.zone-color-green  { border-left-color: ' + z0 + ' !important; }',
    '.zone-color-blue   { border-left-color: ' + z1 + ' !important; }',
    '.zone-color-amber  { border-left-color: ' + z2 + ' !important; }',
    '.zone-color-purple { border-left-color: ' + z3 + ' !important; }',
    '.zone-color-green.zone-card-active  { background: ' + z0 + '18 !important; }',
    '.zone-color-blue.zone-card-active   { background: ' + z1 + '18 !important; }',
    '.zone-color-amber.zone-card-active  { background: ' + z2 + '18 !important; }',
    '.zone-color-purple.zone-card-active { background: ' + z3 + '18 !important; }',
    '.mini-slot.green  { background: ' + z0 + '28 !important; color: ' + z0 + ' !important; }',
    '.mini-slot.blue   { background: ' + z1 + '28 !important; color: ' + z1 + ' !important; }',
    '.mini-slot.amber  { background: ' + z2 + '28 !important; color: ' + z2 + ' !important; }',
    '.mini-slot.purple { background: ' + z3 + '28 !important; color: ' + z3 + ' !important; }',
    '.zt-name-green  { border-left-color: ' + z0 + ' !important; }',
    '.zt-name-blue   { border-left-color: ' + z1 + ' !important; }',
    '.zt-name-amber  { border-left-color: ' + z2 + ' !important; }',
    '.zt-name-purple { border-left-color: ' + z3 + ' !important; }',
    '.pg-zone-label-green  { color: ' + z0 + ' !important; }',
    '.pg-zone-label-blue   { color: ' + z1 + ' !important; }',
    '.pg-zone-label-amber  { color: ' + z2 + ' !important; }',
    '.pg-zone-label-purple { color: ' + z3 + ' !important; }'
  ].join('\n');
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
