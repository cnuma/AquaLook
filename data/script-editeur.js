'use strict';
const $ = (id) => document.getElementById(id);
const S = AquaSchema;
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const ico = (id) => '<svg class="ic"><use href="#' + id + '"/></svg>';
// Memes valeurs que ScriptStore::TRIGGER_* cote firmware.
const TRIG = {1:"le changement d'une entrée", 2:"le démarrage d'une zone", 3:"l'arrêt d'une zone", 0:'rien (lancement manuel)'};
const PALETTE = [['Arrosage',['arroser','arreter','suspendre','reprendre']],['Temps',['attendre']],
  ['Conditions et boucles',['si','repeter','tantque']],['Variables',['variable']],['Informer',['notifier','message']],['Scripts',['lancer','parallele']]];

// Ce que CE module offre.
let ZONES = [], ENTREES = [], BIBLIO = [];
// Emplacements de scripts tels que /api/scripts les decrit, et le nombre de
// scripts que le module fait tourner ensemble (4 sur S3, 2 sur CYD).
let SLOTS = [], SIMULT = 2;
// Limites de la bibliotheque de phrases, relues du module (/api/script-messages).
let BIBLIO_MAX = 48, BIBLIO_LEN = 80;
let TAILLE_MAX = 400, NONCE = 0;
// Notifications pretes sur le module (true/false), null si le firmware ne
// le dit pas encore : on n'affiche alors rien plutot qu'un faux etat.
let NOTIF = null;
// Identifiant du module quand la page est servie par l'espace en ligne
// (?module=<id>) ; null sur le module lui-meme. Voir « Mode espace en ligne ».
const CLOUD = new URLSearchParams(location.search).get('module');
const zoneNom = (id) => { const z = ZONES.find((o) => o.id === id); return z ? z.nom : 'Zone ' + id + ' (inconnue)'; };
const entreeNom = (id) => { const e = ENTREES.find((o) => o.id === id); return e ? e.nom : 'Entrée ' + id + ' (inconnue)'; };
const phraseNom = (c) => { const p = BIBLIO.find((o) => o.code === c); return p ? p.texte : 'code ' + c; };
const scriptSlot = (no) => SLOTS[no - 1];
const scriptNom = (no) => { const s = scriptSlot(no); return no + ' — ' + (s && s.utilise ? s.nom : 'vide'); };
// Variables partagees g1..g16 : [{i, nom, valeur}], relues du module
// (/api/script-globals). Vide (espace en ligne, ancien firmware) : on
// affiche « g3 ».
let GLOBALES = [];
const globaleNom = (i) => { const g = GLOBALES.find((o) => o.i === i); return g && g.nom ? g.nom : 'g' + i; };
const CMP_TXT = { '==': '=', '!=': '≠', '<': '<', '>': '>', '<=': '≤', '>=': '≥' };
const NAMES = { zone: zoneNom, input: entreeNom, phrase: phraseNom, script: scriptNom, globale: globaleNom };

// Le script ouvert. body = liste de blocs (voir script-schema.js).
let doc = { nom: '', actif: true, trigger: { t: 0, target: 0 }, body: [] };
let editable = true;        // faux quand le texte du module ne se dessine pas
let dirty = false;
let sel = null;
let undo = [];
let arret = null;           // bloc du dernier arret signale par le module

// ── Etat et annulation ─────────────────────────────────────────────────
const snap = () => JSON.stringify(doc);
function pushUndo(s) { undo.push(s || snap()); if (undo.length > 80) undo.shift(); $('b-undo').disabled = false; }
function setDirty(v) { dirty = v; $('dirty').hidden = !v; $('b-save').classList.toggle('a-enregistrer', !!v); }
function doUndo() {
  const s = undo.pop(); if (!s) return;
  doc = JSON.parse(s);
  if (sel && sel !== 'start' && !find(sel)) sel = null;
  $('b-undo').disabled = !undo.length;
  setDirty(true);
  refresh(true);
}

function find(id, list) {
  list = list || doc.body;
  for (let i = 0; i < list.length; i++) {
    const n = list[i];
    if (n.id === id) return { node: n, list: list, index: i };
    for (const k of S.childKeys(n)) { const r = find(id, n[k]); if (r) return r; }
  }
  return null;
}
function listsOf(n, acc) { acc = acc || []; S.childKeys(n).forEach((k) => { acc.push(n[k]); n[k].forEach((c) => listsOf(c, acc)); }); return acc; }
function hasWait(list) { return list.some((n) => n.t === 'attendre' || S.childKeys(n).some((k) => hasWait(n[k]))); }
function loopDepth(id, list, d) {
  list = list || doc.body; d = d || 0;
  for (const n of list) {
    if (n.id === id) return d;
    const nd = n.t === 'repeter' ? d + 1 : d;
    for (const k of S.childKeys(n)) { const r = loopDepth(id, n[k], nd); if (r >= 0) return r; }
  }
  return -1;
}
function warnings(n) {
  const w = [];
  if ('zone' in n && !ZONES.some((z) => z.id === n.zone)) w.push('La zone ' + n.zone + ' n’existe pas sur ce module.');
  if (n.cond) n.cond.terms.forEach((t) => {
    if (t.k === 'entree' && !ENTREES.some((e) => e.id === t.id)) w.push('L’entrée ' + t.id + ' n’est pas déclarée sur ce module.');
    if (t.k === 'zoneactive' && !ZONES.some((z) => z.id === t.id)) w.push('La zone ' + t.id + ' n’existe pas sur ce module.');
    if (t.k === 'globale' && GLOBALES.length && globaleNom(t.id) === 'g' + t.id) w.push('La variable g' + t.id + ' n’a pas de nom : onglet « Variables », en bas.');
  });
  if (n.t === 'variable' && GLOBALES.length && globaleNom(n.g) === 'g' + n.g) w.push('La variable g' + n.g + ' n’a pas de nom : onglet « Variables », en bas.');
  if (n.t === 'lancer') {
    const c = scriptSlot(n.script);
    if (n.script === slot() + 1) w.push('Un script ne peut pas se lancer lui-même : le module ignorera ce bloc.');
    else if (!c || !c.utilise) w.push('L’emplacement ' + n.script + ' est vide : rien ne partira.');
    else if (!c.actif) w.push('Le script ' + n.script + ' est inactif : le module refusera de le lancer.');
  }
  if (n.t === 'notifier' && NOTIF === false) w.push('Notifications désactivées sur le module : ce bloc n’enverra rien.');
  if (n.t === 'tantque' && !hasWait(n.body)) w.push('Ajoutez un bloc « Attendre » dans la boucle : sans pause, elle tourne en continu.');
  if (n.t === 'repeter' && loopDepth(n.id) >= 8) w.push('Trop de « Répéter » imbriqués : 8 au plus.');
  return w;
}

// ── Libelles ───────────────────────────────────────────────────────────
function termLabel(t) {
  if (t.k === 'globale') return globaleNom(t.id) + ' ' + CMP_TXT[t.cmp] + ' ' + t.v;
  return t.k === 'entree' ? entreeNom(t.id) + (t.v ? ' est actif' : ' est inactif')
                          : zoneNom(t.id) + (t.v ? ' arrose' : ' n’arrose pas');
}
function condLabel(c) { return S.condText(c, termLabel, ' et ', ' ou '); }
function subOf(n) {
  switch (n.t) {
    case 'arroser': return zoneNom(n.zone) + ' · ' + S.duree(n.sec) + (n.attente ? '' : ' · sans attendre');
    case 'arreter': case 'suspendre': case 'reprendre': return zoneNom(n.zone);
    case 'attendre': return S.duree(n.sec);
    case 'notifier': case 'message': return '« ' + phraseNom(n.code) + ' »' + (n.gvar ? ' + ' + globaleNom(n.gvar) : '');
    case 'lancer': return scriptNom(n.script) + ' · en parallèle';
    case 'si': return condLabel(n.cond) + ' ?';
    case 'variable': return globaleNom(n.g) + (n.op === '=' ? ' = ' : n.op === '-' ? ' − ' : ' + ') + n.v;
    case 'parallele': return n.mode === 'ou' ? 'Ou : reprend dès la 1re finie' : 'Et : attend les deux branches';
    case 'repeter': return n.n + ' fois';
    case 'tantque': return condLabel(n.cond);
  }
  return '';
}
function trigText() {
  const t = doc.trigger;
  if (t.t === 0) return 'Lancement manuel';
  if (t.t === 1) return 'Quand « ' + entreeNom(t.target) + ' » change';
  if (t.t === 2) return 'Quand « ' + zoneNom(t.target) + ' » démarre';
  return 'Quand « ' + zoneNom(t.target) + ' » s’arrête';
}

// ── Rendu du schema ────────────────────────────────────────────────────
let listIds;
function regList(list) { if (!listIds.has(list)) listIds.set(list, 'L' + listIds.size); return listIds.get(list); }
function listById(lid) { for (const [l, id] of listIds) if (id === lid) return l; return null; }
function link(list, i) {
  const d = document.createElement('div'); d.className = 'link';
  if (editable) d.innerHTML = '<button class="plus" data-l="' + regList(list) + '" data-i="' + i + '" aria-label="Ajouter un bloc ici">+</button>';
  return d;
}
function seq(list) {
  const s = document.createElement('div'); s.className = 'seq';
  s.appendChild(link(list, 0));
  list.forEach((n, i) => { s.appendChild(nodeEl(n)); s.appendChild(link(list, i + 1)); });
  return s;
}
// Supprimer directement sur le bloc : le bouton du panneau de droite et la
// touche Suppr existaient, mais ne se voyaient pas depuis le schema.
function delBtn(n) {
  return editable ? '<button class="del" data-del="' + n.id + '" title="Supprimer ce bloc'
    + (S.childKeys(n).length ? ' et tout ce qu’il contient' : '') + '" aria-label="Supprimer ce bloc">×</button>' : '';
}
function boxEl(n) {
  const T = S.TYPES[n.t], d = document.createElement('div');
  const w = warnings(n);
  d.className = 'node cat-' + T.cat;
  d.dataset.id = n.id; d.tabIndex = 0; d.draggable = editable;
  d.innerHTML = '<span class="ico">' + ico(T.icon) + '</span><span class="tt"><b>' + esc(T.label) + '</b><span class="sub">'
    + esc(subOf(n)) + '</span></span>' + (w.length ? '<span class="warn" title="' + esc(w[0]) + '">!</span>' : '')
    + delBtn(n);
  return d;
}
function nodeEl(n) {
  if (n.t === 'si' || n.t === 'parallele') {
    // « Si » : pince, comme les boucles ; une seule colonne s'execute selon
    // la condition. « En parallele » : pas de pince (confus a l'oeil, 8 oct.
    // 2026) ; les deux colonnes partent ENSEMBLE, et l'etiquette du bas dit
    // quand la suite reprend.
    const si = n.t === 'si';
    const w = document.createElement('div'); w.className = si ? 'pince si' : 'parblk';
    w.appendChild(boxEl(n));
    const body = si ? document.createElement('div') : w;
    if (si) {
      const arm = document.createElement('div'); arm.className = 'pince-arm'; w.appendChild(arm);
      body.className = 'pince-body';
    }
    body.appendChild(Object.assign(document.createElement('div'), { className: 'link plain' }));
    const br = document.createElement('div'); br.className = 'branches';
    (si ? [['oui', 'Oui', 'oui'], ['non', 'Non', 'non']] : [['a', 'Branche 1', 'par'], ['b', 'Branche 2', 'par']]).forEach(([k, lab, c]) => {
      const b = document.createElement('div'); b.className = 'branch';
      b.innerHTML = '<div class="chip ' + c + '">' + lab + '</div>';
      b.appendChild(seq(n[k]));
      const g = document.createElement('div'); g.className = 'grow'; b.appendChild(g);
      br.appendChild(b);
    });
    body.appendChild(br);
    if (si) {
      body.appendChild(Object.assign(document.createElement('div'), { className: 'link plain' }));
      w.appendChild(body);
      const f = document.createElement('div'); f.className = 'pince-foot';
      f.innerHTML = ico('i-branch') + 'fin du « si », la suite reprend ici';
      w.appendChild(f);
    } else {
      const fin = document.createElement('div'); fin.className = 'chip par join';
      fin.textContent = n.mode === 'ou' ? 'Ou : reprend dès la 1re finie, l’autre continue' : 'Et : attend les deux';
      w.appendChild(fin);
    }
    return w;
  }
  if (n.t === 'repeter' || n.t === 'tantque') {
    const w = document.createElement('div'); w.className = 'pince boucle';
    w.appendChild(boxEl(n));
    const arm = document.createElement('div'); arm.className = 'pince-arm'; w.appendChild(arm);
    const body = document.createElement('div'); body.className = 'pince-body';
    body.appendChild(seq(n.body)); w.appendChild(body);
    const f = document.createElement('div'); f.className = 'pince-foot';
    f.innerHTML = ico('i-loop') + (n.t === 'repeter' ? 'recommence (' + n.n + ' fois)' : 'recommence tant que c’est vrai');
    w.appendChild(f);
    return w;
  }
  return boxEl(n);
}
function renderCanvas() {
  listIds = new Map();
  const w = $('world'); w.innerHTML = '';
  const st = document.createElement('div');
  st.className = 'node start' + (doc.actif ? '' : ' off'); st.dataset.id = 'start'; st.tabIndex = 0;
  st.innerHTML = '<span class="ico">' + ico('i-bolt') + '</span><span class="tt"><span class="eye">'
    + (doc.actif ? 'DÉPART' : 'DÉPART · SCRIPT INACTIF') + '</span><b>' + esc(trigText()) + '</b></span>';
  w.appendChild(st);
  w.appendChild(seq(doc.body));
  const e = document.createElement('div'); e.className = 'end'; e.textContent = 'FIN'; w.appendChild(e);
}
function markSel() {
  document.querySelectorAll('.sel,.arret').forEach((e) => e.classList.remove('sel', 'arret'));
  if (arret) { const e = document.querySelector('[data-id="' + arret + '"]'); if (e) e.classList.add('arret'); }
  if (sel) { const e = document.querySelector('[data-id="' + sel + '"]'); if (e) e.classList.add('sel'); }
  document.querySelectorAll('.cl').forEach((l) => l.classList.toggle('hl', !!sel && l.dataset.id === sel));
}

