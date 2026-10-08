#!/usr/bin/env python3
"""Banc de l'editeur de scripts (data/scripts-schema.html), sans module.

Assemble dans un dossier temporaire une copie de la page avec un module
simule (tools/editeur_banc/module_simule.js, qui remplace fetch) et les
verifications (tools/editeur_banc/verifications.js), puis l'ouvre dans Edge
sans affichage et lit le bloc <pre id="RESULTATS">.

    python tools/editeur_banc.py            # resume + echecs
    python tools/editeur_banc.py -v         # toutes les lignes
    python tools/editeur_banc.py --edge "C:/chemin/msedge.exe"

Code de sortie : 0 si tout passe, 1 sinon (ou si le banc n'a pas pu tourner).
Rien n'est ecrit dans le depot ; aucune requete reseau n'est emise.
"""

import argparse
import html
import os
import re
import shutil
import subprocess
import sys
import tempfile

RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(RACINE, "data")
BANC = os.path.join(RACINE, "tools", "editeur_banc")
PAGE = "scripts-schema.html"

EDGE_CANDIDATS = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    "msedge", "microsoft-edge", "chromium", "google-chrome",
]


def trouver_edge(explicite):
    for c in ([explicite] if explicite else EDGE_CANDIDATS):
        if os.path.isfile(c):
            return c
        trouve = shutil.which(c)
        if trouve:
            return trouve
    return None


def lire(chemin):
    with open(chemin, encoding="utf-8") as f:
        return f.read()


def assembler(dossier):
    page = lire(os.path.join(DATA, PAGE))
    simule = lire(os.path.join(BANC, "module_simule.js"))
    verifs = lire(os.path.join(BANC, "verifications.js"))

    # Le module simule doit passer avant le premier script de la page, pour
    # que fetch soit remplace avant tout appel ; les verifications en dernier.
    premier = page.find("<script")
    fin = page.rfind("</body>")
    if premier < 0 or fin < 0:
        raise RuntimeError(PAGE + " : <script> ou </body> introuvable")
    page = (page[:premier] + "<script>" + simule + "</script>\n"
            + page[premier:fin] + "<script>" + verifs + "</script>\n"
            + page[fin:])
    with open(os.path.join(dossier, PAGE), "w", encoding="utf-8") as f:
        f.write(page)

    # Ressources locales citees par la page (js/css), copiees depuis data/.
    for nom in sorted(set(re.findall(r'(?:src|href)="([\w.-]+\.(?:js|css))"', page))):
        source = os.path.join(DATA, nom)
        if not os.path.isfile(source):
            raise RuntimeError("ressource citee absente de data/ : " + nom)
        shutil.copy(source, os.path.join(dossier, nom))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("-v", "--verbeux", action="store_true")
    ap.add_argument("--edge", help="chemin de msedge.exe (ou Chromium)")
    args = ap.parse_args()
    # Console Windows en cp1252 : les libelles accentues de la page restent lisibles.
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    edge = trouver_edge(args.edge)
    if not edge:
        print("Edge/Chromium introuvable : preciser --edge")
        return 1

    with tempfile.TemporaryDirectory(prefix="aqualook-banc-") as tmp:
        assembler(tmp)
        url = "file:///" + os.path.join(tmp, PAGE).replace(os.sep, "/")
        # virtual-time-budget avance les minuteries (le message ephemere de
        # 3 s est verifie) sans attendre en temps reel ; la fenetre fixe la
        # hauteur dont depend la verification « pied visible sans defiler ».
        cmd = [edge, "--headless", "--disable-gpu", "--no-first-run",
               "--user-data-dir=" + os.path.join(tmp, "profil"),
               "--allow-file-access-from-files", "--window-size=1280,800",
               "--virtual-time-budget=30000", "--dump-dom", url]
        try:
            sortie = subprocess.run(cmd, capture_output=True, timeout=120,
                                    encoding="utf-8", errors="replace").stdout
        except subprocess.TimeoutExpired:
            print("Edge n'a pas rendu la page en 120 s")
            return 1

    # Le dernier bloc : le texte des verifications injectees cite aussi la
    # balise, mais seul le bloc ajoute a la fin par la page porte le resultat.
    debut = sortie.rfind('<pre id="RESULTATS">')
    fin = sortie.find("</pre>", debut)
    if debut < 0 or fin < 0:
        print("Pas de bloc RESULTATS : la page n'a pas termine ses verifications")
        return 1
    lignes = html.unescape(sortie[debut + len('<pre id="RESULTATS">'):fin]).splitlines()
    tete, detail = lignes[0], lignes[1:]
    for ligne in detail:
        if args.verbeux or not ligne.startswith("OK"):
            print(ligne)
    nb_ok = sum(1 for l in detail if l.startswith("OK"))
    print("%s (%d/%d)" % (tete, nb_ok, len(detail)))
    return 0 if tete == "TOUT OK" else 1


if __name__ == "__main__":
    sys.exit(main())
