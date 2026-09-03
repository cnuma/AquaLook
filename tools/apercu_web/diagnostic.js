
// Diagnostic de debordement : liste les elements dont le bord droit depasse la
// fenetre. Un simple coup d'oeil sur la capture ne dit pas QUI elargit la page
// -- tout paraissait coupe alors qu'un seul tableau en etait la cause.
window.APERCU_DIAG = function(){
  const L = window.innerWidth;
  const coupables = [];
  document.querySelectorAll('*').forEach(el => {
    const r = el.getBoundingClientRect();
    if (r.width === 0) return;
    if (r.right > L + 1 || r.width > L + 1) {
      // On ne garde que le plus haut de chaque branche : signaler les enfants
      // d'un bloc trop large n'apprend rien.
      if (coupables.some(c => c.el.contains(el))) return;
      coupables.push({ el: el, w: Math.round(r.width), right: Math.round(r.right) });
    }
  });
  const d = document.createElement('div');
  d.style.cssText = 'position:fixed;left:0;right:0;bottom:0;z-index:99;background:#b3261e;'
                  + 'color:#fff;font:11px/1.4 monospace;padding:6px 8px;max-height:40%;overflow:auto';
  d.textContent = coupables.length
    ? ('fenetre ' + L + 'px, page ' + document.documentElement.scrollWidth + 'px | '
       + coupables.map(c => (c.el.tagName.toLowerCase()
         + (c.el.className ? '.' + String(c.el.className).split(' ').join('.') : '')
         + ' ' + c.w + 'px')).join('  |  '))
    : ('aucun debordement -- fenetre ' + L + 'px, page '
       + document.documentElement.scrollWidth + 'px');
  document.body.appendChild(d);
};
