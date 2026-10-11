// Verifications du banc de l'editeur (tools/editeur_banc.py), injectees en fin
// de data/scripts-schema.html : utilisations des phrases et variables (puces,
// info-bulles, confirmations), UX d'Enregistrer, ordre et aides des onglets,
// pied toujours visible. Resultat dans <pre id="RESULTATS">, premiere ligne
// « TOUT OK » ou « ECHECS: n ».
(async function () {
  const R = [];
  let echecs = 0;
  const ok = (c, t) => { R.push((c ? 'OK  ' : 'KO  ') + t); if (!c) echecs++; };
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const survol = (el) => el.dispatchEvent(new MouseEvent('mouseover', { bubbles: true }));
  await wait(800);
  // Phrases
  document.getElementById('t-phr').click(); await wait(400);
  const trs = document.querySelectorAll('#phrases-liste tr');
  survol(trs[0].querySelector('input')); ok(/Utilisée dans : 1 — Matin/.test(trs[0].title), 'survol 901 : ' + trs[0].title);
  survol(trs[1].querySelector('input')); ok(/2 — Pompe/.test(trs[1].title) && !/Matin/.test(trs[1].title), 'survol 902 : ' + trs[1].title);
  survol(trs[2].querySelector('input')); ok(/aucun script/.test(trs[2].title) && /script 3/.test(trs[2].title), 'survol 903 : ' + trs[2].title);
  ok(!document.getElementById('ph-enregistrer').classList.contains('a-enregistrer'), 'phrases : bouton vert au départ');
  const nbs = [...document.querySelectorAll('#phrases-liste [data-nb]')].map((b) => b.textContent + (b.classList.contains('oui') ? '+' : '-'));
  ok(nbs.join(',') === '1+,1+,0-', 'puces phrases : ' + nbs.join(','));
  const ordre = [...document.querySelectorAll('.tabs[role=tablist] button')].map((b) => b.id).join(',');
  ok(ordre === 't-phr,t-var,t-run', 'ordre des onglets : ' + ordre);
  ok(/Rangées sur la carte SD/.test(document.getElementById('t-phr').title) && /Seize nombres/.test(document.getElementById('t-var').title) && !document.querySelector('.aide'), 'aides en info-bulles');
  ok(!/Noter au journal » n’est pas/.test(document.getElementById('ph-notif').textContent), 'mention journal retirée');
  ok(getComputedStyle(document.getElementById('ph-fermer')).backgroundColor === 'rgb(230, 241, 247)', 'Annuler bleuté : ' + getComputedStyle(document.getElementById('ph-fermer')).backgroundColor);
  // Supprimer une phrase inutilisee : pas de confirmation
  window.__confirms = [];
  trs[2].querySelector('[data-del-phrase]').click(); await wait(300);
  ok(window.__confirms.length === 0 && BIBLIO.length === 2, 'suppr 903 sans confirmation');
  ok(document.getElementById('ph-enregistrer').classList.contains('a-enregistrer') && document.getElementById('t-phr').classList.contains('modifie'), 'phrases : orange + puce après modif');
  // Supprimer une phrase utilisee, refusee
  window.__repondre = false; window.__confirms = [];
  document.querySelectorAll('#phrases-liste tr')[1].querySelector('[data-del-phrase]').click(); await wait(300);
  ok(window.__confirms.length === 1 && BIBLIO.length === 2, 'suppr 902 refusée : ' + JSON.stringify(window.__confirms[0]));
  // Changer le code 901 -> 950 : confirmation a l'enregistrement
  window.__confirms = [];
  const c = document.querySelector('#phrases-liste input[data-b="code"]'); c.value = '950'; c.dispatchEvent(new Event('input', { bubbles: true }));
  document.getElementById('ph-enregistrer').click(); await wait(400);
  ok(window.__confirms.length === 1 && /901/.test(window.__confirms[0]), 'code changé : ' + JSON.stringify(window.__confirms[0]));
  document.getElementById('ph-fermer').click(); await wait(100);
  ok(!document.getElementById('ph-enregistrer').classList.contains('a-enregistrer') && !document.getElementById('t-phr').classList.contains('modifie'), 'annuler : retour au vert');
  const mp = document.getElementById('msgphrases');
  ok(mp.textContent === 'Modifications annulées.' && mp.parentElement.classList.contains('acts'), 'message entre les boutons');
  await wait(3600);
  ok(mp.textContent === '', 'message effacé après 3 s');
  // Variables
  document.getElementById('t-var').click(); await wait(400);
  const v = (i) => document.querySelector('#gv-liste input[data-gn="' + i + '"]').closest('tr');
  survol(v(1)); ok(/2 — Pompe/.test(v(1).title), 'survol g1 : ' + v(1).title);
  survol(v(3)); ok(/2 — Pompe/.test(v(3).title), 'survol g3 : ' + v(3).title);
  survol(v(5)); ok(/aucun script/.test(v(5).title), 'survol g5 : ' + v(5).title);
  const nv = [1, 2, 3, 5].map((i) => { const b = v(i).querySelector('[data-nb]'); return b.textContent + (b.classList.contains('oui') ? '+' : '-'); });
  ok(nv.join(',') === '1+,0-,1+,0-', 'puces variables g1,g2,g3,g5 : ' + nv.join(','));
  window.__confirms = [];
  const n3 = document.querySelector('#gv-liste input[data-gn="3"]'); n3.value = ''; n3.dispatchEvent(new Event('input', { bubbles: true }));
  const n5 = document.querySelector('#gv-liste input[data-gn="5"]'); n5.value = '';
  ok(document.getElementById('gv-enregistrer').classList.contains('a-enregistrer'), 'variables : orange après modif');
  document.getElementById('gv-enregistrer').click(); await wait(400);
  ok(window.__confirms.length === 1 && /g3/.test(window.__confirms[0]) && !/g5/.test(window.__confirms[0]), 'nom g3 effacé : ' + JSON.stringify(window.__confirms[0]));
  // Pied visible : bas du bouton dans la zone visible de l'onglet
  const pv = document.getElementById('p-var'), b = document.getElementById('gv-enregistrer');
  ok(pv.scrollHeight > pv.clientHeight, 'liste variables plus haute que l’onglet (' + pv.scrollHeight + ' > ' + pv.clientHeight + ')');
  ok(b.getBoundingClientRect().bottom <= pv.getBoundingClientRect().bottom + 1, 'bouton Enregistrer les variables visible sans défiler');
  // Volets lateraux repliables (P2.6) : la colonne tombe a 0, le schema
  // gagne la largeur, la pastille reste dans la gouttiere, preference gardee.
  const main = document.querySelector('.main'), cv = document.getElementById('canvas');
  const bp = document.getElementById('b-volet-pal'), br = document.getElementById('b-volet-props');
  const larg = () => cv.getBoundingClientRect().width;
  const pointe = (el) => getComputedStyle(el, '::before').borderLeftWidth === '0px' ? 'gauche' : 'droite';
  const l0 = larg();
  ok(bp.offsetParent && br.offsetParent && pointe(bp) === 'gauche' && pointe(br) === 'droite', 'volets : pastilles visibles, triangles vers les bords');
  // Jonction en T : centre de la pastille sur la limite schema / zone du bas,
  // y compris zone du bas repliee.
  const auT = () => [bp, br].every((el) => { const r = el.getBoundingClientRect();
    return Math.abs((r.top + r.bottom) / 2 - (document.querySelector('.bottom').getBoundingClientRect().top - 4)) < 2; });
  const t1 = auT(); document.getElementById('b-replier').click(); await wait(100);
  const t2 = auT(); document.getElementById('b-replier').click(); await wait(100);
  ok(t1 && t2, 'pastilles des volets sur les T (zone du bas dépliée : ' + t1 + ', repliée : ' + t2 + ')');
  bp.click(); await wait(100);
  const l1 = larg(), mr = main.getBoundingClientRect();
  ok(main.classList.contains('pal-replie') && getComputedStyle(document.querySelector('.palette')).visibility === 'hidden', 'palette repliée');
  ok(Math.abs(l1 - l0 - 230) < 2, 'schéma élargi de la palette : ' + Math.round(l0) + ' -> ' + Math.round(l1));
  ok(Math.abs(bp.getBoundingClientRect().left + 8 - mr.left - 4) < 2 && pointe(bp) === 'droite', 'pastille palette au bord gauche, triangle vers le schéma');
  ok(bp.getAttribute('aria-expanded') === 'false' && localStorage.getItem('aqualook-pal-replie') === '1', 'palette : aria + préférence');
  br.click(); await wait(100);
  const l2 = larg();
  ok(Math.abs(l2 - l1 - 320) < 2 && pointe(br) === 'gauche', 'réglages repliés : ' + Math.round(l1) + ' -> ' + Math.round(l2));
  ok(Math.abs(mr.right - 4 - (br.getBoundingClientRect().left + 8)) < 2, 'pastille réglages au bord droit');
  bp.click(); br.click(); await wait(100);
  ok(Math.abs(larg() - l0) < 2 && !main.classList.contains('pal-replie') && !main.classList.contains('props-replie')
    && localStorage.getItem('aqualook-props-replie') === '0', 'volets dépliés : largeur d’origine');
  ok(window.__err.length === 0, 'erreurs JS : ' + JSON.stringify(window.__err));
  const pre = document.createElement('pre'); pre.id = 'RESULTATS'; pre.textContent = (echecs ? 'ECHECS: ' + echecs : 'TOUT OK') + '\n' + R.join('\n'); document.body.appendChild(pre);
})().catch((e) => { const pre = document.createElement('pre'); pre.id = 'RESULTATS'; pre.textContent = 'EXCEPTION ' + e.stack; document.body.appendChild(pre); });