// ── Texte genere ───────────────────────────────────────────────────────
let GEN = { text: '', lines: [] };
let lecture = null;         // { texte, octets } du module quand le schema est en lecture seule
// Mode texte : le script ne se dessine pas mais son texte est connu. Le
// declencheur et « Actif » restent reglables (panneau de droite), comme dans
// l'ancien editeur texte.
const modeTexte = () => !editable && !!(lecture && lecture.texte);
const KW = /\b(si|alors|sinon|finsi|pour|de|à|faire|finpour|tantque|fintantque|demarrer|zone|pendant|arreter|suspendre|reprendre|attendre|notifier|message|et|ou)\b/g;
function renderCode() {
  if (!editable && lecture) {
    // Lecture seule : montrer ce qui tourne vraiment, pas un schema vide.
    $('code').innerHTML = (lecture.texte.replace(/\n+$/, '') || '# texte source absent de la carte SD').split('\n').map((l, i) =>
      '<div class="cl"><span class="ln">' + (i + 1) + '</span><span>' + esc(l) + '</span></div>').join('');
    $('taille').textContent = lecture.octets + ' / ' + TAILLE_MAX + ' octets';
    $('taille').className = 'note';
    return;
  }
  GEN = S.generate(doc.body, NAMES);
  $('code').innerHTML = GEN.lines.map((l, i) => {
    const h = l.text.indexOf('#');
    const code = h < 0 ? l.text : l.text.slice(0, h), cm = h < 0 ? '' : l.text.slice(h);
    return '<div class="cl" data-id="' + l.id + '"><span class="ln">' + (i + 1) + '</span><span>'
      + esc(code).replace(KW, '<span class="kw">$1</span>') + (cm ? '<span class="cm">' + esc(cm) + '</span>' : '') + '</span></div>';
  }).join('') || '<div class="cl"><span class="ln">1</span><span class="cm"># script vide : ajoutez un bloc sous « Départ »</span></div>';
  // Taille en direct : le module refuse au-dela de ScriptStore::MAX_BYTECODE.
  const t = $('taille');
  try {
    const n = AquaScript.compile(GEN.text).length;
    t.textContent = n + ' / ' + TAILLE_MAX + ' octets';
    t.className = 'note' + (n > TAILLE_MAX ? ' hors' : n > TAILLE_MAX * 0.85 ? ' proche' : '');
  } catch (e) { t.textContent = 'non compilable : ' + e.message; t.className = 'note hors'; }
  if (!$('p-phr').hidden || !$('p-var').hidden) majPuces();
}

// ── Reglages ───────────────────────────────────────────────────────────
const optsList = (list, v, vide) => list.length
  ? list.map((o) => '<option value="' + o.id + '"' + (o.id === v ? ' selected' : '') + '>' + esc(o.nom) + '</option>').join('')
    + (list.some((o) => o.id === v) ? '' : '<option value="' + v + '" selected>' + v + ' (inconnue)</option>')
  : '<option value="' + v + '">' + esc(vide) + '</option>';
// Entier signe 32 bits saisi (vide ou invalide : 0).
const entier = (s) => { const v = parseInt(s, 10); return Number.isFinite(v) ? Math.max(-2147483648, Math.min(2147483647, v)) : 0; };
const globaleOpts = (sel) => Array.from({ length: S.GLOBAL_COUNT }, (_, k) => k + 1).map((i) =>
  '<option value="' + i + '"' + (i === sel ? ' selected' : '') + '>' + esc('g' + i + (globaleNom(i) !== 'g' + i ? ' — ' + globaleNom(i) : '')) + '</option>').join('');
