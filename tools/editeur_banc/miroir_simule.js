// Espace en ligne simule pour le banc de l'editeur (tools/editeur_banc.py) :
// la page est ouverte avec ?module=Banc-01, et /app/module rend un miroir
// avec deux scripts, trois phrases et les variables g1..g16 (D015). Avec
// &sansvars=1, le miroir est celui d'un firmware anterieur, sans variables ;
// avec &suivies=1, le serveur suit les valeurs (dernier contact le 09/10).
window.__err = [];
window.addEventListener('error', (e) => window.__err.push(String(e.message)));
window.__fetchs = [];
const SANS_VARS_SIMULE = new URLSearchParams(location.search).get('sansvars') === '1';
const SUIVIES_SIMULE = new URLSearchParams(location.search).get('suivies') === '1';
const VARS_SIMULEES = Array.from({ length: 16 }, (_, k) => ({ i: k + 1, nom: '', valeur: 0 }));
VARS_SIMULEES[0] = { i: 1, nom: 'Compteur', valeur: 42 };
VARS_SIMULEES[2] = { i: 3, nom: 'Etat pompe', valeur: -7 };
const MIROIR_SIMULE = {
  revision: 12, zones: [{ id: 1, name: 'Pelouse' }, { id: 2, name: 'Potager' }],
  scripts: {
    max: 6, tailleMax: 400, simultanes: 4, notifications: true, sd: true,
    emplacements: [
      { i: 0, nom: 'Matin', actif: true, declencheur: 0, cible: 0, octets: 4, source: 'message 901\n' },
      { i: 1, nom: 'Pompe', actif: true, declencheur: 0, cible: 0, octets: 12, source: 'notifier 902 avec g3\ng1 = g1 + 1\n' },
      { i: 2 }, { i: 3 }, { i: 4 }, { i: 5 }],
    entrees: [],
    phrases: { max: 48, lenMax: 80, entries: [{ code: 901, texte: 'Bonjour' }, { code: 902, texte: 'Pompe' }, { code: 903, texte: 'Libre' }] }
  }
};
if (!SANS_VARS_SIMULE) MIROIR_SIMULE.scripts.variables = VARS_SIMULEES;
if (SUIVIES_SIMULE) MIROIR_SIMULE.scripts.variablesSuivies = true;
const repMiroir = (o, st) => Promise.resolve(new Response(JSON.stringify(o), { status: st || 200 }));
window.fetch = (u, opt) => {
  u = String(u);
  window.__fetchs.push(((opt && opt.method) || 'GET') + ' ' + u);
  if (u.startsWith('/app/module?')) return repMiroir({ presence: { last_seen: '2026-10-09 06:05:00.000' }, config: { revision: 12, updatedAt: '2026-10-08 19:40:00.000', payload: MIROIR_SIMULE }, derniers: [], commandes: [] });
  return repMiroir({ detail: 'inconnu' }, 404);
};
