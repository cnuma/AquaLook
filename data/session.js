// Session Web locale du module (D016, lot F).
//
// Le mot de passe Web est le secret ApiAuth. Il ne circule jamais : le
// module donne un defi, la page repond HMAC-SHA256(mot de passe,
// "session|" + defi) avec hmac.js (crypto.subtle n'existe pas en HTTP
// simple), et le module pose un cookie HttpOnly.
//
// Ce fichier enveloppe fetch() : une ecriture refusee par le module (401,
// session absente ou expiree) ouvre la fenetre de connexion, puis est
// rejouee une fois si la connexion reussit. Seules les routes /api/ de la
// page courante sont concernees : sur l'editeur publie en ligne, les routes
// du serveur (/v1, /app, /admin) passent sans changement.
(function (global) {
  if (global.AquaSession) return;
  const rawFetch = global.fetch.bind(global);
  let state = null;
  let asking = null;
  const listeners = [];

  function notify() { listeners.forEach((f) => { try { f(state); } catch (e) {} }); }

  async function refresh() {
    try {
      const r = await rawFetch('/api/session/state', { cache: 'no-store' });
      // 404 : firmware anterieur au lot F (pages de la SD plus recentes que
      // le module). Rien n'y est protege : tout reste ouvert, comme avant.
      state = r.ok ? await r.json()
        : r.status === 404 ? { configure: false, active: true, legacy: true } : null;
    } catch (e) { state = null; }
    notify();
    return state;
  }

  async function login(password) {
    const c = await rawFetch('/api/session/challenge', { cache: 'no-store' });
    const j = await c.json();
    if (!j.configure) { await refresh(); return { ok: true }; }
    const sig = global.AquaHmac.hmacSha256Hex(password, 'session|' + j.nonce);
    const r = await rawFetch('/api/session/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ nonce: j.nonce, sig: sig })
    });
    let b = {};
    try { b = await r.json(); } catch (e) {}
    await refresh();
    return { ok: r.ok, error: b.error, retryInSec: b.retryInSec };
  }

  async function logout() {
    try { await rawFetch('/api/session/logout', { method: 'POST' }); } catch (e) {}
    await refresh();
  }

  // Pose ou change le mot de passe. Le module ouvre aussitot une session
  // pour la page qui vient de le saisir.
  async function setPassword(current, next) {
    const r = await rawFetch('/api/auth-secret', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ actuel: current || '', nouveau: next })
    });
    let b = {};
    try { b = await r.json(); } catch (e) {}
    await refresh();
    return { ok: r.ok, error: b.error };
  }

  // Change le mot de passe SANS le faire circuler en clair : le nouveau est
  // chiffre par un flux tire de l'actuel (HMAC sur un defi a usage unique),
  // et l'ensemble est authentifie par l'actuel. Le module refuse la forme en
  // clair des qu'un mot de passe existe. Seule la toute premiere pose reste en
  // clair, faute de secret prealable (et elle exige un geste sur l'ecran).
  async function changePassword(current, next) {
    const cr = await rawFetch('/api/session/challenge', { cache: 'no-store' });
    // Firmware anterieur au lot F : seule la forme historique existe.
    if (!cr.ok) return setPassword(current, next);
    const c = await cr.json();
    if (!c.configure) return setPassword('', next);
    const H = global.AquaHmac.hmacSha256Hex;
    const bytes = new TextEncoder().encode(next);
    if (bytes.length < 12 || bytes.length > 64) {
      return { ok: false, error: 'de 12 a 64 caracteres' };
    }
    const ks = H(current, 'aql-chg-ks|' + c.nonce + '|0') + H(current, 'aql-chg-ks|' + c.nonce + '|1');
    let enc = '';
    for (let i = 0; i < bytes.length; i++) {
      const k = parseInt(ks.substr(i * 2, 2), 16);
      enc += ((bytes[i] ^ k) & 255).toString(16).padStart(2, '0');
    }
    const mac = H(current, 'aql-chg-mac|' + c.nonce + '|' + enc);
    const r = await rawFetch('/api/auth-secret', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ nonce: c.nonce, enc: enc, mac: mac })
    });
    // Defi consomme : apres connexion, tout recommencer avec un defi neuf.
    if (r.status === 401 && await ask('Changer le mot de passe demande une session.')) {
      return changePassword(current, next);
    }
    let b = {};
    try { b = await r.json(); } catch (e) {}
    await refresh();
    return { ok: r.ok, error: b.error };
  }

  function el(tag, css, text) {
    const e = document.createElement(tag);
    if (css) e.style.cssText = css;
    if (text) e.textContent = text;
    return e;
  }

  // Fenetre de connexion construite a la demande : chaque page a sa propre
  // feuille de style, celle-ci n'en depend d'aucune.
  function ask(message) {
    if (asking) return asking;
    asking = new Promise((resolve) => {
      const back = el('div', 'position:fixed;inset:0;background:#000a;z-index:9999;display:flex;align-items:center;justify-content:center;font-family:sans-serif');
      const box = el('form', 'background:#12232e;color:#e8f7ff;padding:18px;border-radius:10px;width:min(320px,90vw);box-shadow:0 4px 20px #0008');
      box.appendChild(el('div', 'font-weight:bold;margin-bottom:8px', 'Mot de passe Web du module'));
      if (message) box.appendChild(el('div', 'font-size:.9em;margin-bottom:8px;opacity:.8', message));
      const input = el('input', 'width:100%;box-sizing:border-box;padding:8px;font-size:1em;border-radius:6px;border:1px solid #456');
      input.type = 'password';
      input.autocomplete = 'current-password';
      box.appendChild(input);
      const err = el('div', 'color:#ff8a80;min-height:1.2em;font-size:.9em;margin-top:6px');
      box.appendChild(err);
      const row = el('div', 'display:flex;gap:8px;justify-content:flex-end;margin-top:8px');
      const cancel = el('button', 'padding:6px 12px', 'Annuler');
      cancel.type = 'button';
      const ok = el('button', 'padding:6px 12px', 'Se connecter');
      ok.type = 'submit';
      row.appendChild(cancel); row.appendChild(ok); box.appendChild(row);
      back.appendChild(box);
      document.body.appendChild(back);
      input.focus();
      const done = (v) => { back.remove(); asking = null; resolve(v); };
      cancel.onclick = () => done(false);
      box.onsubmit = async (ev) => {
        ev.preventDefault();
        ok.disabled = true;
        err.textContent = '';
        try {
          const r = await login(input.value);
          if (r.ok) { done(true); return; }
          err.textContent = (r.error || 'refus') +
            (r.retryInSec ? ' (nouvel essai dans ' + r.retryInSec + ' s)' : '');
        } catch (e) {
          err.textContent = 'module injoignable';
        }
        ok.disabled = false;
        input.select();
      };
    });
    return asking;
  }

  function isModuleApi(input) {
    try {
      const u = new URL(typeof input === 'string' ? input : input.url, global.location.href);
      return u.origin === global.location.origin &&
             u.pathname.startsWith('/api/') && !u.pathname.startsWith('/api/session/');
    } catch (e) { return false; }
  }

  global.fetch = async function (input, init) {
    const r = await rawFetch(input, init);
    if (r.status !== 401 || !isModuleApi(input)) return r;
    if (!(await ask('Cette action demande une session.'))) return r;
    return rawFetch(input, init);
  };

  global.AquaSession = {
    refresh: refresh,
    login: login,
    logout: logout,
    setPassword: setPassword,
    changePassword: changePassword,
    ask: ask,
    state: () => state,
    onChange: (f) => { listeners.push(f); }
  };
})(window);
