// Editeur graphique des scripts : le modele, le texte qu'il produit, et la
// relecture d'un texte existant en schema.
//
// POURQUOI LE SCHEMA PRODUIT DU TEXTE, ET PAS DU BYTECODE
//
// Le langage texte (script-lang.js) reste la seule entree du compilateur, et
// le module ne voit que le bytecode qui en sort. Le schema n'est qu'une autre
// facon d'ecrire ce texte : rien ne change cote module, ni la VM, ni la
// validation, ni le stockage. Le texte genere est aussi ce qui est range sur
// la carte SD, donc un script cree ici s'ouvre tel quel dans l'editeur texte.
//
// POURQUOI LA RELECTURE REFUSE CERTAINS SCRIPTS
//
// Le schema ne sait representer qu'une partie du langage : pas de variables
// libres, pas d'arithmetique, pas de durees calculees. Un script ecrit a la
// main avec ces tournures est refuse avec le numero de ligne, et reste
// modifiable dans l'editeur texte. Le traduire « a peu pres » changerait son
// comportement sans le dire -- exactement ce qu'un arrosage ne doit pas faire.
//
// Equivalences exactes sur lesquelles la relecture s'appuie : entree() et
// zoneactive() valent toujours 0 ou 1 (ScriptHostRuntime::readInput/
// zoneActive), donc « entree(1) », « entree(1) != 0 » et « entree(1) == 1 »
// disent la meme chose.

