"""Prepare une publication des ressources Web dans dist/alwaysdata.

Trois erreurs ont deja ete commises a la main sur cette chaine, chacune
detectee seulement une fois le module en echec :

- manifeste publie AVANT les fichiers -> le module annonce une version dont
  aucun fichier n'est en ligne, et recolte onze 404 ;
- manifeste regenere APRES avoir modifie un fichier -> empreinte fausse,
  content-length-mismatch a la verification ;
- fichiers deposes ailleurs que dans dist/alwaysdata -> publication
  introuvable, alors que tout semblait pret.

Ce script fait les trois choses dans le bon ordre et verifie son propre
travail : chaque fichier depose est relu et compare a l'empreinte que le
manifeste annonce. Il ne televerse rien -- le FTP reste manuel, volontairement,
puisque publier est un acte que l'utilisateur declenche.

dist/ est ignore par git : c'est une zone de preparation, pas une source.
Sa forme reproduit exactement celle du serveur, de sorte que televerser le
contenu de dist/alwaysdata a la racine du site suffit.

Usage :
    python tools/publish_web_assets.py 5.9.18
    python tools/publish_web_assets.py 5.9.18 --comparer   # + diff avec le serveur
"""
import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import urllib.request

RACINE = pathlib.Path(__file__).resolve().parents[1]
DATA = RACINE / "data"
DIST = RACINE / "dist" / "alwaysdata"
SITE = "https://aqualook.alwaysdata.net"
MANIFESTE = "aqualook-web-manifest.json"


def generer(version):
    """Copie data/ puis produit le manifeste. Rend le chemin du manifeste."""
    cible = DIST / "web" / ("v" + version)
    if cible.exists():
        shutil.rmtree(cible)
    cible.mkdir(parents=True)

    fichiers = sorted(p for p in DATA.iterdir() if p.is_file())
    for p in fichiers:
        shutil.copy2(p, cible / p.name)
    print("%d fichier(s) copies dans %s" % (len(fichiers), cible.relative_to(RACINE)))

    sortie = DIST / MANIFESTE
    # Le manifeste est produit APRES la copie, jamais avant : c'est ce qui
    # garantit que les empreintes decrivent bien les octets deposes.
    r = subprocess.run(
        [sys.executable, str(RACINE / "tools" / "generate_web_manifest.py"),
         "--version", version, "--tag", "v" + version,
         "--base-url", "%s/web/v%s" % (SITE, version),
         "--data-dir", str(DATA), "--output", str(sortie)],
        capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit("generate_web_manifest.py a echoue")
    return sortie, cible


def verifier(manifeste, dossier):
    """Relit chaque fichier depose et le compare au manifeste."""
    man = json.loads(manifeste.read_text(encoding="utf-8"))
    ecarts = 0
    for f in man["files"]:
        chemin = dossier / f["name"]
        if not chemin.exists():
            print("  MANQUANT   %s" % f["name"])
            ecarts += 1
            continue
        octets = chemin.read_bytes()
        if len(octets) != f["size"]:
            print("  TAILLE     %s : %d au lieu de %d"
                  % (f["name"], len(octets), f["size"]))
            ecarts += 1
        elif hashlib.sha256(octets).hexdigest() != f["sha256"]:
            print("  EMPREINTE  %s" % f["name"])
            ecarts += 1
    if ecarts:
        raise SystemExit("%d ecart(s) : ne pas televerser cette publication." % ecarts)
    print("Verification : les %d fichiers correspondent au manifeste."
          % len(man["files"]))
    return man


def comparer_au_serveur(man):
    """Dit ce que le module retelechargera reellement."""
    try:
        publie = json.load(urllib.request.urlopen(
            "%s/%s" % (SITE, MANIFESTE), timeout=15))
    except Exception as e:
        print("Comparaison impossible (%s). Ce n'est pas bloquant." % e)
        return
    ancien = {f["name"]: f["sha256"] for f in publie["files"]}
    print("En ligne : %s  ->  a publier : %s"
          % (publie["release"]["version"], man["release"]["version"]))
    changes = [f for f in man["files"] if ancien.get(f["name"]) != f["sha256"]]
    if not changes:
        print("Aucun fichier modifie : le module n'aura rien a telecharger.")
        return
    print("Le module retelechargera :")
    for f in changes:
        print("  %-18s %7d octets" % (f["name"], f["size"]))
    gardes = [f["name"] for f in man["files"] if ancien.get(f["name"]) == f["sha256"]]
    if gardes:
        print("Conserves tels quels : %s" % ", ".join(gardes))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("version", help="version des ressources, par exemple 5.9.18")
    ap.add_argument("--comparer", action="store_true",
                    help="interroger le serveur pour lister ce qui changera")
    args = ap.parse_args()

    manifeste, dossier = generer(args.version)
    man = verifier(manifeste, dossier)
    if args.comparer:
        comparer_au_serveur(man)

    print()
    print("A televerser, DANS CET ORDRE :")
    print("  1. %s  ->  %s/web/v%s/"
          % (dossier.relative_to(RACINE), SITE, args.version))
    print("  2. %s  ->  racine du site"
          % manifeste.relative_to(RACINE))
    print()
    print("Le manifeste en dernier, toujours : publie en premier, il annonce")
    print("une version dont aucun fichier n'est encore en ligne.")
    print("Ensuite : python tools/check_published_web.py")


if __name__ == "__main__":
    main()
