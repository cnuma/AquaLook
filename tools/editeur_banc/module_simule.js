// Module simule pour le banc de l'editeur (tools/editeur_banc.py) : remplace
// fetch avant le chargement de data/scripts-schema.html. Trois scripts
// utilises (0 et 1 avec texte source, 2 sans texte sur la SD), trois phrases
// (901 et 902 citees, 903 libre), trois variables nommees (g1 et g3 citees,
// g5 libre). confirm() est intercepte : window.__repondre = false refuse.
window.__err = [];
window.addEventListener('error', (e) => window.__err.push(String(e.message)));
window.__confirms = [];
window.confirm = (m) => { window.__confirms.push(m); return window.__repondre !== false; };
try { localStorage.setItem('aqualook-bas-replie', '0'); } catch (e) {}
const SRC = {
  0: 'message 901\n',
  1: 'notifier 902 avec g3   # Etat pompe\ng1 = g1 + 1\n',
  2: null
};
const rep = (o, st) => Promise.resolve(new Response(typeof o === 'string' ? o : JSON.stringify(o), { status: st || 200 }));
window.fetch = (u) => {
  u = String(u);
  if (u.startsWith('/api/scripts')) return rep({ tailleMax: 400, simultanes: 4, scripts: [
    { i: 0, utilise: true, nom: 'Matin', actif: true }, { i: 1, utilise: true, nom: 'Pompe', actif: true },
    { i: 2, utilise: true, nom: 'Sans texte', actif: true }, { i: 3, utilise: false }, { i: 4, utilise: false }, { i: 5, utilise: false }] });
  if (u.startsWith('/api/script-one')) return rep({ ok: true, nom: 'Matin', actif: true, declencheur: 0, code: [1, 2] });
  if (u.startsWith('/api/script-source')) { const i = +u.split('=')[1]; return SRC[i] == null ? rep('', 404) : rep(SRC[i]); }
  if (u.startsWith('/api/script-messages')) return rep({ max: 48, lenMax: 80, entries: [
    { code: 901, texte: 'Bonjour' }, { code: 902, texte: 'Pompe' }, { code: 903, texte: 'Libre' }] });
  if (u.startsWith('/api/script-globals')) return rep({ vars: [{ i: 1, nom: 'Compteur', valeur: 4 }, { i: 3, nom: 'Etat pompe', valeur: 3 }, { i: 5, nom: 'Inutile', valeur: 0 }] });
  return rep({}, 404);
};
