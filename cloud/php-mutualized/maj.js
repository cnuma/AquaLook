// Mode d'emploi de mise a jour (maj.html) : une etape a la fois, et nom
// local du module (?nom=jardin-01, transmis par Mes modules) pour un lien
// direct http://<nom>.local. Pas de script inline (CSP du site).
(function () {
  'use strict';
  const nom = new URLSearchParams(location.search).get('nom') || '';
  if (/^[a-z0-9-]{1,32}$/.test(nom)) {
    document.body.classList.add('avec-nom');
    document.querySelectorAll('.nom, .nom-svg').forEach(e => { e.textContent = nom; });
    const a = document.getElementById('aller');
    if (a) a.href = 'http://' + nom + '.local/';
  }

  const etapes = Array.from(document.querySelectorAll('.etape'));
  if (!etapes.length) return;
  document.body.classList.add('js');
  const prec = document.getElementById('prec');
  const suiv = document.getElementById('suiv');
  const ou = document.getElementById('ou');
  const points = document.getElementById('points');
  points.innerHTML = etapes.map(() => '<span></span>').join('');
  let i = 0;

  function montrer(n) {
    i = Math.max(0, Math.min(etapes.length - 1, n));
    etapes.forEach((e, k) => e.classList.toggle('active', k === i));
    Array.from(points.children).forEach((p, k) => {
      p.className = k < i ? 'fait' : (k === i ? 'ici' : '');
    });
    ou.textContent = 'Étape ' + (i + 1) + ' sur ' + etapes.length;
    prec.disabled = i === 0;
    suiv.disabled = i === etapes.length - 1;
    suiv.textContent = i === etapes.length - 2 ? 'Dernière étape ›' : 'Suivant ›';
  }
  prec.addEventListener('click', () => { montrer(i - 1); window.scrollTo(0, 0); });
  suiv.addEventListener('click', () => { montrer(i + 1); window.scrollTo(0, 0); });
  document.addEventListener('keydown', ev => {
    if (ev.key === 'ArrowRight') montrer(i + 1);
    else if (ev.key === 'ArrowLeft') montrer(i - 1);
  });
  // Lien direct vers une etape : maj.html#etape-3
  const m = /^#etape-(\d+)$/.exec(location.hash);
  montrer(m ? parseInt(m[1], 10) - 1 : 0);
})();