const optsMap = (o, v) => Object.entries(o).map(([k, l]) => '<option value="' + k + '"' + (String(k) === String(v) ? ' selected' : '') + '>' + esc(l) + '</option>').join('');
const zoneOpts = (v) => optsList(ZONES, v, 'aucune zone');
const entreeOpts = (v) => optsList(ENTREES, v, 'aucune entrée déclarée');
function phraseOpts(v) {
  const l = BIBLIO.map((p) => ({ id: p.code, nom: p.code + ' — ' + p.texte }));
  return optsList(l, v, 'code ' + v + ' (aucune phrase)');
}
// Duree : saisie dans l'unite la plus naturelle pour la valeur.
function unitOf(sec) { return sec >= 3600 && sec % 3600 === 0 ? 3600 : sec % 60 === 0 ? 60 : 1; }
function dureeField(sec) {
  const u = unitOf(sec);
  return '<div class="f"><label for="p-d">Durée</label><div class="row"><input id="p-d" type="number" min="1" data-f="duree" value="'
    + (sec / u) + '"><select id="p-u" data-f="duree">' + optsMap({1:'secondes',60:'minutes',3600:'heures'}, u) + '</select></div></div>';
}
// Un « et » / « ou » entre chaque condition (cond.ops[i] joint les termes i
// et i+1). « et » passe avant « ou », comme dans le langage : la ligne « Lu
// comme » montre les groupes entre parentheses. Les conditions se deplacent
// par la poignee ou le bouton ↑ ; les « et/ou » restent a leur place.
function condFields(c) {
  let h = '<div class="lab" style="margin-bottom:6px">Condition</div>';
  c.terms.forEach((t, i) => {
    if (i) h += '<div class="opsel' + (c.ops[i - 1] === 'ou' ? ' ou' : '') + '"><select data-op="' + (i - 1)
      + '" aria-label="Lien entre les conditions ' + i + ' et ' + (i + 1) + '">' + optsMap({et:'et', ou:'ou'}, c.ops[i - 1]) + '</select></div>';
    h += '<div class="term" data-tbox="' + i + '">'
      + '<div class="row">' + (c.terms.length > 1 ? '<span class="grip" title="Glisser pour déplacer cette condition">⠿</span>' : '')
      + '<select data-term="' + i + '" data-tf="k" aria-label="Sur">' + optsMap({entree:'une entrée', zoneactive:'une zone', globale:'une variable'}, t.k) + '</select>'
      + (i ? '<button class="btn x" data-up-term="' + i + '" title="Monter cette condition" aria-label="Monter cette condition">↑</button>' : '')
      + (c.terms.length > 1 ? '<button class="btn x danger" data-del-term="' + i + '" title="Retirer cette condition" aria-label="Retirer cette condition">×</button>' : '') + '</div>'
      + (t.k === 'globale'
        ? '<select data-term="' + i + '" data-tf="id" aria-label="Laquelle">' + globaleOpts(t.id) + '</select>'
          + '<div class="row"><select data-term="' + i + '" data-tf="cmp" aria-label="Comparaison">'
          + optsMap({'==':'= égale à', '!=':'≠ différente de', '>':'> plus que', '>=':'≥ au moins', '<':'< moins que', '<=':'≤ au plus'}, t.cmp)
          + '</select><input type="number" data-term="' + i + '" data-tf="v" value="' + t.v + '" aria-label="Valeur" style="width:90px"></div>'
        : '<select data-term="' + i + '" data-tf="id" aria-label="Laquelle">' + (t.k === 'entree' ? entreeOpts(t.id) : zoneOpts(t.id)) + '</select>'
          + '<select data-term="' + i + '" data-tf="v" aria-label="État">' + optsMap(t.k === 'entree' ? {1:'est actif', 0:'est inactif'} : {1:'arrose', 0:'n’arrose pas'}, t.v) + '</select>')
      + '</div>';
  });
  if (c.terms.length > 1) {
    const mix = c.ops.includes('et') && c.ops.includes('ou');
    h += '<div class="hint">Lu comme : <b>' + esc(condLabel(c)) + '</b>.'
      + (mix ? ' « et » passe avant « ou » : chaque groupe entre parenthèses est vrai si toutes ses conditions le sont, et il suffit d’un groupe vrai.' : '')
      + '</div>';
  }
  h += '<button class="btn" id="p-add-term">+ Ajouter une condition</button>';
  return h;
}
function moveTerm(c, from, to) {
  if (from === to || to < 0 || to >= c.terms.length) return;
  const t = c.terms.splice(from, 1)[0];
  c.terms.splice(to, 0, t);
}
function renderProps() {
  const p = $('props');
  if (!sel) {
    p.innerHTML = '<div class="empty"><p>Cliquez sur un bloc du schéma pour le régler.</p><ol>'
      + '<li>Le bloc vert <b>Départ</b> fixe ce qui lance le script.</li>'
      + '<li>Glissez un bloc de la palette sur un <b>+</b> pour l’ajouter.</li>'
      + '<li>Glissez un bloc existant sur un autre <b>+</b> pour le déplacer.</li>'
      + '<li>Le texte du bas est ce que le module recevra.</li></ol>'
      + '<p>Pour revenir ici après avoir choisi un bloc : bouton <b>Réglages généraux</b> en haut, ou touche Échap.</p></div>'
      + '<div class="sec-acts acts"><button class="btn danger" id="b-effacer">Effacer ce script</button></div>'
      // Mot de passe : sans objet dans l'espace en ligne (pas de signature,
      // et /api/auth-secret n'existe que sur le module).
      + (CLOUD ? '' : '<div class="sec-acts"><div class="ph">Mot de passe du module</div>'
        + '<div class="hint">C’est le mot de passe de connexion à la page du module. Il protège aussi chaque '
        + 'enregistrement de script : l’éditeur le demande la première fois, puis le retient tant que cette page reste ouverte.</div>'
        + '<div class="acts"><button class="btn" id="b-secret">Changer le mot de passe…</button>'
        + (secretRetenu() ? '<button class="btn" id="b-oublier">Effacer du navigateur</button>' : '') + '</div>'
        + (secretRetenu() ? '<div class="hint">Sur un ordinateur partagé, cliquez « Effacer du navigateur » avant de partir : '
          + 'le mot de passe gardé par cette page est supprimé du navigateur, ce qui évite qu’une autre personne le réutilise.</div>' : '')
        + '</div>');
    $('b-effacer').onclick = effacer;
    if (!CLOUD) {
      $('b-secret').onclick = definirSecret;
      if ($('b-oublier')) $('b-oublier').onclick = oublierSecret;
    }
    return;
  }
  let h;
  if (sel === 'start') {
    const t = doc.trigger;
    h = '<div class="phead cat-eau">' + ico('i-bolt') + '<b>Départ du script</b></div>'
      + '<div class="f"><label for="s-t">Déclenché par</label><select id="s-t" data-f="trigger">' + optsMap(TRIG, t.t) + '</select></div>'
      + (t.t ? '<div class="f"><label for="s-c">' + (t.t === 1 ? 'Entrée' : 'Zone') + '</label><select id="s-c" data-f="target">'
        + (t.t === 1 ? entreeOpts(t.target) : zoneOpts(t.target)) + '</select></div>' : '')
      + '<div class="f"><label for="s-a">Actif</label><select id="s-a" data-f="actif">' + optsMap({1:'oui', 0:'non'}, doc.actif ? 1 : 0) + '</select></div>'
      + (doc.actif ? '' : '<div class="alert">Script inactif : il ne démarre jamais seul, et « Lancer maintenant » le refuse aussi.</div>')
      + '<div class="hint">Le script part au moment où ça <b>change</b>, jamais sur un état qui dure. Il ne repart pas deux fois en moins de cinq secondes, et ' + SIMULT + ' scripts au plus tournent en même temps.</div>';
  } else {
    const n = find(sel).node, T = S.TYPES[n.t];
    h = '<div class="phead cat-' + T.cat + '">' + ico(T.icon) + '<b>' + esc(T.label) + '</b></div>';
    warnings(n).forEach((w) => { h += '<div class="alert">' + esc(w) + '</div>'; });
    if ('zone' in n) h += '<div class="f"><label for="p-z">Zone</label><select id="p-z" data-f="zone">' + zoneOpts(n.zone) + '</select></div>';
    if ('sec' in n) h += dureeField(n.sec);
    if (n.t === 'variable') {
      h += '<div class="f"><label for="p-g">Variable</label><select id="p-g" data-f="g">' + globaleOpts(n.g) + '</select></div>'
        + '<div class="f"><label for="p-op">Action</label><select id="p-op" data-f="op">'
        + optsMap({ '=': 'prend la valeur', '+': 'augmente de', '-': 'diminue de' }, n.op) + '</select></div>'
        + '<div class="f"><label for="p-gv">Valeur</label><input id="p-gv" type="number" data-f="gv" value="' + n.v + '"'
        + (n.op === '=' ? '' : ' min="0"') + '></div>'
        + '<div class="hint">Partagée par tous les scripts et conservée au redémarrage (enregistrée au plus une fois par minute : '
        + 'une coupure de courant peut faire perdre la dernière minute). Noms et valeurs : onglet « Variables », en bas.</div>';
    }
    if (n.t === 'parallele') {
      const ou = n.mode === 'ou';
      h += '<div class="f"><label for="p-mode">La suite du script reprend</label><select id="p-mode" data-f="mode">'
        + optsMap({ et: 'script en Et : quand les deux branches sont finies', ou: 'script en Ou : dès que l’une est finie' }, ou ? 'ou' : 'et')
        + '</select></div>'
        + '<div class="hint">Les blocs de la <b>branche 1</b> et de la <b>branche 2</b> partent en même temps. '
        + (ou ? 'Dès que l’une est finie, la suite du script part ; l’autre continue jusqu’au bout, et le script ne se termine qu’avec elle. '
              + 'Le bloc occupe trois des ' + SIMULT + ' places du module (le script et ses deux branches)'
              : 'La suite du script attend que les deux soient finies. Le bloc occupe deux des ' + SIMULT + ' places du module')
        + ' : faute de place, une branche s’exécute à la suite de l’autre, et le journal le signale.</div>';
    }
    if (n.t === 'lancer') {
      h += '<div class="f"><label for="p-s">Script à lancer</label><select id="p-s" data-f="script">'
        + SLOTS.filter((x) => x.i !== slot()).map((x) => '<option value="' + (x.i + 1) + '"' + (x.i + 1 === n.script ? ' selected' : '') + '>'
          + esc(scriptNom(x.i + 1) + (x.utilise && !x.actif ? ' (inactif)' : '')) + '</option>').join('')
        + '</select></div>'
        + '<div class="hint">Le script lancé part en parallèle : celui-ci continue aussitôt. S’il ne peut pas partir (déjà en cours, inactif, ou '
        + SIMULT + ' scripts tournent déjà), le refus est écrit dans le journal et ce script continue.</div>';
    }
    if ('code' in n) h += '<div class="f"><label for="p-c">' + (n.t === 'notifier' ? 'Notification envoyée' : 'Ligne écrite au journal')
      + '</label><select id="p-c" data-f="code">' + phraseOpts(n.code) + '</select></div>'
      + (BIBLIO.length ? '' : '<div class="hint">Aucune phrase dans la bibliothèque : ajoutez-en dans l’onglet « Phrases », en bas.</div>');
    if (n.t === 'message' || n.t === 'notifier') {
      h += '<div class="f"><label for="p-gvar">Ajouter la valeur d’une variable (facultatif)</label><select id="p-gvar" data-f="gvar">'
        + '<option value="0"' + (n.gvar ? '' : ' selected') + '>(aucune variable)</option>' + globaleOpts(n.gvar || 0) + '</select></div>'
        + (n.gvar ? '<div class="hint">' + (n.t === 'message' ? 'Le journal' : 'La notification') + ' portera la phrase puis « '
          + esc(globaleNom(n.gvar)) + ' = valeur », lue au moment où le bloc s’exécute.</div>' : '');
    }
    if ('n' in n) h += '<div class="f"><label for="p-n">Nombre de fois</label><input id="p-n" type="number" min="1" max="1000" data-f="n" value="' + n.n + '"></div>';
    if (n.cond) h += condFields(n.cond);
    if (n.t === 'si') h += '<div class="hint">Les blocs sous <b>Oui</b> s’exécutent si la condition est vraie, ceux sous <b>Non</b> sinon. Une branche peut rester vide.</div>';
    if (n.t === 'attendre') h += '<div class="hint">Pendant l’attente, le module continue de piloter les autres zones et l’écran.</div>';
    if (n.t === 'arroser') {
      h += '<div class="f"><label class="check"><input type="checkbox" id="p-att" data-f="attente"' + (n.attente ? ' checked' : '')
        + '> Attendre la fin de l’arrosage avant le bloc suivant</label></div>'
        + '<div class="hint">' + (n.attente
          ? 'Le bloc suivant part quand la zone a fini d’arroser, même si elle a été suspendue entre-temps.'
          : 'La zone s’ouvre et le script continue aussitôt : les blocs suivants se déroulent pendant l’arrosage.')
        + '</div>';
    }
    h += '<div class="acts"><button class="btn" id="p-dup">Dupliquer</button><button class="btn danger" id="p-del">Supprimer le bloc</button></div>';
  }
  p.innerHTML = h;
  if (!editable && !(sel === 'start' && modeTexte())) p.querySelectorAll('select,input,button').forEach((e) => { e.disabled = true; });
  wireProps(p);
}
function wireProps(p) {
  let before = null;
  const target = () => sel === 'start' ? null : find(sel).node;
  const change = (fn, rerender) => {
    pushUndo(before || snap()); before = snap();
    fn(); setDirty(true); refresh(rerender);
  };
  p.querySelectorAll('[data-f],[data-tf]').forEach((f) => {
    f.addEventListener('focus', () => { before = snap(); });
    f.addEventListener('change', () => {
      const n = target();
      if (f.dataset.tf) {
        const t = n.cond.terms[+f.dataset.term];
        change(() => {
          if (f.dataset.tf === 'k') {
            t.k = f.value;
            if (t.k === 'globale') { t.id = 1; t.cmp = '>'; t.v = 0; }
            else { delete t.cmp; t.id = (t.k === 'entree' ? (ENTREES[0] || {id:1}) : (ZONES[0] || {id:1})).id; t.v = 1; }
          }
          else if (f.dataset.tf === 'cmp') t.cmp = f.value;
          else t[f.dataset.tf] = entier(f.value);
        }, true);
        return;
      }
      const k = f.dataset.f;
      if (k === 'trigger') change(() => {
        doc.trigger.t = parseInt(f.value, 10);
        doc.trigger.target = doc.trigger.t === 0 ? 0 : ((doc.trigger.t === 1 ? ENTREES[0] : ZONES[0]) || {id:0}).id;
      }, true);
      else if (k === 'target') change(() => { doc.trigger.target = parseInt(f.value, 10) || 0; }, false);
      else if (k === 'actif') change(() => { doc.actif = f.value === '1'; }, true);
      else if (k === 'attente') change(() => { n.attente = f.checked; }, true);
      else if (k === 'mode') change(() => { n.mode = f.value === 'ou' ? 'ou' : 'et'; }, true);
      else if (k === 'gvar') change(() => { const g = parseInt(f.value, 10) || 0; if (g >= 1 && g <= 16) n.gvar = g; else delete n.gvar; }, true);
      else if (k === 'g') change(() => { n.g = Math.min(16, Math.max(1, parseInt(f.value, 10) || 1)); }, true);
      else if (k === 'op') change(() => { n.op = ['=', '+', '-'].indexOf(f.value) >= 0 ? f.value : '='; if (n.op !== '=') n.v = Math.abs(n.v); }, true);
      else if (k === 'gv') change(() => { n.v = entier(f.value); if (n.op !== '=') n.v = Math.abs(n.v); }, false);
      else if (k === 'duree') change(() => {
        const v = Math.max(1, Math.round(parseFloat($('p-d').value) || 1));
        n.sec = Math.min(86400, v * parseInt($('p-u').value, 10));
      }, true);
      else if (k === 'n') change(() => { n.n = Math.max(1, Math.min(1000, parseInt(f.value, 10) || 1)); }, false);
      else change(() => { n[k] = parseInt(f.value, 10) || 0; }, false);
    });
  });
  p.querySelectorAll('[data-op]').forEach((f) => f.addEventListener('change', () => {
    change(() => { target().cond.ops[+f.dataset.op] = f.value; }, true);
  }));
  // Retirer une condition retire aussi le « et/ou » qui la liait a la precedente
  // (a la suivante pour la premiere).
  p.querySelectorAll('[data-del-term]').forEach((b) => b.addEventListener('click', () => {
    const i = +b.dataset.delTerm;
    change(() => { const c = target().cond; c.terms.splice(i, 1); c.ops.splice(i ? i - 1 : 0, 1); }, true);
  }));
  p.querySelectorAll('[data-up-term]').forEach((b) => b.addEventListener('click', () => {
    const i = +b.dataset.upTerm;
    change(() => { moveTerm(target().cond, i, i - 1); }, true);
  }));
  // Glisser-deposer des conditions, par la poignee seulement : un cadre
  // entierement « draggable » genait la selection dans ses listes.
  let dragTerm = -1;
  p.querySelectorAll('.term .grip').forEach((g) => {
    const box = g.closest('.term');
    g.addEventListener('mousedown', () => { if (editable) box.draggable = true; });
    box.addEventListener('dragstart', (e) => {
      dragTerm = +box.dataset.tbox; e.dataTransfer.setData('text/plain', 'term:' + dragTerm); e.dataTransfer.effectAllowed = 'move';
    });
    box.addEventListener('dragend', () => { box.draggable = false; dragTerm = -1; p.querySelectorAll('.term.over').forEach((x) => x.classList.remove('over')); });
  });
  p.querySelectorAll('.term').forEach((box) => {
    box.addEventListener('dragover', (e) => { if (dragTerm >= 0) { e.preventDefault(); box.classList.add('over'); } });
    box.addEventListener('dragleave', () => box.classList.remove('over'));
    box.addEventListener('drop', (e) => {
      if (dragTerm < 0) return;
      e.preventDefault(); e.stopPropagation();
      const from = dragTerm, to = +box.dataset.tbox;
      change(() => { moveTerm(target().cond, from, to); }, true);
    });
  });
  if ($('p-add-term')) $('p-add-term').onclick = () => change(() => {
    const c = target().cond;
    c.terms.push({ k: 'entree', id: (ENTREES[0] || {id:1}).id, v: 1 });
    c.ops.push('et');
  }, true);
  if ($('p-del')) $('p-del').onclick = removeSel;
  if ($('p-dup')) $('p-dup').onclick = () => {
    const r = find(sel); pushUndo();
    const clone = S.reId(JSON.parse(JSON.stringify(r.node)));
    r.list.splice(r.index + 1, 0, clone); sel = clone.id; setDirty(true); refresh(true);
  };
}

function refresh(props) {
  renderCanvas(); renderCode(); markSel(); if (props) renderProps();
  // Interrupteur « Actif » de la barre du haut : meme valeur que le reglage
  // du bloc Depart, place la ou on le cherche.
  $('actif-top').checked = doc.actif;
  $('actif-top').disabled = !editable && !modeTexte();
  $('actif-top').parentNode.classList.toggle('off', !doc.actif);
  majNomSlot();
}

// ── Insertion, deplacement, suppression ────────────────────────────────
// Toutes les listes situees, a n'importe quelle profondeur, dans un bloc
// « En parallele ».
function parallelLists(list, acc, dedans) {
  list = list || doc.body; acc = acc || [];
  list.forEach((n) => {
    const d = dedans || n.t === 'parallele';
    S.childKeys(n).forEach((k) => { if (d) acc.push(n[k]); parallelLists(n[k], acc, d); });
  });
  return acc;
}
function contientParallele(n) {
  return n.t === 'parallele' || S.childKeys(n).some((k) => n[k].some(contientParallele));
}
function insertAt(lid, idx, what) {
  if (!editable) return;
  const list = listById(lid); if (!list || !what) return;
  // Pas de bloc « En parallele » dans un autre : le module ne sait pas les
  // imbriquer (une seule branche de repli par machine, voir ScriptVm FORK).
  const versParallele = parallelLists().indexOf(list) >= 0;
  const apporteParallele = what === 'new:parallele'
    || (what.indexOf('move:') === 0 && find(what.slice(5)) && contientParallele(find(what.slice(5)).node));
  if (versParallele && apporteParallele) {
    dire('Un bloc « En parallèle » ne peut pas en contenir un autre.', 'ko');
    return;
  }
  if (what.indexOf('new:') === 0) {
    pushUndo();
    const autre = SLOTS.find((x) => x.i !== slot() && x.utilise && x.actif) || SLOTS.find((x) => x.i !== slot()) || { i: 0 };
    const n = S.make(what.slice(4), { zoneId: (ZONES[0] || {}).id, inputId: (ENTREES[0] || {}).id, code: (BIBLIO[0] || {}).code,
                                      scriptNo: autre.i + 1 });
    list.splice(idx, 0, n); sel = n.id;
  } else if (what.indexOf('move:') === 0) {
    const id = what.slice(5), r = find(id); if (!r) return;
    if (listsOf(r.node).indexOf(list) >= 0) { dire('Un bloc ne peut pas être déplacé à l’intérieur de lui-même.', 'ko'); return; }
    pushUndo();
    r.list.splice(r.index, 1);
    if (r.list === list && r.index < idx) idx--;
    list.splice(idx, 0, r.node); sel = id;
  } else return;
  setDirty(true);
  refresh(true);
}
function removeSel() {
  if (!editable || !sel || sel === 'start') return;
  const r = find(sel); if (!r) return;
  pushUndo(); r.list.splice(r.index, 1); sel = null; setDirty(true); refresh(true);
}

