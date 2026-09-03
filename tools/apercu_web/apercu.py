"""Apercu de l'espace utilisateur a une largeur donnee, sans serveur ni session.

Pourquoi : chaque defaut de mise en page de app.html a ete decouvert par
l'utilisateur, sur son telephone, apres un televersement FTP -- puis corrige a
l'aveugle sur la foi d'une capture d'ecran. Une boucle de plusieurs minutes pour
une regle CSS.

Ce banc rend la page dans Edge en mode sans fenetre, avec des donnees fictives a
la place du serveur, et produit une image. Il signale en plus, en clair sur
l'image, tout element plus large que la fenetre : la premiere version de cette
verification concluait a un debordement qui n'existait pas, la capture etant
simplement plus etroite que la fenetre rendue.

Le cadre : Edge sans fenetre ignore --window-size pour la largeur de mise en
page (bloquee a 492 px sur ce poste). La page est donc placee dans un <iframe>
de la largeur voulue, qui lui possede bien son propre contexte de mise en page.

Limite a connaitre : fixtures.js decrit une installation imaginaire et doit
suivre les evolutions du contrat de l'API. Si un ecran s'affiche vide, c'est
probablement une reponse absente de ces donnees, pas un defaut de la page.

Usage :
    python tools/apercu_web/apercu.py                     # les trois vues, 390 px
    python tools/apercu_web/apercu.py --largeur 768       # tablette
    python tools/apercu_web/apercu.py --vue jour
"""
import argparse
import pathlib
import subprocess
import sys
import tempfile

RACINE = pathlib.Path(__file__).resolve().parents[2]
ICI = pathlib.Path(__file__).resolve().parent
PAGE = RACINE / "cloud" / "php-mutualized" / "app.html"

EDGE = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
]

# Point d'entree de la page, remplace par le banc.
AMORCE = "demarrer().catch(() => { $('login').hidden = false; });"

# Ce que chaque vue ouvre apres chargement, et la hauteur de rendu.
VUES = {
    "page": ("", 1500),
    "jour": ("ouvrirJour(0, 2);", 920),
    "zone": ("ouvrirZone(0);", 800),
    "sauvegarde": ("ouvrirSauvegarde(9);", 1120),
}


def edge():
    for chemin in EDGE:
        if pathlib.Path(chemin).exists():
            return chemin
    print("Microsoft Edge est introuvable ; ce banc en depend.", file=sys.stderr)
    raise SystemExit(2)


def construire(travail, vue, largeur):
    """Ecrit la page instrumentee et son cadre. Rend le chemin du cadre."""
    source = PAGE.read_text(encoding="utf-8")
    if AMORCE not in source:
        raise SystemExit("Le point d'entree de app.html a change : "
                         "adapter AMORCE dans ce fichier.")

    fixtures = (ICI / "fixtures.js").read_text(encoding="utf-8")
    diagnostic = (ICI / "diagnostic.js").read_text(encoding="utf-8")

    ouvrir, hauteur = VUES[vue]
    fixtures = fixtures.replace(
        "    document.body.dataset.pret = '1';",
        "    " + ouvrir + "\n    document.body.dataset.pret = '1';"
        "\n    window.APERCU_DIAG();")

    (travail / "page.html").write_text(
        source.replace(AMORCE, diagnostic + fixtures, 1), encoding="utf-8")
    cadre = travail / "cadre.html"
    cadre.write_text(
        '<!doctype html><meta charset="utf-8">'
        '<style>html,body{margin:0;background:#333}'
        'iframe{border:0;display:block;background:#fff}</style>'
        '<iframe src="page.html" width="%d" height="%d"></iframe>'
        % (largeur, hauteur), encoding="utf-8")
    return cadre, hauteur


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--largeur", type=int, default=390,
                    help="largeur de mise en page en pixels CSS (defaut 390)")
    ap.add_argument("--vue", choices=sorted(VUES), action="append",
                    help="vue a rendre ; repetable, toutes par defaut")
    ap.add_argument("--sortie", default=str(RACINE / "docs" / "apercus"),
                    help="dossier ou deposer les images")
    args = ap.parse_args()

    vues = args.vue or sorted(VUES)
    sortie = pathlib.Path(args.sortie)
    sortie.mkdir(parents=True, exist_ok=True)
    navigateur = edge()

    with tempfile.TemporaryDirectory() as tmp:
        travail = pathlib.Path(tmp)
        for vue in vues:
            cadre, hauteur = construire(travail, vue, args.largeur)
            image = sortie / ("app-%s-%dpx.png" % (vue, args.largeur))
            subprocess.run(
                [navigateur, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                 "--allow-file-access-from-files", "--virtual-time-budget=6000",
                 "--window-size=%d,%d" % (args.largeur + 10, hauteur + 20),
                 "--user-data-dir=" + str(travail / "profil"),
                 "--screenshot=" + str(image), cadre.as_uri()],
                capture_output=True, check=False)
            etat = "%d octets" % image.stat().st_size if image.exists() else "ECHEC"
            print("%-12s -> %s (%s)" % (vue, image, etat))


if __name__ == "__main__":
    main()
