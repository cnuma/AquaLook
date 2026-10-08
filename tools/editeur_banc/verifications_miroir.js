// Verifications du banc de l'editeur en mode espace en ligne
// (tools/editeur_banc.py, avec miroir_simule.js) : onglet Variables en
// lecture seule (D015), noms et releve dates, puces d'utilisation, aucune
// ecriture ; onglet masque si le miroir n'a pas de variables. Premiere
// ligne du resultat : « TOUT OK » ou « ECHECS: n ».
(async function () {
  const R = [];
  let echecs = 0;
  const ok = (c, t) => { R.push((c ? 'OK  ' : 'KO  ') + t); if (!c) echecs++; };
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const visible = (el) => !!el && getComputedStyle(el).display !== 'none';
  await wait(800);
  const tv = document.getElementById('t-var');
  if (new URLSearchParams(location.search).get('sansvars') === '1') {
    ok(document.body.classList.contains('cloud'), 'mode espace en ligne');
    ok(!visible(tv), 'firmware anterieur : onglet Variables masque');
    ok(visible(document.getElementById('t-phr')), 'onglet Phrases toujours present');
  } else {
    ok(visible(tv), 'onglet Variables visible en ligne');
    ok(/lecture seule/.test(tv.title), 'aide de l’onglet : ' + tv.title);
    tv.click(); await wait(400);
    const nom = (i) => document.querySelector('#gv-liste input[data-gn="' + i + '"]');
    const ligne = (i) => nom(i).closest('tr');
    ok(nom(1).value === 'Compteur' && nom(3).value === 'Etat pompe' && nom(2).value === '', 'noms du miroir');
    ok([1, 2, 3, 16].every((i) => nom(i).readOnly), 'noms en lecture seule');
    ok(ligne(1).querySelector('.val').textContent === '42' && ligne(3).querySelector('.val').textContent === '-7', 'valeurs du miroir');
    const rel = document.getElementById('gv-releve').textContent;
    ok(/^Valeur au \d\d\/\d\d/.test(rel), 'date du releve : ' + rel);
    ok(!visible(ligne(1).querySelector('.nv')) && !visible(document.querySelector('#p-var thead .nv')), 'colonne Nouvelle valeur masquee');
    ok(['gv-relire', 'gv-fermer', 'gv-enregistrer'].every((id) => !visible(document.getElementById(id))), 'boutons d’ecriture masques');
    await wait(400);
    const nb = (i) => { const b = ligne(i).querySelector('[data-nb]'); return b.textContent + (b.classList.contains('oui') ? '+' : '-'); };
    ok([1, 2, 3].map(nb).join(',') === '1+,0-,1+', 'puces g1,g2,g3 : ' + [1, 2, 3].map(nb).join(','));
    ok(globaleNom(3) === 'Etat pompe', 'nom de g3 pour les blocs : ' + globaleNom(3));
  }
  ok(window.__fetchs.every((f) => f.startsWith('GET ')), 'aucune ecriture : ' + JSON.stringify(window.__fetchs));
  ok(window.__err.length === 0, 'erreurs JS : ' + JSON.stringify(window.__err));
  const pre = document.createElement('pre'); pre.id = 'RESULTATS'; pre.textContent = (echecs ? 'ECHECS: ' + echecs : 'TOUT OK') + '\n' + R.join('\n'); document.body.appendChild(pre);
})().catch((e) => { const pre = document.createElement('pre'); pre.id = 'RESULTATS'; pre.textContent = 'EXCEPTION ' + e.stack; document.body.appendChild(pre); });