(function (global) {
  'use strict';

  // Ordre = ordre de la palette. « cat » choisit la couleur du bloc.
  const TYPES = {
    arroser:   { label: 'Arroser',          cat: 'eau',     icon: 'i-drop',   desc: 'Ouvre une zone pendant une durée' },
    arreter:   { label: 'Arrêter',          cat: 'eau',     icon: 'i-stop',   desc: 'Ferme une zone tout de suite' },
    suspendre: { label: 'Suspendre',        cat: 'eau',     icon: 'i-pause',  desc: 'Met une zone en pause' },
    reprendre: { label: 'Reprendre',        cat: 'eau',     icon: 'i-play',   desc: 'Relance une zone en pause' },
    attendre:  { label: 'Attendre',         cat: 'temps',   icon: 'i-clock',  desc: 'Patiente sans bloquer le module' },
    si:        { label: 'Si… alors',        cat: 'logique', icon: 'i-branch', desc: 'Deux chemins selon une condition' },
    repeter:   { label: 'Répéter',          cat: 'boucle',  icon: 'i-loop',   desc: 'Refait les blocs un nombre de fois' },
    tantque:   { label: 'Tant que',         cat: 'boucle',  icon: 'i-loop',   desc: 'Refait les blocs tant qu’une condition est vraie' },
    notifier:  { label: 'Notifier',         cat: 'info',    icon: 'i-bell',   desc: 'Envoie une notification au téléphone' },
    message:   { label: 'Noter au journal', cat: 'info',    icon: 'i-note',   desc: 'Ajoute une ligne au journal du module' },
    lancer:    { label: 'Lancer un script', cat: 'script',  icon: 'i-run',    desc: 'Démarre un autre script, qui tourne en parallèle' },
    parallele: { label: 'En parallèle',     cat: 'script',  icon: 'i-par',    desc: 'Deux suites de blocs qui partent ensemble' }
  };

  const VARS = 'abcdefgh';
  const ZONE_ACTIONS = ['arreter', 'suspendre', 'reprendre'];

  let uid = 1;
  function newId() { return 'n' + (uid++); }

  // Valeurs par defaut : la premiere zone / entree / phrase connue, pour que
  // le bloc pose soit immediatement valide.
  function make(t, ctx) {
    const z = ctx && ctx.zoneId ? ctx.zoneId : 1;
    const e = ctx && ctx.inputId ? ctx.inputId : 1;
    const c = ctx && ctx.code ? ctx.code : 900;
    const n = { id: newId(), t: t };
    switch (t) {
      case 'arroser': n.zone = z; n.sec = 600; n.attente = true; break;
      case 'arreter': case 'suspendre': case 'reprendre': n.zone = z; break;
      case 'attendre': n.sec = 300; break;
      case 'notifier': case 'message': n.code = c; break;
      case 'lancer': n.script = ctx && ctx.scriptNo ? ctx.scriptNo : 1; break;
      case 'parallele': n.a = []; n.b = []; break;
      case 'si': n.cond = { terms: [{ k: 'entree', id: e, v: 1 }], ops: [] }; n.oui = []; n.non = []; break;
      case 'repeter': n.n = 3; n.body = []; break;
      case 'tantque': n.cond = { terms: [{ k: 'zoneactive', id: z, v: 1 }], ops: [] }; n.body = []; break;
    }
    return n;
  }

  function childKeys(n) {
    if (n.t === 'si') return ['oui', 'non'];
    if (n.t === 'parallele') return ['a', 'b'];
    if (n.t === 'repeter' || n.t === 'tantque') return ['body'];
    return [];
  }

  function reId(n) {
    n.id = newId();
    childKeys(n).forEach(function (k) { n[k].forEach(reId); });
    return n;
  }

  // ── Texte ──────────────────────────────────────────────────────────────
  function termCode(t) {
    return (t.k === 'entree' ? 'entree(' : 'zoneactive(') + t.id + ') == ' + (t.v ? 1 : 0);
  }
  // Une condition = des termes, et entre deux termes voisins un mot « et »
  // ou « ou » (ops[i] joint terms[i] et terms[i+1]). « et » passe avant
  // « ou », comme dans le compilateur (script-lang.js, orExpr/andExpr) :
  // la condition se lit donc comme des groupes « et » separes par « ou ».
  function condGroups(c) {
    const g = [[c.terms[0]]];
    for (let i = 1; i < c.terms.length; i++) {
      if (c.ops[i - 1] === 'ou') g.push([]);
      g[g.length - 1].push(c.terms[i]);
    }
    return g;
  }
  // Texte de la condition avec fmt(terme) pour chaque terme. Quand « et » et
  // « ou » se melangent, chaque groupe « et » est mis entre parentheses : le
  // compilateur n'en a pas besoin, le lecteur si. Un operateur unique s'ecrit
  // comme avant (meme texte, meme bytecode pour les scripts existants).
  function condText(c, fmt, et, ou) {
    const g = condGroups(c);
    return g.map(function (grp) {
      const s = grp.map(fmt).join(et);
      return g.length > 1 && grp.length > 1 ? '(' + s + ')' : s;
    }).join(ou);
  }
  function condCode(c) { return condText(c, termCode, ' et ', ' ou '); }
  function condEval(c, test) { return condGroups(c).some(function (g) { return g.every(test); }); }

  // Duree lisible pour les commentaires et les blocs : « 10 min », « 90 s »,
  // « 1 h 30 min ».
  function duree(sec) {
    if (sec % 60 !== 0 || sec < 60) return sec + ' s';
    const m = sec / 60;
    if (m < 60) return m + ' min';
    const h = Math.floor(m / 60), r = m % 60;
    return h + ' h' + (r ? ' ' + r + ' min' : '');
  }

  // names = { zone(id), input(id), phrase(code) } -> libelles, pour les
  // commentaires. Le compilateur ignore tout ce qui suit « # ».
  //
  // Rend { text, lines:[{text, id}] } : la ligne garde le bloc qui l'a
  // produite, ce qui permet de surligner l'un depuis l'autre et de retrouver
  // le bloc d'un arret survenu sur le module (lineMap du compilateur).
  function generate(body, names) {
    const lines = [];
    function add(depth, s, id) { lines.push({ text: '  '.repeat(depth) + s, id: id }); }
    function walk(list, d, loopDepth) {
      list.forEach(function (n) {
        switch (n.t) {
          case 'arroser':
            add(d, 'demarrer zone ' + n.zone + ' pendant ' + n.sec + '   # ' + names.zone(n.zone) + ', ' + duree(n.sec), n.id);
            // « demarrer » ouvre la vanne et passe aussitot a la suite. Un
            // jardinier lit « Arroser » comme « arroser, PUIS continuer » :
            // sans cette attente, un « Repeter 3 fois » ouvrait trois fois la
            // zone en quelques secondes (constate sur .141 le 6 oct. 2026).
            // reste() vaut 0 des que l'arrosage est fini ou arrete, et garde
            // le reliquat d'une zone suspendue : c'est plus juste qu'un
            // « attendre <duree> », qu'une suspension decalerait.
            if (n.attente) {
              add(d, 'tantque reste(' + n.zone + ') > 0 faire   # attend la fin de l’arrosage', n.id);
              add(d + 1, 'attendre 1', n.id);
              add(d, 'fintantque', n.id);
            }
            break;
          case 'arreter': case 'suspendre': case 'reprendre':
            add(d, n.t + ' zone ' + n.zone + '   # ' + names.zone(n.zone), n.id);
            break;
          case 'attendre':
            add(d, 'attendre ' + n.sec + '   # ' + duree(n.sec), n.id);
            break;
          case 'notifier': case 'message':
            add(d, n.t + ' ' + n.code + '   # ' + names.phrase(n.code), n.id);
            break;
          case 'lancer':
            add(d, 'lancer script ' + n.script + '   # ' + (names.script ? names.script(n.script) : 'script ' + n.script), n.id);
            break;
          case 'si':
            add(d, 'si ' + condCode(n.cond) + ' alors', n.id);
            walk(n.oui, d + 1, loopDepth);
            // « sinon » meme si la branche Oui est vide : le compilateur
            // l'accepte, et la relecture retrouve les blocs du bon cote.
            if (n.non.length) { add(d, 'sinon', n.id); walk(n.non, d + 1, loopDepth); }
            add(d, 'finsi', n.id);
            break;
          case 'repeter':
            add(d, 'pour ' + VARS[loopDepth] + ' de 1 à ' + n.n + ' faire', n.id);
            walk(n.body, d + 1, loopDepth + 1);
            add(d, 'finpour', n.id);
            break;
          case 'parallele':
            add(d, 'parallele   # les deux branches partent ensemble', n.id);
            walk(n.a, d + 1, loopDepth);
            add(d, 'avec', n.id);
            walk(n.b, d + 1, loopDepth);
            add(d, 'finparallele   # attend la fin des deux branches', n.id);
            break;
          case 'tantque':
            add(d, 'tantque ' + condCode(n.cond) + ' faire', n.id);
            walk(n.body, d + 1, loopDepth);
            add(d, 'fintantque', n.id);
            break;
        }
      });
    }
    walk(body, 0, 0);
    return { text: lines.map(function (l) { return l.text; }).join('\n') + '\n', lines: lines };
  }

  // ── Relecture texte -> schema ─────────────────────────────────────────
  class Unsupported extends Error {
    constructor(message, line) { super(message); this.line = line; }
  }

  // Meme decoupage que script-lang.js (commentaires « # », memes jetons).
  function tokenize(text) {
    const toks = [];
    text.split(/\r?\n/).forEach(function (raw, idx) {
      const src = raw.replace(/#.*$/, '');
      const re = /\s*(>=|<=|==|!=|[-+*/%()=<>,]|[A-Za-zÀ-ÿ_][A-Za-zÀ-ÿ0-9_]*|\d+)/g;
      let m, pos = 0;
      while ((m = re.exec(src)) !== null) {
        if (m.index !== pos) throw new Unsupported('caractère inattendu', idx + 1);
        toks.push({ v: m[1], low: m[1].toLowerCase(), line: idx + 1 });
        pos = re.lastIndex;
      }
      if (src.slice(pos).trim() !== '') throw new Unsupported('caractère inattendu', idx + 1);
    });
    toks.push({ v: null, low: null, line: toks.length ? toks[toks.length - 1].line : 1 });
    return toks;
  }

  function parse(text) {
    const toks = tokenize(text);
    const loopVars = [];
    let inParallel = false;
    let i = 0;
    const peek = function () { return toks[i].low; };
    const line = function () { return toks[i].line; };
    const next = function () { return toks[i++]; };
    function expect(words) {
      const alts = Array.isArray(words) ? words : [words];
      if (alts.indexOf(peek()) < 0) {
        throw new Unsupported('« ' + alts[0] + ' » attendu, trouvé « ' + (toks[i].v || 'fin du script') + ' »', line());
      }
      return next();
    }
    function number() {
      const t = next();
      if (!t.v || !/^\d+$/.test(t.v)) {
        throw new Unsupported('valeur calculée « ' + (t.v || 'fin du script') + ' » : seul un nombre fixe se dessine', t.line);
      }
      return parseInt(t.v, 10);
    }
    // Apres une valeur fixe, rien d'autre qu'une fin d'instruction : un
    // « attendre 60 * a » serait sinon lu « attendre 60 » sans rien dire.
    function noArith() {
      if (['+', '-', '*', '/', '%'].indexOf(peek()) >= 0) {
        throw new Unsupported('calcul « ' + toks[i].v + ' » : seul un nombre fixe se dessine', line());
      }
    }

    // L'attente de fin d'arrosage que generate() ecrit apres un « demarrer »,
    // reconnue mot pour mot et sur la MEME zone : toute autre boucle reste
    // une boucle « Tant que » (ou est refusee, reste() n'etant pas dessinable).
    function attenteFin(zone) {
      const motif = ['tantque', 'reste', '(', String(zone), ')', '>', '0', 'faire', 'attendre', '1', 'fintantque'];
      for (let k = 0; k < motif.length; k++) {
        if (!toks[i + k] || toks[i + k].low !== motif[k]) return false;
      }
      i += motif.length;
      return true;
    }

    // Une condition = des termes « entree(n) / zoneactive(n) » valant 0 ou
    // 1, joints par « et » / « ou » avec la priorite du compilateur. Chaque
    // niveau rend des groupes « et » separes par « ou » (tableau de tableaux
    // de termes) ; ce qui ne s'ecrit pas ainsi sans recalculer la condition
    // est refuse plutot que reecrit.
    function orGroups() {
      let g = andGroups();
      while (peek() === 'ou') { next(); g = g.concat(andGroups()); }
      return g;
    }
    function andGroups() {
      const ln = line();
      const parts = [unaryGroups()];
      while (peek() === 'et') { next(); parts.push(unaryGroups()); }
      if (parts.length === 1) return parts[0];
      // « (a ou b) et c » : le dessiner a plat demanderait de developper la
      // condition, donc de changer le texte de l'auteur.
      if (parts.some(function (p) { return p.length > 1; })) {
        throw new Unsupported('« ou » entre parenthèses à l’intérieur d’un « et »', ln);
      }
      return [[].concat.apply([], parts.map(function (p) { return p[0]; }))];
    }
    function unaryGroups() {
      if (peek() === 'non') {
        const ln = line();
        next();
        const g = unaryGroups();
        if (g.length !== 1 || g[0].length !== 1) throw new Unsupported('« non » devant plusieurs conditions', ln);
        g[0][0].v = g[0][0].v ? 0 : 1;
        return g;
      }
      if (peek() === '(') { next(); const g = orGroups(); expect(')'); return g; }
      return [[term()]];
    }
    function term() {
      const w = peek();
      if (w !== 'entree' && w !== 'zoneactive') {
        throw new Unsupported('condition « ' + (toks[i].v || 'fin du script') + ' » : seules les entrées et l’état des zones se dessinent', line());
      }
      next(); expect('('); const id = number(); expect(')');
      const t = { k: w, id: id, v: 1 };
      if (peek() === '==' || peek() === '!=') {
        const op = next().v;
        const ln = line();
        const val = number();
        if (val !== 0 && val !== 1) throw new Unsupported('comparaison à ' + val + ' : une entrée ne vaut que 0 ou 1', ln);
        t.v = (op === '==') === (val === 1) ? 1 : 0;
      } else if (['<', '>', '<=', '>='].indexOf(peek()) >= 0) {
        throw new Unsupported('comparaison « ' + toks[i].v + ' » : seuls == et != se dessinent', line());
      }
      return t;
    }
    function condition() {
      const c = { terms: [], ops: [] };
      orGroups().forEach(function (g, gi) {
        g.forEach(function (t, ti) {
          if (c.terms.length) c.ops.push(ti === 0 && gi > 0 ? 'ou' : 'et');
          c.terms.push(t);
        });
      });
      return c;
    }

    function statement() {
      const w = peek(), ln = line();
      if (w === 'si') {
        next();
        const n = { id: newId(), t: 'si', cond: condition(), oui: [], non: [] };
        expect('alors');
        n.oui = block(['sinon', 'finsi']);
        if (peek() === 'sinon') { next(); n.non = block(['finsi']); }
        expect('finsi');
        return n;
      }
      if (w === 'parallele' || w === 'parallèle') {
        if (inParallel) throw new Unsupported('bloc « parallele » dans un autre', ln);
        next();
        inParallel = true;
        const n = { id: newId(), t: 'parallele', a: [], b: [] };
        n.a = block(['avec']);
        expect('avec');
        n.b = block(['finparallele', 'finparallèle']);
        expect(['finparallele', 'finparallèle']);
        inParallel = false;
        return n;
      }
      if (w === 'tantque') {
        next();
        const n = { id: newId(), t: 'tantque', cond: condition(), body: [] };
        expect('faire');
        n.body = block(['fintantque']);
        expect('fintantque');
        return n;
      }
      if (w === 'pour') {
        next();
        const v = next();
        if (!v.low || VARS.indexOf(v.low) < 0 || v.low.length !== 1) throw new Unsupported('variable de boucle attendue (a à h)', ln);
        // Deux boucles imbriquees sur la meme variable se perturbent : ce
        // n'est pas un « Repeter dans un Repeter », que le schema regenere
        // avec une variable par niveau.
        if (loopVars.indexOf(v.low) >= 0) throw new Unsupported('boucle imbriquée qui réutilise la variable « ' + v.v + ' »', ln);
        expect('de');
        const from = number();
        if (from !== 1) throw new Unsupported('boucle qui ne part pas de 1', ln);
        expect(['à', 'a']);
        const n = { id: newId(), t: 'repeter', n: number(), body: [] };
        noArith();
        expect('faire');
        loopVars.push(v.low);
        n.body = block(['finpour']);
        loopVars.pop();
        expect('finpour');
        return n;
      }
      if (w === 'attendre') { next(); const n = { id: newId(), t: 'attendre', sec: number() }; noArith(); return n; }
      if (w === 'notifier' || w === 'message') { next(); return { id: newId(), t: w, code: number() }; }
      if (w === 'lancer') { next(); expect('script'); return { id: newId(), t: 'lancer', script: number() }; }
      if (w === 'demarrer') {
        next(); expect('zone');
        const zone = number();
        expect('pendant');
        const n = { id: newId(), t: 'arroser', zone: zone, sec: number(), attente: false };
        noArith();
        n.attente = attenteFin(zone);
        return n;
      }
      if (ZONE_ACTIONS.indexOf(w) >= 0) {
        next(); expect('zone');
        return { id: newId(), t: w, zone: number() };
      }
      if (w && VARS.indexOf(w) >= 0 && w.length === 1) {
        throw new Unsupported('variable « ' + toks[i].v + ' » : le schéma ne gère pas les variables', ln);
      }
      throw new Unsupported('instruction « ' + (toks[i].v || 'fin du script') + ' » inconnue du schéma', ln);
    }
    function block(stops) {
      const list = [];
      while (peek() !== null && stops.indexOf(peek()) < 0) list.push(statement());
      if (peek() === null && stops.length) throw new Unsupported('« ' + stops[stops.length - 1] + ' » manquant', line());
      return list;
    }

    const body = block([]);
    return body;
  }

  global.AquaSchema = {
    TYPES: TYPES, make: make, childKeys: childKeys, reId: reId,
    generate: generate, parse: parse, duree: duree, Unsupported: Unsupported,
    condGroups: condGroups, condText: condText, condEval: condEval
  };
})(window);
