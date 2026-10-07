// Compilateur du langage de scripts AquaLook : texte -> bytecode.
//
// POURQUOI ICI ET PAS DANS LE MODULE
//
// Le navigateur a la puissance, le module a l'eau. Un analyseur syntaxique
// complet sur l'ESP32 aurait coute plusieurs kilo-octets de flash -- ceux-la
// memes qu'on vient de recuperer -- pour un travail qui se fait mieux ici :
// les erreurs sont signalees pendant la frappe, avec le numero de ligne et
// le mot fautif, plutot que decouvertes a l'enregistrement.
//
// Le module, lui, ne fait pas confiance a ce qui sort d'ici : il revalide le
// bytecode avant de l'accepter. C'est normal -- ce compilateur tourne chez le
// client, donc hors de tout controle.
//
// LE LANGAGE
//
//   si <cond> alors ... [sinon ...] finsi
//   tantque <cond> faire ... fintantque
//   pour <var> de <a> a <b> faire ... finpour
//   attendre <expr>                  -- en secondes, rend la main
//   demarrer zone <id> pendant <expr>
//   arreter zone <id>
//   suspendre zone <id>
//   reprendre zone <id>
//   message <code>                   -- une ligne dans le journal
//   notifier <code>                  -- une vraie notification, si configuree
//   lancer script <n>                -- demarre le script n (1..6) en parallele
//   parallele ... avec ... finparallele  -- deux branches qui partent ensemble
//   <var> = <expr>
//
//   Expressions : nombres, variables a..h, entree(<id>), zoneactive(<id>),
//   reste(<id>), + - * / %, == != < <= > >=, et / ou / non, parentheses.
//
// Les mots-cles sont en francais parce que tout le reste de l'interface
// l'est. Un langage dont la moitie des mots sont dans une autre langue que
// les libelles qui l'entourent se lit deux fois moins vite.