let menuEl = null;
function closeMenu() { if (menuEl) { menuEl.remove(); menuEl = null; } }
function openMenu(btn) {
  closeMenu();
  const r = btn.getBoundingClientRect();
  menuEl = document.createElement('div'); menuEl.className = 'menu'; menuEl.setAttribute('role', 'menu');
  menuEl.innerHTML = Object.keys(S.TYPES).map((k) => {
    const T = S.TYPES[k];
    return '<button role="menuitem" class="cat-' + T.cat + '" data-t="' + k + '"><span class="ico">' + ico(T.icon) + '</span>' + esc(T.label) + '</button>';
  }).join('');
  document.body.appendChild(menuEl);
  menuEl.style.left = Math.max(8, Math.min(r.left - 100, window.innerWidth - 228)) + 'px';
  menuEl.style.top = Math.max(8, Math.min(r.bottom + 6, window.innerHeight - menuEl.offsetHeight - 8)) + 'px';
  menuEl.querySelector('button').focus();
  menuEl.onclick = (e) => { const b = e.target.closest('[data-t]'); if (b) { closeMenu(); insertAt(btn.dataset.l, +btn.dataset.i, 'new:' + b.dataset.t); } };
}

// ── Palette ────────────────────────────────────────────────────────────
$('palette').innerHTML = PALETTE.map(([g, ks]) => '<div class="pal-group"><h3>' + g + '</h3>' + ks.map((k) => {
  const T = S.TYPES[k];
  return '<div class="pal cat-' + T.cat + '" draggable="true" data-new="' + k + '"><span class="ico">' + ico(T.icon)
    + '</span><span><b>' + esc(T.label) + '</b><span>' + esc(T.desc) + '</span></span></div>';
}).join('') + '</div>').join('');
$('palette').addEventListener('dragstart', (e) => {
  const p = e.target.closest('[data-new]'); if (!p || !editable) { e.preventDefault(); return; }
  e.dataTransfer.setData('text/plain', 'new:' + p.dataset.new); e.dataTransfer.effectAllowed = 'copy';
  document.body.classList.add('dragging');
});

// ── Canevas : selection, glisser-deposer, zoom ─────────────────────────
const world = $('world'), canvas = $('canvas');
function choisir(id) { sel = id; markSel(); renderProps(); }
world.addEventListener('click', (e) => {
  const del = e.target.closest('.del');
  if (del) {
    e.stopPropagation();
    sel = del.dataset.del; removeSel();
    dire('Bloc supprimé. « Annuler » ou Ctrl+Z pour le rétablir.', '');
    return;
  }
  const plus = e.target.closest('.plus');
  if (plus) { e.stopPropagation(); openMenu(plus); return; }
  const n = e.target.closest('[data-id]');
  if (n) choisir(n.dataset.id);
});
world.addEventListener('keydown', (e) => {
  const n = e.target.closest('[data-id]');
  if (n && (e.key === 'Enter' || e.key === ' ')) { e.preventDefault(); choisir(n.dataset.id); }
});
world.addEventListener('dragstart', (e) => {
  const n = e.target.closest('[data-id]');
  if (!editable || !n || n.dataset.id === 'start') { e.preventDefault(); return; }
  e.dataTransfer.setData('text/plain', 'move:' + n.dataset.id); e.dataTransfer.effectAllowed = 'move';
  document.body.classList.add('dragging');
});
document.addEventListener('dragend', () => {
  document.body.classList.remove('dragging');
  document.querySelectorAll('.plus.over').forEach((p) => p.classList.remove('over'));
});
world.addEventListener('dragover', (e) => { const p = e.target.closest('.plus'); if (p) { e.preventDefault(); p.classList.add('over'); } });
world.addEventListener('dragleave', (e) => { const p = e.target.closest('.plus'); if (p) p.classList.remove('over'); });
world.addEventListener('drop', (e) => {
  const p = e.target.closest('.plus'); if (!p) return;
  e.preventDefault(); document.body.classList.remove('dragging');
  insertAt(p.dataset.l, +p.dataset.i, e.dataTransfer.getData('text/plain'));
});

let view = { x: 0, y: 0, s: 1 };
function applyView() { world.style.transform = 'translate(calc(-50% + ' + view.x + 'px),' + view.y + 'px) scale(' + view.s + ')'; }
function fit() { view = { x: 0, y: 0, s: Math.min(1, (canvas.clientWidth - 24) / Math.max(1, world.offsetWidth)) }; applyView(); }
// Molette = defiler (un long script depasse vers le bas), Ctrl+molette =
// zoom : la convention des editeurs de schemas.
canvas.addEventListener('wheel', (e) => {
  e.preventDefault();
  if (e.ctrlKey || e.metaKey) view.s = Math.min(1.6, Math.max(0.3, view.s * (e.deltaY < 0 ? 1.1 : 0.9)));
  else { view.y -= e.deltaY; view.x -= e.deltaX; }
  applyView();
}, { passive: false });
let pan = null;
canvas.addEventListener('pointerdown', (e) => {
  if (e.target.closest('[data-id],.plus,.zoom,.cover,button,a')) return;
  pan = { x: e.clientX - view.x, y: e.clientY - view.y }; canvas.classList.add('panning'); canvas.setPointerCapture(e.pointerId);
});
canvas.addEventListener('pointermove', (e) => { if (pan) { view.x = e.clientX - pan.x; view.y = e.clientY - pan.y; applyView(); } });
canvas.addEventListener('pointerup', () => { pan = null; canvas.classList.remove('panning'); });
$('z-in').onclick = () => { view.s = Math.min(1.6, view.s * 1.15); applyView(); };
$('z-out').onclick = () => { view.s = Math.max(0.3, view.s / 1.15); applyView(); };
$('z-fit').onclick = fit;
window.addEventListener('resize', fit);
$('code').addEventListener('click', (e) => { const l = e.target.closest('.cl'); if (l && l.dataset.id) choisir(l.dataset.id); });

// Onglets de droite. Passer d'un onglet a l'autre ne perd rien : le
// contenu de Phrases / Variables n'est prepare qu'a la premiere ouverture,
// puis apres Enregistrer, Annuler ou Relire.
let phrPret = false, varPret = false;
async function tab(which) {
  if (document.querySelector('.center').classList.contains('replie')) replier(false);
  ['run', 'phr', 'var'].forEach((k) => {
    $('t-' + k).setAttribute('aria-selected', which === k);
    $('p-' + k).hidden = which !== k;
  });
  if (which === 'phr' && !phrPret) { biblioAvant = JSON.stringify(BIBLIO); rendrePhrases(); direPhrases(''); phrPret = true; phrasesModifiees(false); }
  if (which === 'var' && !varPret) { await lireGlobales(); rendreVariables(); direVariables(''); varPret = true; variablesModifiees(false); }
  if (which !== 'run') lireSources();
}
$('t-run').onclick = () => tab('run');
// Replier la zone du bas : preference de ce navigateur seulement.
function replier(oui) {
  document.querySelector('.center').classList.toggle('replie', oui);
  $('b-replier').setAttribute('aria-label', oui ? 'Déplier la zone du bas' : 'Replier la zone du bas');
  $('b-replier').setAttribute('aria-expanded', !oui);
  try { localStorage.setItem('aqualook-bas-replie', oui ? '1' : '0'); } catch (e) {}
  requestAnimationFrame(fit);
}
$('b-replier').onclick = () => replier(!document.querySelector('.center').classList.contains('replie'));
try { if (localStorage.getItem('aqualook-bas-replie') === '1') replier(true); } catch (e) {}
// Volets lateraux (palette, reglages) : meme principe, preference par volet.
function volet(cote, oui) {
  const nom = cote === 'pal' ? 'la palette de blocs' : 'les réglages du bloc';
  document.querySelector('.main').classList.toggle(cote + '-replie', oui);
  $('b-volet-' + cote).setAttribute('aria-label', (oui ? 'Déplier ' : 'Replier ') + nom);
  $('b-volet-' + cote).setAttribute('aria-expanded', !oui);
  try { localStorage.setItem('aqualook-' + cote + '-replie', oui ? '1' : '0'); } catch (e) {}
  requestAnimationFrame(fit);
}
// Bandeau du haut : retour a la vue generale du panneau de droite (aide,
// mot de passe, effacer), quel que soit le bloc choisi ; deplie le volet.
$('b-reglages').onclick = () => {
  if (document.querySelector('.main').classList.contains('props-replie')) volet('props', false);
  choisir(null);
};
['pal', 'props'].forEach((c) => {
  $('b-volet-' + c).onclick = () => volet(c, !document.querySelector('.main').classList.contains(c + '-replie'));
  try { if (localStorage.getItem('aqualook-' + c + '-replie') === '1') volet(c, true); } catch (e) {}
});
$('t-phr').onclick = () => tab('phr');
$('t-var').onclick = () => tab('var');

// ── Essai a blanc ──────────────────────────────────────────────────────
// Parcourt le SCHEMA ici, sans le module : montre l'ordre des blocs et le
// chemin pris aux conditions. Les durees ne sont pas reellement attendues.
let running = false, stopReq = false;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
function log(t) { const d = document.createElement('div'); d.textContent = t; $('log').appendChild(d); $('log').scrollTop = 1e9; }
async function flash(id) {
  const e = document.querySelector('[data-id="' + id + '"]');
  document.querySelectorAll('.cl').forEach((l) => l.classList.toggle('hl', l.dataset.id === id));
  if (e) { e.classList.remove('run'); void e.offsetWidth; e.classList.add('run'); }
  await sleep(650);
}
function renderSim() {
  $('simins').innerHTML = ENTREES.length
    ? ENTREES.map((e) => '<label><input type="checkbox" id="sim' + e.id + '"> ' + esc(e.nom) + ' actif</label>').join('')
    : '<span style="color:var(--muted)">Aucune entrée déclarée.</span>';
}
async function essai() {
  if (running) { stopReq = true; return; }
  // L'essai parcourt le schema : en mode texte, il ne correspondrait pas a
  // ce que le module recevra.
  if (!editable) { dire('Essai à blanc indisponible : ce script ne se dessine pas, l’essai ne refléterait pas son texte.', 'ko'); return; }
  running = true; stopReq = false; tab('run'); $('log').innerHTML = '';
  $('b-essai').lastChild.textContent = 'Arrêter l’essai';
  const active = new Set();
  // Variables : on part des valeurs lues sur le module, que l'essai modifie
  // sans jamais les renvoyer.
  const gsim = {};
  GLOBALES.forEach((g) => { gsim[g.i] = g.valeur; });
  const gval = (i) => gsim[i] || 0;
  const cmpOk = (a, c, b) => ({ '==': a === b, '!=': a !== b, '<': a < b, '>': a > b, '<=': a <= b, '>=': a >= b })[c];
  const evalTerm = (t) => t.k === 'globale' ? cmpOk(gval(t.id), t.cmp, t.v) : (t.k === 'entree' ? ($('sim' + t.id) && $('sim' + t.id).checked ? 1 : 0) : (active.has(t.id) ? 1 : 0)) === t.v;
  const evalCond = (c) => S.condEval(c, evalTerm);
  log('Départ : ' + trigText()); await flash('start');
  const fond = [];   // branches des blocs « ou », que la fin du script attend
  async function exec(list) {
    for (const n of list) {
      if (stopReq) return;
      await flash(n.id);
      switch (n.t) {
        case 'arroser':
          if (n.attente) log('Arrose ' + zoneNom(n.zone) + ' pendant ' + S.duree(n.sec) + ', puis continue');
          else { active.add(n.zone); log('Ouvre ' + zoneNom(n.zone) + ' pour ' + S.duree(n.sec) + ' et continue sans attendre'); }
          break;
        case 'arreter': active.delete(n.zone); log('Ferme ' + zoneNom(n.zone)); break;
        case 'suspendre': log('Met en pause ' + zoneNom(n.zone)); break;
        case 'reprendre': log('Relance ' + zoneNom(n.zone)); break;
        case 'attendre': log('Attend ' + S.duree(n.sec)); break;
        case 'notifier': log('Notification sur GSM : « ' + phraseNom(n.code) + ' »' + (n.gvar ? ' -- ' + globaleNom(n.gvar) + ' = ' + gval(n.gvar) : '')); break;
        case 'message': log('Journal : « ' + phraseNom(n.code) + ' »' + (n.gvar ? ' -- ' + globaleNom(n.gvar) + ' = ' + gval(n.gvar) : '')); break;
        case 'lancer': log('Lance le script ' + scriptNom(n.script) + ', qui tourne en parallèle'); break;
        case 'variable':
          gsim[n.g] = n.op === '=' ? n.v : gval(n.g) + (n.op === '-' ? -n.v : n.v);
          log(globaleNom(n.g) + ' vaut maintenant ' + gsim[n.g] + ' (essai : rien n’est envoyé)');
          break;
        case 'parallele':
          log('Les deux branches partent ensemble');
          if (n.mode === 'ou') {
            // L'autre branche continue ; le script ne finit qu'avec elle.
            const deux = [exec(n.a), exec(n.b)];
            fond.push(...deux);
            await Promise.race(deux);
            log('Une branche est finie : la suite reprend, l’autre continue');
          } else {
            await Promise.all([exec(n.a), exec(n.b)]);
            log('Les deux branches sont finies : la suite reprend');
          }
          break;
        case 'si': { const r = evalCond(n.cond); log(condLabel(n.cond) + ' ? ' + (r ? 'oui' : 'non')); await exec(r ? n.oui : n.non); break; }
        case 'repeter': for (let k = 1; k <= n.n && !stopReq; k++) { log('Tour ' + k + ' sur ' + n.n); await exec(n.body); } break;
        case 'tantque': {
          let k = 0;
          while (!stopReq && evalCond(n.cond) && k < 3) { k++; log(condLabel(n.cond) + ' : tour ' + k); await exec(n.body); }
          log(k === 3 ? 'Essai limité à 3 tours de boucle' : 'Condition fausse : sortie de la boucle'); break;
        }
      }
    }
  }
  await exec(doc.body);
  if (fond.length) { log('Attend les branches encore en cours'); await Promise.all(fond); }
  log(stopReq ? 'Essai interrompu' : 'Fin du script');
  running = false; $('b-essai').lastChild.textContent = 'Essai à blanc'; markSel();
}
$('b-essai').onclick = essai;

