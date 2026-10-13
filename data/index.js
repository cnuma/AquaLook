/* Repartiteur des gestionnaires data-on<type> (P3.11) : les pages du module
   n'ont plus de script inline ni d'attribut on*= (regle des pages Web
   d'AGENTS.md, CSP script-src 'self'). Les anciens attributs onclick/onchange/
   oninput/onerror sont devenus data-onclick... avec la MEME expression ; elle
   est lue ici par un analyseur restreint, sans eval ni new Function :
     appel(args)[, appel(args)...]   fonction globale (window) seulement ;
     args : nombre, true/false/null, 'chaine' ou "chaine" sans guillemet
            interne, this, this.value, this.checked, event ;
     event.stopPropagation()         arrete la remontee, comme avant.
   La remontee est rejouee de la cible vers la racine : un gestionnaire
   interne passe avant celui de son conteneur, et stopPropagation() (appele
   par l'expression ou par la fonction) l'arrete comme un attribut inline. */
(function () {
  const NOM = /^[A-Za-z_$][\w$]*$/;
  function decouper(s) {
    const out = []; let prof = 0, q = '', debut = 0;
    for (let i = 0; i < s.length; i++) {
      const c = s[i];
      if (q) { if (c === q) q = ''; continue; }
      if (c === "'" || c === '"') q = c;
      else if (c === '(') prof++;
      else if (c === ')') prof--;
      else if (c === ',' && prof === 0) { out.push(s.slice(debut, i)); debut = i + 1; }
    }
    out.push(s.slice(debut));
    return out.map(x => x.trim()).filter(x => x !== '');
  }
  function valeur(a, el, ev) {
    if (a === 'this') return el;
    if (a === 'this.value') return el.value;
    if (a === 'this.checked') return el.checked;
    if (a === 'event') return ev;
    if (a === 'true') return true;
    if (a === 'false') return false;
    if (a === 'null') return null;
    if (/^-?\d+(\.\d+)?$/.test(a)) return Number(a);
    const q = a[0];
    if ((q === "'" || q === '"') && a.length >= 2 && a.indexOf(q, 1) === a.length - 1) return a.slice(1, -1);
    throw new Error('argument non pris en charge : ' + a);
  }
  function executer(expr, el, ev) {
    decouper(expr).forEach(function (appel) {
      if (appel === 'event.stopPropagation()') { ev.stopPropagation(); return; }
      const i = appel.indexOf('(');
      if (i < 0 || appel[appel.length - 1] !== ')') throw new Error('expression non prise en charge : ' + appel);
      const nom = appel.slice(0, i).trim();
      if (!NOM.test(nom) || typeof window[nom] !== 'function') throw new Error('fonction inconnue : ' + nom);
      window[nom].apply(el, decouper(appel.slice(i + 1, -1)).map(a => valeur(a, el, ev)));
    });
  }
  function lancer(expr, el, ev) {
    try { executer(expr, el, ev); } catch (e) { console.error('[data-on' + ev.type + ']', expr, e); }
  }
  function repartir(ev) {
    const attr = 'data-on' + ev.type;
    for (let el = ev.target; el && el.nodeType === 1; el = el.parentElement) {
      const expr = el.getAttribute(attr);
      if (expr !== null) lancer(expr, el, ev);
      if (ev.cancelBubble) break;
    }
  }
  ['click', 'change', 'input'].forEach(t => document.addEventListener(t, repartir));
  // error ne remonte pas : capture. Une image deja en echec avant le
  // chargement de ce script est rattrapee au demarrage.
  document.addEventListener('error', function (ev) {
    const el = ev.target, expr = el && el.getAttribute && el.getAttribute('data-onerror');
    if (expr !== null && expr !== undefined) lancer(expr, el, ev);
  }, true);
  function rattraper() {
    document.querySelectorAll('img[data-onerror]').forEach(function (img) {
      if (img.complete && img.naturalWidth === 0) lancer(img.getAttribute('data-onerror'), img, { type: 'error', stopPropagation() {} });
    });
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', rattraper); else rattraper();
})();

function masquer(el) { el.style.display = 'none'; }

/* Options avancees du module (D016 lot F) : la configuration du module
   (reseau, affichage, serveur, cablage...) reste repliee pour l'usage
   courant et s'ouvre a la demande. Ce n'est pas un verrou : la protection
   est la session Web, exigee par le module pour toute ecriture. */
const A=["sec-wifi","sec-ntp","sec-owm","sec-display","sec-system","sec-notifications","sec-cloud","sec-upd","sec-io","sec-topo","relay-admin","link-urls"],AV="aqualook-avance";
function adminApply(v){A.forEach(i=>{let e=document.getElementById(i);e&&(e.style.display=v?"":"none")});let b=document.getElementById("admin-btn");b&&(b.textContent=v?"Masquer les options avancées":"Options avancées du module");typeof buildCfgMenu=="function"&&buildCfgMenu()}
function adminShow(s){let w=document.getElementById("unprotected-banner");if(s&&!s.configure&&!s.legacy){if(!w){w=document.createElement("div");w.id="unprotected-banner";w.textContent="Accès non protégé : définir un mot de passe Web";w.style.cssText="background:#8a5a00;color:#fff;padding:6px 12px;text-align:center;cursor:pointer;font-size:13px";w.onclick=adminSetPassword;document.body.prepend(w)}}else if(w)w.remove()}
async function adminSetPassword(){let p=await AquaSession.promptSecret("Nouveau mot de passe Web","12 caractères au moins");if(!p)return;if(await AquaSession.promptSecret("Retaper le mot de passe")!==p){alert("Les deux saisies diffèrent");return}let r=await AquaSession.setPassword("",p);alert(r.ok?"Mot de passe posé : l'interface est désormais protégée.":"Refus : "+(r.error||"module injoignable"))}
function avanceOuvert(){try{return sessionStorage.getItem(AV)==="1"}catch(e){return false}}
async function adminToggle(){if(avanceOuvert()){try{sessionStorage.removeItem(AV)}catch(e){}adminApply(0);return}let s=AquaSession.state()||await AquaSession.refresh();if(s&&s.configure&&!s.active&&!await AquaSession.ask("Les options avancées modifient la configuration du module."))return;try{sessionStorage.setItem(AV,"1")}catch(e){}adminApply(1)}
document.addEventListener("DOMContentLoaded",()=>{adminApply(avanceOuvert());AquaSession.onChange(adminShow);AquaSession.refresh()});
