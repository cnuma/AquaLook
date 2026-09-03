"""Controle de coherence du JavaScript embarque dans une page HTML.

Il n'y a pas d'interpreteur JavaScript sur ce poste, et les pages de ce projet
portent leur script en ligne. Une accolade oubliee dans app.html ne se voit donc
qu'a l'execution, dans le navigateur, apres un televersement FTP -- c'est-a-dire
trop tard et trop loin.

Un simple comptage de caracteres ne suffit pas : les commentaires de ce projet
sont en francais et contiennent des apostrophes ("l'etat", "n'est"), que tout
comptage naif prend pour des debuts de chaine. C'est exactement ce qui a rendu
un premier controle inutilisable. Il faut donc un vrai balayage lexical.

Ce que ce fichier detecte : chaine ou commentaire non termine, delimiteur
desequilibre, et l'endroit ou cela commence. Ce qu'il ne detecte pas : les
fautes de grammaire du langage (un `if` sans condition passe). C'est un
garde-fou, pas un compilateur.

Usage :  python tools/check_inline_js.py <fichier.html> [...]
Sortie :  code 0 si tout est coherent, 1 sinon.
"""
import sys
import pathlib

# Caracteres apres lesquels un '/' ouvre une expression reguliere et non une
# division. Approximation classique, suffisante ici : on ne divise jamais par
# le resultat d'une parenthese fermante suivie d'un litteral regex.
AVANT_REGEX = set("(,=:[!&|?{};+-*%~^<>") | {"\n"}

PAIRES = {"{": "}", "(": ")", "[": "]"}


def scripts(html):
    """Rend les blocs <script>...</script> avec leur ligne de depart."""
    out = []
    pos = 0
    while True:
        d = html.find("<script", pos)
        if d < 0:
            return out
        d = html.find(">", d)
        if d < 0:
            return out
        f = html.find("</script>", d)
        if f < 0:
            return out
        out.append((html.count("\n", 0, d) + 1, html[d + 1:f]))
        pos = f + 9


def verifier(source, ligne0):
    """Balaye un bloc et rend la liste des anomalies (ligne, message)."""
    anomalies = []
    pile = []
    i = 0
    n = len(source)
    ligne = ligne0
    # Dernier caractere significatif rencontre : sert a decider si '/' ouvre
    # une expression reguliere.
    precedent = "\n"

    while i < n:
        c = source[i]

        if c == "\n":
            ligne += 1
            i += 1
            continue

        if c in " \t\r":
            i += 1
            continue

        # ── Commentaires ────────────────────────────────────────────────
        if c == "/" and i + 1 < n and source[i + 1] == "/":
            j = source.find("\n", i)
            i = n if j < 0 else j
            continue
        if c == "/" and i + 1 < n and source[i + 1] == "*":
            j = source.find("*/", i + 2)
            if j < 0:
                anomalies.append((ligne, "commentaire /* jamais ferme"))
                break
            ligne += source.count("\n", i, j)
            i = j + 2
            continue

        # ── Chaines ─────────────────────────────────────────────────────
        if c in "'\"`":
            depart = ligne
            fin = c
            i += 1
            ferme = False
            while i < n:
                d = source[i]
                if d == "\\":
                    i += 2
                    continue
                if d == "\n":
                    ligne += 1
                    if fin != "`":
                        # Un retour a la ligne dans une chaine simple est une
                        # erreur de syntaxe : on la signale la ou elle commence.
                        anomalies.append(
                            (depart, "chaine %s non fermee en fin de ligne" % fin))
                        ferme = True
                        break
                    i += 1
                    continue
                if d == fin:
                    ferme = True
                    i += 1
                    break
                i += 1
            if not ferme:
                anomalies.append((depart, "chaine %s jamais fermee" % fin))
                break
            precedent = fin
            continue

        # ── Expressions regulieres ──────────────────────────────────────
        if c == "/" and precedent in AVANT_REGEX:
            depart = ligne
            i += 1
            classe = False
            ferme = False
            while i < n:
                d = source[i]
                if d == "\\":
                    i += 2
                    continue
                if d == "[":
                    classe = True
                elif d == "]":
                    classe = False
                elif d == "\n":
                    break
                elif d == "/" and not classe:
                    ferme = True
                    i += 1
                    break
                i += 1
            if not ferme:
                anomalies.append((depart, "expression reguliere jamais fermee"))
                break
            precedent = "/"
            continue

        # ── Delimiteurs ─────────────────────────────────────────────────
        if c in PAIRES:
            pile.append((c, ligne))
        elif c in PAIRES.values():
            if not pile:
                anomalies.append((ligne, "'%s' sans ouverture correspondante" % c))
            else:
                ouv, lo = pile.pop()
                if PAIRES[ouv] != c:
                    anomalies.append(
                        (ligne, "'%s' ferme un '%s' ouvert ligne %d" % (c, ouv, lo)))

        precedent = c
        i += 1

    for ouv, lo in pile:
        anomalies.append((lo, "'%s' jamais ferme" % ouv))
    return anomalies


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    faute = False
    for chemin in argv[1:]:
        p = pathlib.Path(chemin)
        html = p.read_text(encoding="utf-8")
        blocs = scripts(html)
        if not blocs:
            print("%s : aucun bloc <script>" % p.name)
            continue
        total = 0
        for ligne0, source in blocs:
            for ligne, msg in verifier(source, ligne0):
                print("%s:%d: %s" % (p.name, ligne, msg))
                total += 1
        if total:
            faute = True
        else:
            print("%s : %d bloc(s) <script>, coherent" % (p.name, len(blocs)))
    return 1 if faute else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