// ── Module : lecture ───────────────────────────────────────────────────
function dire(texte, etat) { const m = $('msg'); m.textContent = texte; m.className = etat || ''; }
const slot = () => parseInt($('slot').value, 10);

async function lireModule() {
  try {
    const st = await (await fetch('/api/zonesConfig')).json();
    ZONES = (st.zones || []).map((z, i) => ({ id: z.id || (i + 1), nom: z.name || ('Zone ' + (i + 1)) }));
  } catch (e) { ZONES = []; }
  try {
    const topo = await (await fetch('/api/relay/topology')).json();
    ENTREES = (topo.assignments || []).filter((a) => a.input).map((a) => ({ id: a.id, nom: a.roleName }));
  } catch (e) { ENTREES = []; }
  try {
    const r = await fetch('/api/script-messages');
    if (r.ok) {
      const d = await r.json();
      BIBLIO = (d.entries || []).map((o) => ({ id: o.code, code: o.code, texte: o.texte }));
      BIBLIO_MAX = d.max || BIBLIO_MAX;
      BIBLIO_LEN = d.lenMax || BIBLIO_LEN;
    }
  } catch (e) { BIBLIO = []; }
  await lireGlobales();
  renderSim();
}
async function lireGlobales() {
  if (CLOUD) { GLOBALES = (MIROIR.scripts || {}).variables || []; return; }
  try {
    const r = await fetch('/api/script-globals');
    GLOBALES = r.ok ? ((await r.json()).vars || []) : [];
  } catch (e) { GLOBALES = []; }
}
async function listeSlots() {
  const d = await (await fetch('/api/scripts')).json();
  TAILLE_MAX = d.tailleMax || TAILLE_MAX;
  SLOTS = d.scripts || [];
  SIMULT = d.simultanes || 2;
  majModule(d.notifications, d.productLine);
  rendreSlots();
}
// Etat des notifications et gamme, relus avec la liste des scripts.
function majModule(notif, gamme) {
  NOTIF = typeof notif === 'boolean' ? notif : null;
  // La marque AquaLook est deja a gauche : ne garder que « Pro ».
  gamme = (gamme || '').replace(/^aqualook\s*/i, '');
  $('gamme').textContent = gamme; $('gamme').hidden = !gamme;
  $('ph-notif').hidden = NOTIF !== false;
  const p = $('palette').querySelector('[data-new="notifier"] span span');
  if (p) {
    p.textContent = NOTIF === false ? 'Notifications désactivées sur le module' : S.TYPES.notifier.desc;
    p.classList.toggle('pal-off', NOTIF === false);
  }
}
// Liste des emplacements. L'espace en ligne remplace vueSlot et suffixeSlot
// pour montrer la commande en attente plutot que le miroir.
let vueSlot = (s) => s;
let suffixeSlot = () => '';
function libelleSlot(s, v) {
  v = v || vueSlot(s);
  return (s.i + 1) + ' — ' + (!v.utilise ? 'vide' : (v.nom + (v.actif ? '' : ' — inactif'))) + suffixeSlot(s.i);
}
function rendreSlots() {
  const garde = $('slot').value;
  $('slot').innerHTML = SLOTS.map((s) => '<option value="' + s.i + '">' + esc(libelleSlot(s)) + '</option>').join('');
  if (garde !== '') $('slot').value = garde;
}
// L'emplacement ouvert montre le nom saisi tout de suite, sans attendre
// l'enregistrement : c'est ce nom-la (le champ, pas doc.nom) qui partira.
function majNomSlot() {
  const o = $('slot').selectedOptions[0]; if (!o) return;
  const s = SLOTS.find((x) => x.i === slot()) || { i: slot(), utilise: false };
  const nom = $('nom').value;
  o.textContent = libelleSlot(s, { utilise: vueSlot(s).utilise || !!nom, nom: nom || 'Sans nom', actif: doc.actif });
}

function couvrir(titre, texte) {
  $('cover-titre').textContent = titre;
  $('cover-texte').textContent = texte;
  $('cover').hidden = !titre;
}

async function charger() {
  const i = slot();
  sel = null; undo = []; $('b-undo').disabled = true; arret = null;
  editable = true; lecture = null; couvrir('');
  const d = await (await fetch('/api/script-one?i=' + i)).json();
  if (!d.ok) {
    doc = { nom: '', actif: true, trigger: { t: 0, target: 0 }, body: [] };
    $('nom').value = '';
    setDirty(false); refresh(true); fit();
    dire('Emplacement ' + (i + 1) + ' vide : construisez le script à partir du bloc « Départ ».', '');
    return;
  }
  doc = { nom: d.nom, actif: !!d.actif, trigger: { t: d.declencheur, target: d.cible || d.entree || 0 }, body: [] };
  $('nom').value = d.nom;
  const r = await fetch('/api/script-source?i=' + i);
  let msg = '', etat = 'ok';
  if (r.status !== 200) {
    editable = false; lecture = { texte: '', octets: d.code.length };
    couvrir('Texte source absent de la carte SD',
      'Ce script tourne sur le module, mais son texte n’a pas été retrouvé sur la carte SD : il ne peut pas être dessiné. '
      + 'Vous pouvez le réécrire ici, en schéma ou avec « Modifier le texte ».');
    msg = 'Chargé : ' + d.code.length + ' octets, sans texte source.'; etat = 'ko';
  } else {
    const texte = await r.text();
    try {
      doc.body = S.parse(texte);
      msg = 'Chargé : ' + d.code.length + ' octets.';
      arret = blocDuDernierArret(d);
    } catch (e) {
      if (!(e instanceof S.Unsupported)) throw e;
      editable = false; lecture = { texte: texte, octets: d.code.length };
      couvrir('Ce script s’écrit en mode texte',
        'Il utilise une tournure que le schéma ne sait pas dessiner (ligne ' + e.line + ' : ' + e.message + '). '
        + 'Il reste entièrement modifiable avec « Modifier le texte ». Repartir d’un schéma vide remplacera le script à l’enregistrement.');
      msg = 'Chargé en lecture : ce script ne se dessine pas.'; etat = 'ko';
    }
  }
  if (d.dernierArret) {
    msg += ' Dernier arrêt : ' + d.dernierArret + (d.dernierArretDetail ? ' — ' + d.dernierArretDetail : '')
      + (arret ? ' (bloc entouré en rouge).' : '.');
    etat = 'err';
  }
  setDirty(false);
  refresh(true);
  if (modeTexte()) { sel = 'start'; markSel(); renderProps(); }
  else if (!editable) $('props').innerHTML = '<div class="empty">Lecture seule.</div>';
  requestAnimationFrame(fit);
  dire(msg, etat);
}

// Le compilateur est deterministe, donc le texte regenere redonne le
// bytecode qui a tourne --
// sauf si sa taille differe, auquel cas on ne montre rien plutot qu'un
// bloc faux.
function blocDuDernierArret(d) {
  if (!d.dernierArretDetail || typeof d.dernierArretPc !== 'number') return null;
  const g = S.generate(doc.body, NAMES);
  let code;
  try { code = AquaScript.compile(g.text); } catch (e) { return null; }
  if (code.length !== d.code.length || !code.lineMap) return null;
  let best = null;
  code.lineMap.forEach((pt) => { if (pt.pc <= d.dernierArretPc && (!best || pt.pc > best.pc)) best = pt; });
  return best && g.lines[best.line - 1] ? g.lines[best.line - 1].id : null;
}

// ── Module : ecriture (signee) ───────────────────────────────────────
// Le secret est aussi le mot de passe Web (D016 lot F) : il n'est plus garde
// durablement (localStorage, lisible par tout script de la page et survivant
// a la fermeture), seulement le temps de l'onglet. L'ancienne copie durable
// est effacee au passage.
try { localStorage.removeItem('aqualook-secret'); } catch (e) {}
// Saisie masquee sur le module (session.js) ; prompt() en clair ne reste
// qu'en repli, hors module.
function demanderSecret(titre, detail) {
  if (window.AquaSession && AquaSession.promptSecret) return AquaSession.promptSecret(titre, detail);
  return Promise.resolve(prompt(titre + (detail ? ' (' + detail + ')' : '') + ' :'));
}
async function secret() {
  let s = null;
  try { s = sessionStorage.getItem('aqualook-secret'); } catch (e) {}
  if (!s) {
    s = await demanderSecret('Mot de passe Web du module', '12 caractères minimum');
    if (s) { try { sessionStorage.setItem('aqualook-secret', s); } catch (e) {} if (!sel) renderProps(); }
  }
  return s;
}
function secretRetenu() {
  try { return !!sessionStorage.getItem('aqualook-secret'); } catch (e) { return false; }
}
function oublierSecret() {
  try { sessionStorage.removeItem('aqualook-secret'); } catch (e) {}
  if (!sel) renderProps();
  dire('Mot de passe effacé de cette page (il n’est pas modifié sur le module). Il vous sera demandé au prochain enregistrement.', '');
}
async function prochainNonce() {
  const d = await (await fetch('/api/auth/state')).json();
  NONCE = Math.max(NONCE, d.nonce || 0) + 1;
  return { nonce: NONCE, configure: d.configure };
}
async function signer(canonique) {
  const s = await secret();
  return s ? AquaHmac.hmacSha256Hex(s, canonique) : null;
}
async function definirSecret() {
  const actuel = await demanderSecret('Mot de passe ACTUEL', 'laisser vide si le module n’en a pas encore');
  if (actuel === null) return;
  const nouveau = await demanderSecret('NOUVEAU mot de passe', '12 caractères minimum');
  if (!nouveau) return;
  let d;
  if (window.AquaSession) {
    // Module : le nouveau mot de passe part chiffre par l'actuel (session.js).
    d = await AquaSession.changePassword(actuel, nouveau);
  } else {
    const r = await fetch('/api/auth-secret', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ actuel: actuel, nouveau: nouveau })
    });
    d = await r.json();
  }
  if (d.ok) {
    try { sessionStorage.setItem('aqualook-secret', nouveau); } catch (e) {}
    if (!sel) renderProps();
    dire('Mot de passe enregistré sur le module et retenu pour cet onglet.', 'ok');
  } else dire('Refusé : ' + (d.error || ''), 'ko');
}