(function (global) {
  'use strict';

  const OP = {
    HALT: 0, PUSH: 1, LOAD: 2, STORE: 3, DROP: 4,
    ADD: 16, SUB: 17, MUL: 18, DIV: 19, MOD: 20, NEG: 21,
    EQ: 32, NE: 33, LT: 34, LE: 35, GT: 36, GE: 37,
    AND: 38, OR: 39, NOT: 40,
    JMP: 48, JZ: 49, JNZ: 50,
    READ_INPUT: 64, ZONE_ACTIVE: 65, ZONE_REMAIN: 66,
    ACTION: 80, NOTIFY: 81, WAIT: 82, ALERT: 83,
    FORK: 84, JOIN: 85, ENDBRANCH: 86
  };

  const ACTION = {
    ZONE_START: 1, ZONE_STOP: 2, ZONE_PAUSE: 3, ZONE_RESUME: 4, SET_OUTPUT: 5,
    SCRIPT_RUN: 6
  };
  // Emplacements de scripts sur le module (ScriptStore::MAX_SCRIPTS).
  const MAX_SCRIPTS = 6;

  const VARS = 'abcdefgh';

  class CompileError extends Error {
    constructor(message, line) {
      super(message);
      this.line = line;
    }
  }

  // ── Analyse lexicale ───────────────────────────────────────────────────
  // Les commentaires commencent par # et vont jusqu'au bout de la ligne.
  function tokenize(text) {
    const tokens = [];
    const lines = text.split(/\r?\n/);
    lines.forEach((raw, idx) => {
      const line = idx + 1;
      const src = raw.replace(/#.*$/, '');
      const re = /\s*(>=|<=|==|!=|[-+*/%()=<>,]|[A-Za-zÀ-ÿ_][A-Za-zÀ-ÿ0-9_]*|\d+)/g;
      let m;
      let pos = 0;
      while ((m = re.exec(src)) !== null) {
        if (m.index !== pos) {
          throw new CompileError('caractère inattendu : ' + src.slice(pos, m.index).trim(), line);
        }
        tokens.push({ v: m[1], line: line });
        pos = re.lastIndex;
      }
      if (src.slice(pos).trim() !== '') {
        throw new CompileError('caractère inattendu : ' + src.slice(pos).trim(), line);
      }
    });
    tokens.push({ v: null, line: lines.length });
    return tokens;
  }

  // ── Compilation ────────────────────────────────────────────────────────
  function compile(text) {
    const toks = tokenize(text);
    let i = 0;
    const out = [];
    // Un point {pc, line} au debut de CHAQUE instruction (voir statement()
    // ci-dessous) -- pas une table octet par octet, trop lourde pour rien.
    // Sert uniquement cote editeur, a retrouver la ligne d'un arret survenu
    // sur le module (voir l'arret sur le schema dans
    // scripts-schema.html) ; le module, lui, ne voit jamais cette table.
    const stmtLines = [];
    // Un seul bloc « parallele » a la fois : la machine ne garde qu'un retour
    // pour le cas ou elle doit faire la branche 2 elle-meme (ScriptVm FORK).
    let inParallel = false;

    const peek = () => toks[i].v;
    const line = () => toks[i].line;
    const next = () => toks[i++].v;
    const lower = (v) => (v === null ? null : String(v).toLowerCase());

    // Accepte plusieurs orthographes d'un meme mot-cle : « a » entre en
    // collision avec la variable « a », alors que « à » ne peut etre qu'un
    // mot-cle. On tolere les deux, la documentation montre « à ».
    function expect(word) {
      const alts = Array.isArray(word) ? word : [word];
      if (alts.indexOf(lower(peek())) < 0) {
        throw new CompileError('« ' + alts[0] + ' » attendu, trouvé « ' +
          (peek() || 'fin du script') + ' »', line());
      }
      return next();
    }

    function emit(...bytes) { bytes.forEach((b) => out.push(b & 0xFF)); }
    function emitU16(v) { emit(v & 0xFF, (v >> 8) & 0xFF); }
    function emitI32(v) {
      const raw = v >>> 0;
      emit(raw & 0xFF, (raw >> 8) & 0xFF, (raw >> 16) & 0xFF, (raw >> 24) & 0xFF);
    }
    // Reserve deux octets pour une cible pas encore connue, et rend l'adresse
    // du trou a reboucher.
    function emitJump(op) { emit(op); const hole = out.length; emitU16(0); return hole; }
    function patch(hole, target) {
      out[hole] = target & 0xFF;
      out[hole + 1] = (target >> 8) & 0xFF;
    }

    function varIndex(name) {
      const idx = VARS.indexOf(String(name).toLowerCase());
      if (idx < 0) {
        throw new CompileError('variable inconnue « ' + name + ' » (a à h)', line());
      }
      return idx;
    }

    function number() {
      const v = next();
      if (!/^\d+$/.test(v)) throw new CompileError('nombre attendu, trouvé « ' + v + ' »', line());
      return parseInt(v, 10);
    }

    function callArg() {
      expect('(');
      const id = number();
      expect(')');
      return id;
    }

    // Precedence : ou < et < comparaison < addition < multiplication < unaire
    function expression() { return orExpr(); }

    function orExpr() {
      andExpr();
      while (lower(peek()) === 'ou') { next(); andExpr(); emit(OP.OR); }
    }
    function andExpr() {
      cmpExpr();
      while (lower(peek()) === 'et') { next(); cmpExpr(); emit(OP.AND); }
    }
    function cmpExpr() {
      addExpr();
      const ops = { '==': OP.EQ, '!=': OP.NE, '<': OP.LT, '<=': OP.LE, '>': OP.GT, '>=': OP.GE };
      while (ops[peek()] !== undefined) {
        const op = ops[next()];
        addExpr();
        emit(op);
      }
    }
    function addExpr() {
      mulExpr();
      while (peek() === '+' || peek() === '-') {
        const op = next() === '+' ? OP.ADD : OP.SUB;
        mulExpr();
        emit(op);
      }
    }
    function mulExpr() {
      unary();
      while (peek() === '*' || peek() === '/' || peek() === '%') {
        const t = next();
        unary();
        emit(t === '*' ? OP.MUL : t === '/' ? OP.DIV : OP.MOD);
      }
    }
    function unary() {
      if (peek() === '-') { next(); unary(); emit(OP.NEG); return; }
      if (lower(peek()) === 'non') { next(); unary(); emit(OP.NOT); return; }
      primary();
    }
    function primary() {
      const t = peek();
      if (t === '(') { next(); expression(); expect(')'); return; }
      if (/^\d+$/.test(t)) { emit(OP.PUSH); emitI32(number()); return; }

      const word = lower(t);
      if (word === 'entree') { next(); emit(OP.READ_INPUT); emitU16(callArg()); return; }
      if (word === 'zoneactive') { next(); emit(OP.ZONE_ACTIVE); emitU16(callArg()); return; }
      if (word === 'reste') { next(); emit(OP.ZONE_REMAIN); emitU16(callArg()); return; }
      if (VARS.indexOf(word) >= 0) { next(); emit(OP.LOAD, varIndex(word)); return; }

      throw new CompileError('expression attendue, trouvé « ' + (t || 'fin du script') + ' »', line());
    }

    function zoneTarget() {
      expect('zone');
      return number();
    }

    function statement() {
      stmtLines.push({ pc: out.length, line: line() });
      const word = lower(peek());

      if (word === 'si') {
        next();
        expression();
        expect('alors');
        const toElse = emitJump(OP.JZ);
        block(['sinon', 'finsi']);
        if (lower(peek()) === 'sinon') {
          next();
          const toEnd = emitJump(OP.JMP);
          patch(toElse, out.length);
          block(['finsi']);
          patch(toEnd, out.length);
        } else {
          patch(toElse, out.length);
        }
        expect('finsi');
        return;
      }

      // parallele <branche 1> avec <branche 2> finparallele
      // Les deux branches partent ensemble ; la suite attend qu'elles soient
      // finies toutes les deux. Forme compilee : FORK Lb ; [1] ; JOIN ;
      // JMP fin ; Lb: [2] ; ENDBRANCH ; fin:
      if (word === 'parallele' || word === 'parallèle') {
        if (inParallel) {
          throw new CompileError('un bloc « parallele » ne peut pas en contenir un autre', line());
        }
        next();
        inParallel = true;
        emit(OP.FORK);
        const toBranch2 = out.length;
        emitU16(0);
        block(['avec']);
        expect('avec');
        emit(OP.JOIN);
        const toEnd = emitJump(OP.JMP);
        patch(toBranch2, out.length);
        block(['finparallele', 'finparallèle']);
        expect(['finparallele', 'finparallèle']);
        emit(OP.ENDBRANCH);
        patch(toEnd, out.length);
        inParallel = false;
        return;
      }

      if (word === 'tantque') {
        next();
        const top = out.length;
        expression();
        expect('faire');
        const toEnd = emitJump(OP.JZ);
        block(['fintantque']);
        emit(OP.JMP); emitU16(top);
        patch(toEnd, out.length);
        expect('fintantque');
        return;
      }

      if (word === 'pour') {
        next();
        const v = varIndex(next());
        expect('de');
        expression();
        emit(OP.STORE, v);
        expect(['à', 'a']);
        // La borne est evaluee a CHAQUE tour : c'est le comportement le moins
        // surprenant quand elle depend d'une entree qui bouge.
        const top = out.length;
        emit(OP.LOAD, v);
        expression();
        emit(OP.LE);
        expect('faire');
        const toEnd = emitJump(OP.JZ);
        block(['finpour']);
        emit(OP.LOAD, v); emit(OP.PUSH); emitI32(1); emit(OP.ADD); emit(OP.STORE, v);
        emit(OP.JMP); emitU16(top);
        patch(toEnd, out.length);
        expect('finpour');
        return;
      }

      if (word === 'attendre') { next(); expression(); emit(OP.WAIT); return; }
      if (word === 'message') { next(); emit(OP.NOTIFY); emitU16(number()); return; }
      // « notifier » reveille quelqu'un ; « message » ecrit une ligne. Deux
      // mots distincts parce que ce sont deux gestes distincts : confondre
      // les deux ferait envoyer une alerte a chaque trace de mise au point.
      if (word === 'notifier') { next(); emit(OP.ALERT); emitU16(number()); return; }

      if (word === 'demarrer') {
        next();
        const id = zoneTarget();
        expect('pendant');
        expression();
        emit(OP.ACTION, ACTION.ZONE_START); emitU16(id);
        return;
      }
      if (word === 'arreter' || word === 'suspendre' || word === 'reprendre') {
        next();
        const id = zoneTarget();
        // L'argument n'est pas utilise, mais l'instruction en depile un :
        // empiler zero garde la pile equilibree.
        emit(OP.PUSH); emitI32(0);
        emit(OP.ACTION,
             word === 'arreter' ? ACTION.ZONE_STOP
             : word === 'suspendre' ? ACTION.ZONE_PAUSE : ACTION.ZONE_RESUME);
        emitU16(id);
        return;
      }

      // Le script lance part EN PARALLELE : celui-ci continue aussitot. Un
      // lancement que le module ne peut pas honorer (place prise, script
      // inactif...) est journalise, sans arreter ce script.
      if (word === 'lancer') {
        next();
        expect('script');
        const ln = line();
        const n = number();
        if (n < 1 || n > MAX_SCRIPTS) {
          throw new CompileError('script ' + n + ' : les emplacements vont de 1 à ' + MAX_SCRIPTS, ln);
        }
        emit(OP.PUSH); emitI32(0);
        emit(OP.ACTION, ACTION.SCRIPT_RUN);
        emitU16(n);
        return;
      }

      if (VARS.indexOf(word) >= 0) {
        const v = varIndex(next());
        expect('=');
        expression();
        emit(OP.STORE, v);
        return;
      }

      throw new CompileError('instruction inconnue « ' + (peek() || 'fin du script') + ' »', line());
    }

    function block(stops) {
      while (peek() !== null && stops.indexOf(lower(peek())) < 0) statement();
      if (peek() === null && stops.length) {
        throw new CompileError('« ' + stops[stops.length - 1] + ' » manquant', line());
      }
    }

    block([]);
    emit(OP.HALT);
    // Propriete ajoutee sur le TABLEAU retourne, pas un second element : tout
    // appelant qui traite le resultat comme un simple tableau d'octets
    // (code.length, code.map(...), JSON.stringify...) ne voit aucune
    // difference. Seul un appelant qui la demande explicitement (out.lineMap)
    // la voit.
    out.lineMap = stmtLines;
    return out;
  }

  global.AquaScript = { compile: compile, CompileError: CompileError, OP: OP };
})(window);