// Texte a envoyer : celui du schema, ou en mode texte celui garde tel quel
// (null si le schema est en lecture sans texte : source absente de la SD).
function texteAEnvoyer() {
  if (editable) return S.generate(doc.body, NAMES).text;
  return lecture && lecture.texte ? lecture.texte : null;
}
async function enregistrer() {
  const texte = texteAEnvoyer();
  if (texte === null) { dire('Aucun texte à enregistrer : écrivez-le avec « Modifier le texte », ou repartez d’un schéma vide.', 'ko'); return; }
  const g = { text: texte };
  let code;
  try { code = AquaScript.compile(g.text); }
  catch (e) { dire('Compilation impossible : ' + e.message + (e.line ? ' (ligne ' + e.line + ')' : ''), 'err'); return; }
  if (code.length > TAILLE_MAX) { dire('Script trop long : ' + code.length + ' octets pour ' + TAILLE_MAX + ' au plus. Retirez des blocs.', 'err'); return; }
  if (doc.trigger.t && !doc.trigger.target) { dire('Choisissez ce qui déclenche le script dans le bloc « Départ ».', 'ko'); choisir('start'); return; }
  const i = slot();
  const etat = await prochainNonce();
  if (!etat.configure) { dire('Le module n’a pas encore de mot de passe : « ← Réglages du script » (panneau de droite), puis « Changer le mot de passe… ».', 'ko'); return; }
  const hex = code.map((b) => b.toString(16).padStart(2, '0')).join('');
  const sig = await signer('script-save|' + i + '|' + etat.nonce + '|' + hex);
  if (!sig) { dire('Mot de passe du module requis pour enregistrer.', 'ko'); return; }
  const r = await fetch('/api/script-save', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      i: i, nom: $('nom').value || 'Sans nom', actif: doc.actif,
      declencheur: doc.trigger.t, cible: doc.trigger.target || 0,
      code: code, source: g.text, nonce: etat.nonce, sig: sig
    })
  });
  const d = await r.json();
  if (d.ok) {
    setDirty(false); arret = null; markSel();
    dire('Enregistré : ' + d.octets + ' octets acceptés par le module.', 'ok');
    await listeSlots();
  } else {
    dire('Refusé par le module : ' + (d.error || 'raison inconnue') + '. Le script précédent est intact.', 'ko');
  }
}

async function lancer() {
  if (dirty && !confirm('Le schéma n’est pas enregistré : le module lancera la version enregistrée. Continuer ?')) return;
  const i = slot();
  const etat = await prochainNonce();
  const sig = await signer('script-run|' + i + '|' + etat.nonce);
  if (!sig) { dire('Mot de passe du module requis pour lancer.', 'ko'); return; }
  const r = await fetch('/api/script-run', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ i: i, nonce: etat.nonce, sig: sig })
  });
  const d = await r.json();
  dire(d.ok ? 'Lancé. Le journal du module dira ce qu’il a fait.' : ('Non lancé : ' + (d.error || '')), d.ok ? 'ok' : 'ko');
}

async function effacer() {
  const i = slot();
  if (!confirm('Effacer le script ' + (i + 1) + ' du module ?')) return;
  const etat = await prochainNonce();
  const sig = await signer('script-erase|' + i + '|' + etat.nonce);
  if (!sig) { dire('Mot de passe du module requis pour effacer.', 'ko'); return; }
  const r = await fetch('/api/script-erase', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ i: i, nonce: etat.nonce, sig: sig })
  });
  const d = await r.json();
  await listeSlots();
  if (d.ok) await charger();
  dire(d.ok ? 'Effacé.' : ('Échec : ' + (d.error || '')), d.ok ? 'ok' : 'ko');
}

// ── Barre du haut et clavier ───────────────────────────────────────────
let slotAvant = null;
$('slot').addEventListener('focus', () => { slotAvant = $('slot').value; });
$('slot').addEventListener('change', () => {
  if (dirty && !confirm('Le schéma en cours n’est pas enregistré. Changer d’emplacement quand même ?')) {
    $('slot').value = slotAvant; return;
  }
  rendreSlots();   // l'emplacement quitte reprend son nom enregistre
  history.replaceState(null, '', '?' + (CLOUD ? 'module=' + encodeURIComponent(CLOUD) + '&' : '') + 'i=' + slot());
  charger().catch(() => dire('Module injoignable.', 'err'));
});
$('nom').addEventListener('input', () => { doc.nom = $('nom').value; setDirty(true); majNomSlot(); });
$('actif-top').addEventListener('change', () => {
  pushUndo(); doc.actif = $('actif-top').checked; setDirty(true); refresh(true);
  dire(doc.actif ? 'Script actif : il partira sur son déclencheur une fois enregistré.'
                 : 'Script inactif : il ne partira plus, même sur « Lancer maintenant », une fois enregistré.', '');
});

// ── Ou servent phrases et variables (demande du 8 oct. 2026) ───────────
// Au survol d'une ligne, les scripts qui la citent ; avant d'effacer une
// phrase ou le nom d'une variable cites, une confirmation. Textes relus a
// l'ouverture des onglets et juste avant une confirmation ; l'emplacement
// ouvert compte pour son schema en cours, pas pour ce qui est enregistre.
// null = texte introuvable : l'emplacement est signale « non vérifié ».
let SOURCES = {}, sourcesLues = false;
async function lireSources() {
  const lu = {};
  for (const s of SLOTS) {
    const v = vueSlot(s);
    if (!v.utilise || s.i === slot()) continue;
    if (CLOUD) { lu[s.i] = typeof v.source === 'string' && v.source ? v.source : null; continue; }
    try { const r = await fetch('/api/script-source?i=' + s.i); lu[s.i] = r.status === 200 ? await r.text() : null; }
    catch (e) { lu[s.i] = null; }
  }
  SOURCES = lu; sourcesLues = true;
  majPuces();
}
// Codes de phrase et numeros de variable cites, commentaires retires : le
// texte genere y recopie les noms.
function references(texte) {
  const ph = new Set(), gv = new Set();
  String(texte || '').split('\n').forEach((l) => {
    l = l.replace(/#.*$/, '');
    for (const m of l.matchAll(/\b(?:message|notifier)\s+(\d+)/g)) if (+m[1]) ph.add(+m[1]);
    for (const m of l.matchAll(/\bg(\d+)\b/g)) gv.add(+m[1]);
  });
  return { ph: ph, gv: gv };
}
// Scripts qui citent la phrase (genre 'ph') ou la variable ('gv') n.
function utilisations(genre, n) {
  const oui = [], inconnus = [];
  SLOTS.forEach((s) => {
    const ouvert = s.i === slot();
    if (!ouvert && !vueSlot(s).utilise) return;
    const texte = ouvert ? texteAEnvoyer() : SOURCES[s.i];
    if (texte === undefined) return;
    if (texte === null) { inconnus.push(s.i + 1); return; }
    if (references(texte)[genre].has(n)) oui.push((s.i + 1) + ' — ' + (ouvert ? ($('nom').value || 'Sans nom') : vueSlot(s).nom));
  });
  return { oui: oui, inconnus: inconnus };
}
function texteUtilisations(u) {
  return (u.oui.length ? 'Utilisée dans : ' + u.oui.join(', ') : 'Utilisée dans aucun script')
    + (u.inconnus.length ? ' (texte introuvable, non vérifié : script ' + u.inconnus.join(', ') + ')' : '') + '.';
}
function survolUtilisations(liste, genre, numero) {
  liste.addEventListener('mouseover', (e) => {
    const tr = e.target.closest('tr'); if (!tr || !liste.contains(tr)) return;
    const n = numero(tr);
    tr.title = !n ? '' : sourcesLues ? texteUtilisations(utilisations(genre, n)) : 'Recherche des scripts qui l’utilisent…';
  });
}
const numPhrase = (tr) => { const c = tr.querySelector('input[data-b="code"]'); return c ? parseInt(c.value, 10) || 0 : 0; };
const numVariable = (tr) => { const c = tr.querySelector('input[data-gn]'); return c ? +c.dataset.gn : 0; };
function majPuces() {
  [['phrases-liste', 'ph', numPhrase], ['gv-liste', 'gv', numVariable]].forEach(([id, genre, numero]) => {
    $(id).querySelectorAll('[data-nb]').forEach((b) => {
      const n = numero(b.closest('tr'));
      if (!n || !sourcesLues) { b.textContent = '…'; b.className = 'nb'; return; }
      const k = utilisations(genre, n).oui.length;
      b.textContent = k; b.className = 'nb' + (k ? ' oui' : '');
    });
  });
}
function phrasesModifiees(v) { $('ph-enregistrer').classList.toggle('a-enregistrer', v); $('t-phr').classList.toggle('modifie', v); }
function variablesModifiees(v) { $('gv-enregistrer').classList.toggle('a-enregistrer', v); $('t-var').classList.toggle('modifie', v); }

// ── Bibliotheque de phrases (signee comme les scripts)
// Un message d'erreur reste ; les autres s'effacent au bout de 3 s.
function direTab(id, t, e) {
  const m = $(id); clearTimeout(m._t);
  m.textContent = t || ''; m.className = 'msgtab ' + (e || '');
  if (t && e !== 'ko') m._t = setTimeout(() => { m.classList.add('fond'); m._t = setTimeout(() => { m.textContent = ''; }, 400); }, 3000);
}
function direPhrases(t, e) { direTab('msgphrases', t, e); }
function rendrePhrases() {
  $('phrases-liste').innerHTML = BIBLIO.length ? BIBLIO.map((o, i) =>
    '<tr><td class="code"><input type="number" min="1" max="65535" data-i="' + i + '" data-b="code" value="' + o.code + '" aria-label="Code"></td>'
    + '<td class="u"><span class="nb" data-nb>…</span></td>'
    + '<td><input type="text" maxlength="' + BIBLIO_LEN + '" data-i="' + i + '" data-b="texte" value="' + esc(o.texte) + '" aria-label="Phrase"></td>'
    + '<td class="x"><button class="btn danger" data-del-phrase="' + i + '" title="Supprimer cette phrase">×</button></td></tr>').join('')
    : '<tr><td style="color:var(--muted)">Aucune phrase. « Ajouter une phrase » pour commencer.</td></tr>';
  majPuces();
}
function lirePhrases() {
  $('phrases-liste').querySelectorAll('input[data-b]').forEach((el) => {
    const o = BIBLIO[+el.dataset.i];
    if (el.dataset.b === 'code') o.code = parseInt(el.value, 10) || 0; else o.texte = el.value;
  });
}
// Premier code libre a partir de 900.
function codeLibre() { const pris = new Set(BIBLIO.map((o) => o.code)); let c = 900; while (pris.has(c) && c < 65535) c++; return c; }
let biblioAvant = null;
// Phrases citees dont la suppression a deja ete confirmee (bouton ×) : ne
// pas redemander a l'enregistrement.
let phrasesConfirmees = new Set();
const avertPhrase = (u) => 'Utilisée dans : ' + u.oui.join(', ')
  + '.\nCes blocs n’auront plus de texte : le journal et la notification ne montreront que le code.';
survolUtilisations($('phrases-liste'), 'ph', numPhrase);
$('phrases-liste').addEventListener('input', (e) => { phrasesModifiees(true); if (e.target.dataset.b === 'code') majPuces(); });
$('ph-fermer').onclick = () => {
  // Annuler : rien n'est parti vers le module, revenir a la liste lue.
  if (biblioAvant !== null) BIBLIO = JSON.parse(biblioAvant);
  phrasesConfirmees = new Set();
  rendrePhrases(); direPhrases('Modifications annulées.', ''); phrasesModifiees(false);
};
$('ph-ajouter').onclick = () => {
  lirePhrases();
  if (BIBLIO.length >= BIBLIO_MAX) { direPhrases(BIBLIO_MAX + ' phrases au maximum.', 'ko'); return; }
  const c = codeLibre();
  BIBLIO.push({ id: c, code: c, texte: '' });
  rendrePhrases(); phrasesModifiees(true);
  const champs = $('phrases-liste').querySelectorAll('input[data-b="texte"]');
  if (champs.length) champs[champs.length - 1].focus();
};
$('phrases-liste').addEventListener('click', async (e) => {
  const b = e.target.closest('[data-del-phrase]'); if (!b) return;
  lirePhrases();
  const o = BIBLIO[+b.dataset.delPhrase];
  await lireSources();
  const u = utilisations('ph', o.code);
  if (u.oui.length) {
    if (!confirm('Supprimer la phrase ' + o.code + ' « ' + o.texte + ' » ?\n' + avertPhrase(u))) return;
    phrasesConfirmees.add(o.code);
  }
  BIBLIO.splice(BIBLIO.indexOf(o), 1); rendrePhrases(); phrasesModifiees(true);
});
$('ph-enregistrer').onclick = async () => {
  lirePhrases();
  const vus = new Set();
  for (let k = 0; k < BIBLIO.length; k++) {
    const o = BIBLIO[k];
    if (!(o.code >= 1 && o.code <= 65535)) { direPhrases('Ligne ' + (k + 1) + ' : code hors 1–65535.', 'ko'); return; }
    if (vus.has(o.code)) { direPhrases('Code ' + o.code + ' en double.', 'ko'); return; }
    vus.add(o.code);
    const t = (o.texte || '').trim();
    if (!t) { direPhrases('Ligne ' + (k + 1) + ' : phrase vide.', 'ko'); return; }
    if (t.length > BIBLIO_LEN) { direPhrases('Ligne ' + (k + 1) + ' : plus de ' + BIBLIO_LEN + ' caractères.', 'ko'); return; }
    if (/[\t\r\n]/.test(t)) { direPhrases('Ligne ' + (k + 1) + ' : tabulation ou saut de ligne interdit.', 'ko'); return; }
    o.texte = t; o.id = o.code;
  }
  // Un code change ou une phrase videe puis retiree sans × : meme question.
  await lireSources();
  const perdues = (biblioAvant ? JSON.parse(biblioAvant) : []).filter((a) => !vus.has(a.code) && !phrasesConfirmees.has(a.code))
    .map((a) => ({ a: a, u: utilisations('ph', a.code) })).filter((x) => x.u.oui.length);
  if (perdues.length && !confirm(perdues.map((x) => 'La phrase ' + x.a.code + ' « ' + x.a.texte + ' » disparaît.\n' + avertPhrase(x.u)).join('\n\n')
    + '\n\nEnregistrer quand même ?')) return;
  if (CLOUD) { await envoyerPhrasesCloud(); return; }
  try {
    const etat = await prochainNonce();
    if (!etat.configure) { direPhrases('Le module n’a pas de secret : posez-en un (bouton « Secret ») avant d’enregistrer.', 'ko'); return; }
    // Forme canonique = les octets TSV exacts que le module ecrira.
    const corpus = BIBLIO.map((o) => o.code + '\t' + o.texte + '\n').join('');
    const sig = await signer('script-messages|' + etat.nonce + '|' + corpus);
    if (!sig) { direPhrases('Mot de passe du module requis pour enregistrer.', 'ko'); return; }
    const r = await fetch('/api/script-messages', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ entries: BIBLIO.map((o) => ({ code: o.code, texte: o.texte })), nonce: etat.nonce, sig: sig })
    });
    const d = await r.json().catch(() => ({}));
    if (d.ok) {
      biblioAvant = JSON.stringify(BIBLIO);
      phrasesConfirmees = new Set(); phrasesModifiees(false);
      direPhrases('Enregistré : ' + d.entrees + ' phrase(s) sur la carte SD.', 'ok');
      refresh(true);   // les blocs Notifier affichent les nouveaux textes
    } else {
      direPhrases('Refusé par le module : ' + (d.error || 'raison inconnue') + '. La bibliothèque précédente est intacte.', 'ko');
    }
  } catch (e) { direPhrases('Module injoignable : rien n’a été enregistré.', 'ko'); }
};
// ── Variables partagees ────────────────────────────────────────────────
function direVariables(t, e) { direTab('msgvariables', t, e); }
function rendreVariables() {
  $('gv-liste').innerHTML = Array.from({ length: S.GLOBAL_COUNT }, (_, k) => k + 1).map((i) => {
    const g = GLOBALES.find((o) => o.i === i) || { nom: '', valeur: 0 };
    return '<tr><td class="g">g' + i + '</td><td class="u"><span class="nb" data-nb>…</span></td>'
      + '<td><input type="text" maxlength="23" data-gn="' + i + '" value="' + esc(g.nom) + '"' + (CLOUD ? ' readonly' : '') + ' aria-label="Nom de g' + i + '"></td>'
      + '<td class="val">' + g.valeur + '</td>'
      + '<td class="nv"><input type="number" data-gv="' + i + '" placeholder="inchangée" aria-label="Nouvelle valeur de g' + i + '"></td></tr>';
  }).join('');
  majPuces();
}

survolUtilisations($('gv-liste'), 'gv', numVariable);
$('gv-liste').addEventListener('input', () => variablesModifiees(true));

$('gv-relire').onclick = async () => {
  // Garder les noms en cours de saisie : seules les valeurs sont relues.
  const saisis = {};
  $('gv-liste').querySelectorAll('input[data-gn]').forEach((el) => { saisis[el.dataset.gn] = el.value; });
  await lireGlobales(); rendreVariables();
  $('gv-liste').querySelectorAll('input[data-gn]').forEach((el) => { el.value = saisis[el.dataset.gn]; });
  direVariables('Valeurs relues sur le module.', 'ok');
};
$('gv-fermer').onclick = () => { rendreVariables(); direVariables('Modifications annulées.', ''); variablesModifiees(false); };
$('gv-enregistrer').onclick = async () => {
  const noms = [], vus = new Set(), fixe = [];
  for (let i = 1; i <= S.GLOBAL_COUNT; i++) {
    const nom = $('gv-liste').querySelector('input[data-gn="' + i + '"]').value.trim();
    if (new TextEncoder().encode(nom).length > 23) { direVariables('g' + i + ' : nom trop long (23 octets au plus).', 'ko'); return; }
    if (/[#|\u0000-\u001f]/.test(nom)) { direVariables('g' + i + ' : caractère interdit (# ou |).', 'ko'); return; }
    if (nom && vus.has(nom.toLowerCase())) { direVariables('Nom « ' + nom + ' » en double.', 'ko'); return; }
    if (nom) vus.add(nom.toLowerCase());
    noms.push(nom);
    const nv = $('gv-liste').querySelector('input[data-gv="' + i + '"]').value.trim();
    if (nv !== '') {
      if (!/^-?\d+$/.test(nv)) { direVariables('g' + i + ' : la valeur doit être un nombre entier.', 'ko'); return; }
      fixe.push({ i: i, v: entier(nv) });
    }
  }
  // Nom efface d'une variable citee : les blocs n'afficheront plus que gN.
  await lireSources();
  const effaces = noms.map((nom, k) => ({ i: k + 1, nom: nom, avant: globaleNom(k + 1) }))
    .filter((x) => !x.nom && x.avant !== 'g' + x.i).map((x) => Object.assign(x, { u: utilisations('gv', x.i) }))
    .filter((x) => x.u.oui.length);
  if (effaces.length && !confirm(effaces.map((x) => 'La variable g' + x.i + ' « ' + x.avant + ' » perd son nom.\nUtilisée dans : '
    + x.u.oui.join(', ') + '.\nLes blocs et le journal n’afficheront plus que g' + x.i + '.').join('\n\n')
    + '\n\nEnregistrer quand même ?')) return;
  try {
    const etat = await prochainNonce();
    if (!etat.configure) { direVariables('Le module n’a pas de secret : posez-en un (bouton « Secret ») avant d’enregistrer.', 'ko'); return; }
    // Forme canonique : voir WebManager::handleSaveScriptGlobals.
    const canon = 'script-globals|' + etat.nonce + '|' + noms.map((n) => n + '\n').join('') + '|'
      + fixe.map((f) => f.i + '=' + f.v + ';').join('');
    const sig = await signer(canon);
    if (!sig) { direVariables('Mot de passe du module requis pour enregistrer.', 'ko'); return; }
    const r = await fetch('/api/script-globals', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ noms: noms, fixe: fixe, nonce: etat.nonce, sig: sig })
    });
    const d = await r.json().catch(() => ({}));
    if (d.ok) {
      await lireGlobales(); rendreVariables(); variablesModifiees(false);
      direVariables('Enregistré : noms à jour' + (d.fixees ? ', ' + d.fixees + ' valeur(s) fixée(s).' : '.'), 'ok');
      refresh(true);   // les blocs affichent les nouveaux noms
    } else {
      direVariables('Refusé par le module : ' + (d.error || 'raison inconnue') + '. Les variables sont inchangées.', 'ko');
    }
  } catch (e) { direVariables('Module injoignable : rien n’a été enregistré.', 'ko'); }
};
$('b-save').onclick = enregistrer;
$('b-lancer').onclick = lancer;
$('b-undo').onclick = doUndo;
$('cover-vide').onclick = () => {
  pushUndo(); doc.body = []; editable = true; lecture = null; couvrir(''); setDirty(true); refresh(true);
  dire('Schéma vide : le texte actuel du module sera remplacé à l’enregistrement.', 'ko');
};

// ── Modifier le texte (remplace l'ancien editeur texte scripts.html) ────
// Le texte applique est compile, puis relu en schema. S'il ne se dessine
// pas, il est GARDE tel quel (mode texte : schema en lecture, Enregistrer
// envoie ce texte) plutot que perdu ou traduit « a peu pres ».
let texteOuvert = '';
function analyseTexte(t) {
  const r = { code: null, err: null, body: null, raison: null };
  try { r.code = AquaScript.compile(t); } catch (e) { r.err = e; return r; }
  try { r.body = S.parse(t); } catch (e) { if (!(e instanceof S.Unsupported)) throw e; r.raison = e; }
  return r;
}
function majEtatTexte() {
  const t = $('tx-src').value, r = analyseTexte(t);
  let h;
  if (r.err) h = '<span class="err">Ne compile pas' + (r.err.line ? ' (ligne ' + r.err.line + ')' : '') + ' : ' + esc(r.err.message) + '</span>';
  else if (r.code.length > TAILLE_MAX) h = '<span class="err">Trop long : ' + r.code.length + ' octets pour ' + TAILLE_MAX + ' au plus.</span>';
  else {
    h = '<span class="ok">Compile : ' + r.code.length + ' / ' + TAILLE_MAX + ' octets.</span> ';
    if (r.body) {
      // Commentaires et mise en forme sont regeneres a partir du schema :
      // le dire si le texte saisi en differe, plutot que de les effacer en douce.
      h += S.generate(r.body, NAMES).text.trim() === t.trim() ? '<span class="ok">Se dessine en schéma.</span>'
        : '<span class="ok">Se dessine en schéma</span> <span class="ko">(mise en forme et commentaires « # » seront réécrits).</span>';
    } else {
      h += '<span class="ko">Ne se dessine pas (ligne ' + r.raison.line + ' : ' + esc(r.raison.message)
        + ') : il sera gardé tel quel, schéma en lecture.</span>';
    }
  }
  $('tx-etat').innerHTML = h;
  return r;
}
function ouvrirTexte() {
  texteOuvert = !editable && lecture ? (lecture.texte || '') : S.generate(doc.body, NAMES).text;
  $('tx-src').value = texteOuvert;
  $('texte').hidden = false;
  majEtatTexte();
  $('tx-src').focus();
}
// « fin du script » est signale sur la ligne vide qui suit le dernier saut
// de ligne : remonter a la derniere ligne non vide, celle a corriger.
function selectionnerLigne(n) {
  const ta = $('tx-src'), lignes = ta.value.split('\n');
  if (!n) return;
  n = Math.min(n, lignes.length);
  while (n > 1 && !lignes[n - 1].trim()) n--;
  const debut = lignes.slice(0, n - 1).reduce((a, l) => a + l.length + 1, 0);
  ta.focus(); ta.setSelectionRange(debut, debut + lignes[n - 1].length);
}
function appliquerTexte() {
  const t = $('tx-src').value.replace(/\r\n/g, '\n');
  const r = majEtatTexte();
  if (r.err) { selectionnerLigne(r.err.line); return; }
  if (r.code.length > TAILLE_MAX) return;
  if (r.body) {
    // Schema -> schema : annulable. Depuis ou vers le mode texte, l'historique
    // d'annulation ne decrit plus le meme objet : il repart de zero.
    if (editable) pushUndo(); else { undo = []; $('b-undo').disabled = true; }
    doc.body = r.body; editable = true; lecture = null; couvrir('');
    dire('Texte appliqué et redessiné. Il part au module à l’enregistrement.', 'ok');
  } else {
    undo = []; $('b-undo').disabled = true;
    doc.body = []; editable = false; lecture = { texte: t, octets: r.code.length };
    couvrir('Ce script s’écrit en mode texte',
      'Il utilise une tournure que le schéma ne sait pas dessiner (ligne ' + r.raison.line + ' : ' + r.raison.message + '). '
      + 'Il est gardé tel quel et « Enregistrer » l’enverra au module. « Modifier le texte » pour y revenir.');
    dire('Texte appliqué en mode texte : il part au module à l’enregistrement.', 'ko');
  }
  sel = editable ? null : 'start'; setDirty(true); $('texte').hidden = true;
  refresh(true);
  requestAnimationFrame(fit);
}
$('b-modif-texte').onclick = ouvrirTexte;
$('cover-modif').onclick = ouvrirTexte;
$('tx-src').addEventListener('input', majEtatTexte);
$('tx-appliquer').onclick = appliquerTexte;
$('tx-fermer').onclick = () => {
  if ($('tx-src').value !== texteOuvert && !confirm('Fermer sans appliquer ? Les modifications du texte seront perdues.')) return;
  $('texte').hidden = true;
};
window.addEventListener('beforeunload', (e) => {
  if (dirty || $('t-phr').classList.contains('modifie') || $('t-var').classList.contains('modifie')) { e.preventDefault(); e.returnValue = ''; }
});
document.addEventListener('keydown', (e) => {
  const inField = /INPUT|SELECT|TEXTAREA/.test(document.activeElement.tagName);
  if (e.key === 'Escape' && !$('texte').hidden) { $('tx-fermer').click(); return; }
  if (e.key === 'Escape') { closeMenu(); choisir(null); }
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z' && !inField) { e.preventDefault(); doUndo(); }
  if ((e.key === 'Delete' || e.key === 'Backspace') && !inField && sel && sel !== 'start') { e.preventDefault(); removeSel(); }
});
document.addEventListener('pointerdown', (e) => { if (menuEl && !menuEl.contains(e.target) && !e.target.closest('.plus')) closeMenu(); });

// ── Mode espace en ligne (decision D014) ───────────────────────────────
// La meme page, servie par le serveur avec ?module=<id>. Lecture dans le
// miroir que le module remonte a chaque changement ; ecriture par une
// commande config.apply que le module appliquera -- ou refusera -- a sa
// prochaine synchronisation : il reste seul juge. Pas de signature ni de
// secret sur ce chemin, et rien ne se lance d'ici (pas de « Lancer »).
// Une modification envoyee mais pas encore appliquee est AFFICHEE comme
// telle, effacement compris : sinon rien ne dirait qu'elle est partie.
let MIROIR = {}, CMDS = [];
async function apiCloud(chemin, corps) {
  const r = await fetch(chemin, corps
    ? { method: 'POST', credentials: 'same-origin', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(corps) }
    : { credentials: 'same-origin' });
  const d = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(d.detail || ('erreur ' + r.status));
  return d;
}
// Derniere commande visant cette cible ('phrases' ou un emplacement) ; le
// serveur les rend de la plus recente a la plus ancienne.
function cmdPour(cible) {
  return CMDS.find((c) => {
    const k = c.command || {};
    return cible === 'phrases' ? Array.isArray(k.phrases)
      : (Array.isArray(k.scripts) && k.scripts[0] && k.scripts[0].i === cible);
  }) || null;
}
const enAttente = (c) => !!c && c.state === 'pending';
async function envoyerPhrasesCloud() {
  try {
    const d = await apiCloud('/app/module/phrases', { moduleId: CLOUD, phrases: BIBLIO.map((o) => ({ code: o.code, texte: o.texte })) });
    biblioAvant = JSON.stringify(BIBLIO);
    phrasesConfirmees = new Set(); phrasesModifiees(false);
    await lireModule();
    direPhrases('Envoyé : le module l’appliquera à sa prochaine synchronisation'
      + (d.remplace ? ', à la place de l’envoi précédent' : '') + '.', 'ok');
    refresh(true);
  } catch (e) { direPhrases('Non envoyé : ' + e.message + '.', 'ko'); }
}
if (CLOUD) {
  document.body.classList.add('cloud');
  document.querySelector('.brand').href = '/app.html';
  $('msg').textContent = 'Lecture de l’espace en ligne…';

  lireModule = async function () {
    const d = await apiCloud('/app/module?moduleId=' + encodeURIComponent(CLOUD));
    MIROIR = (d.config && d.config.payload) || {};
    CMDS = d.commandes || [];
    const sc = MIROIR.scripts || {};
    ZONES = (MIROIR.zones || []).map((z, i) => ({ id: z.id || (i + 1), nom: z.name || ('Zone ' + (i + 1)) }));
    ENTREES = (sc.entrees || []).map((e) => ({ id: e.id, nom: e.nom }));
    const c = cmdPour('phrases');
    // Phrases absentes du miroir = carte SD illisible : ne pas proposer de
    // remplacer le catalogue par une liste vide.
    const ph = sc.phrases;
    const liste = enAttente(c) ? c.command.phrases : (ph ? ph.entries || [] : []);
    BIBLIO = liste.map((o) => ({ id: o.code, code: o.code, texte: o.texte }));
    if (ph) { BIBLIO_MAX = ph.max || BIBLIO_MAX; BIBLIO_LEN = ph.lenMax || BIBLIO_LEN; }
    $('t-phr').disabled = !ph;
    $('t-phr').textContent = enAttente(c) ? 'Phrases (en attente)' : 'Phrases';
    // Variables (D015) : noms et releve des valeurs, en lecture seule. Un
    // firmware anterieur ne les remonte pas : l'onglet reste masque.
    GLOBALES = sc.variables || [];
    document.body.classList.toggle('sansvars', !sc.variables);
    // Suivies (le serveur reporte chaque changement signale par le module) :
    // valeurs a jour au dernier contact. Sinon : releve a la date du miroir.
    const suivies = !!sc.variablesSuivies;
    const brut = suivies ? (d.presence || {}).last_seen : (d.config || {}).updatedAt;
    const quand = brut ? new Date(String(brut).replace(' ', 'T') + 'Z') : null;
    $('gv-releve').textContent = quand && !isNaN(quand)
      ? 'Valeur au ' + quand.toLocaleString('fr-FR', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit' }) : 'Valeur';
    $('gv-releve').title = suivies
      ? 'À jour au dernier contact du module : il signale chaque changement de valeur à sa synchronisation.'
      : 'Relevé envoyé par le module avec sa configuration : une valeur changée par un script depuis n’apparaît pas encore.';
    $('t-var').title = 'Seize nombres entiers partagés par tous les scripts. En lecture seule ici : les noms se changent dans l’éditeur du module.';
    varPret = false;
    if (!$('p-var').hidden) tab('var');
    // Relu depuis l'espace en ligne : l'onglet repart de cette liste.
    phrPret = false;
    if (!$('p-phr').hidden) tab('phr');
    renderSim();
  };

  listeSlots = async function () {
    const sc = MIROIR.scripts;
    if (!sc) throw new Error('ce module ne remonte pas encore ses scripts (firmware à mettre à jour)');
    TAILLE_MAX = sc.tailleMax || TAILLE_MAX;
    SIMULT = sc.simultanes || 2;
    SLOTS = (sc.emplacements || []).map((s) => Object.assign({ utilise: s.nom !== undefined }, s));
    majModule(sc.notifications, sc.productLine);
    rendreSlots();
  };
  // Une commande en attente l'emporte sur le miroir : c'est elle que charger()
  // affiche, la liste doit dire le meme nom.
  vueSlot = function (s) {
    const c = cmdPour(s.i);
    if (!enAttente(c)) return s;
    const p = c.command.scripts[0];
    return p.efface ? { i: s.i, utilise: false } : Object.assign({ utilise: true }, p);
  };
  suffixeSlot = (i) => (enAttente(cmdPour(i)) ? ' (en attente)' : '');

  charger = async function () {
    const i = slot();
    sel = null; undo = []; $('b-undo').disabled = true; arret = null;
    editable = true; lecture = null; couvrir('');
    const s = SLOTS.find((x) => x.i === i) || { i: i, utilise: false };
    const c = cmdPour(i);
    const p = enAttente(c) ? c.command.scripts[0] : null;
    let note = '', etat = '';
    if (p && p.efface) note = ' Effacement envoyé, en attente de synchronisation.';
    else if (p) note = ' Modification envoyée, en attente de synchronisation : c’est elle qui est affichée.';
    else if (c && c.state === 'refused') {
      note = ' Dernier envoi refusé par le module : ' + ((c.result || {}).detail || 'raison inconnue') + '.';
      etat = 'err';
    }
    const src = p ? (p.efface ? null : p) : (s.utilise ? s : null);
    if (!src) {
      doc = { nom: '', actif: true, trigger: { t: 0, target: 0 }, body: [] };
      $('nom').value = '';
      setDirty(false); refresh(true); fit();
      dire('Emplacement ' + (i + 1) + ' vide : construisez le script à partir du bloc « Départ ».' + note, etat);
      return;
    }
    doc = { nom: src.nom, actif: !!src.actif, trigger: { t: src.declencheur, target: src.cible || 0 }, body: [] };
    $('nom').value = src.nom;
    const octets = p ? p.code.length / 2 : (s.octets || 0);
    let msg;
    if (typeof src.source !== 'string' || !src.source) {
      editable = false; lecture = { texte: '', octets: octets };
      couvrir('Texte source indisponible', s.sourceTropLongue
        ? 'Le texte de ce script est trop long pour être édité en ligne. Il reste modifiable sur le réseau local.'
        : (MIROIR.scripts.sd === false
          ? 'La carte SD du module est illisible : le texte de ce script n’a pas pu être lu.'
          : 'Ce script tourne sur le module, mais son texte n’a pas été retrouvé sur la carte SD. Vous pouvez le réécrire ici, en schéma ou avec « Modifier le texte ».'));
      msg = 'Chargé : ' + octets + ' octets, sans texte source.'; etat = etat || 'ko';
    } else {
      try {
        doc.body = S.parse(src.source);
        msg = 'Chargé : ' + octets + ' octets.'; etat = etat || 'ok';
      } catch (e) {
        if (!(e instanceof S.Unsupported)) throw e;
        editable = false; lecture = { texte: src.source, octets: octets };
        couvrir('Ce script s’écrit en mode texte',
          'Il utilise une tournure que le schéma ne sait pas dessiner (ligne ' + e.line + ' : ' + e.message + '). '
          + 'Il se modifie avec « Modifier le texte ». Repartir d’un schéma vide remplacera le script à l’enregistrement.');
        msg = 'Chargé en lecture : ce script ne se dessine pas.'; etat = etat || 'ko';
      }
    }
    setDirty(false); refresh(true);
    if (modeTexte()) { sel = 'start'; markSel(); renderProps(); }
    else if (!editable) $('props').innerHTML = '<div class="empty">Lecture seule.</div>';
    requestAnimationFrame(fit);
    dire(msg + note, etat);
  };

  enregistrer = async function () {
    const texte = texteAEnvoyer();
    if (texte === null) { dire('Aucun texte à envoyer : écrivez-le avec « Modifier le texte », ou repartez d’un schéma vide.', 'ko'); return; }
    const g = { text: texte };
    let code;
    try { code = AquaScript.compile(g.text); }
    catch (e) { dire('Compilation impossible : ' + e.message + (e.line ? ' (ligne ' + e.line + ')' : ''), 'err'); return; }
    if (code.length > TAILLE_MAX) { dire('Script trop long : ' + code.length + ' octets pour ' + TAILLE_MAX + ' au plus. Retirez des blocs.', 'err'); return; }
    if (doc.trigger.t && !doc.trigger.target) { dire('Choisissez ce qui déclenche le script dans le bloc « Départ ».', 'ko'); choisir('start'); return; }
    try {
      const d = await apiCloud('/app/module/script', { moduleId: CLOUD, script: {
        i: slot(), nom: $('nom').value || 'Sans nom', actif: doc.actif,
        declencheur: doc.trigger.t, cible: doc.trigger.target || 0,
        code: code.map((b) => b.toString(16).padStart(2, '0')).join(''), source: g.text } });
      setDirty(false); arret = null; markSel();
      await lireModule(); await listeSlots();
      dire('Envoyé (' + code.length + ' octets) : le module l’appliquera à sa prochaine synchronisation'
        + (d.remplace ? ', à la place de l’envoi précédent' : '') + '. Il reste seul juge et peut refuser.', 'ok');
    } catch (e) { dire('Non envoyé : ' + e.message + '.', 'ko'); }
  };

  effacer = async function () {
    const i = slot();
    if (!confirm('Effacer le script ' + (i + 1) + ' du module ? L’effacement partira à sa prochaine synchronisation.')) return;
    try {
      await apiCloud('/app/module/script', { moduleId: CLOUD, script: { i: i, efface: true } });
      await lireModule(); await listeSlots(); await charger();
    } catch (e) { dire('Non envoyé : ' + e.message + '.', 'ko'); }
  };

  lancer = function () { dire('Un script ne se lance pas depuis l’espace en ligne.', 'ko'); };
  $('b-save').onclick = enregistrer;
  $('b-lancer').onclick = lancer;
}

(async function () {
  await lireModule();
  try { await listeSlots(); }
  catch (e) {
    dire(CLOUD ? 'Lecture impossible : ' + e.message + '.' : 'Module injoignable : la liste des scripts n’a pas pu être lue.', 'err');
    return;
  }
  const demande = new URLSearchParams(location.search).get('i');
  if (demande !== null && $('slot').querySelector('option[value="' + parseInt(demande, 10) + '"]')) $('slot').value = String(parseInt(demande, 10));
  await charger();
})().catch((e) => dire(CLOUD ? 'Lecture impossible : ' + e.message + '. Reconnectez-vous depuis l’espace en ligne.' : 'Module injoignable.', 'err'));
