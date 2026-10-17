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
manifeste annonce. Sans --envoyer il ne televerse rien. Avec --envoyer
(P2.18), il televerse en FTPS dans l'ordre impose -- fichiers, relus par FTP
puis en HTTPS, ensuite le .sig, le manifeste en dernier -- et termine par
check_published_web.py. Publier reste un acte du proprietaire : --envoyer ne
se lance qu'avec son accord explicite. Identifiants : ceux de deploy_cloud.py
(.env a la racine du depot, jamais versionne).

dist/ est ignore par git : c'est une zone de preparation, pas une source.
Sa forme reproduit exactement celle du serveur, de sorte que televerser le
contenu de dist/alwaysdata a la racine du site suffit.

Usage :
    python tools/publish_web_assets.py 5.9.18
    python tools/publish_web_assets.py 5.9.18 --comparer   # + diff avec le serveur
    python tools/publish_web_assets.py 5.9.18 --envoyer    # + envoi FTPS, /editeur/ compris
"""
import argparse
import ftplib
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import urllib.request

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import update_signing  # noqa: E402  (D018)
import deploy_cloud  # noqa: E402  (connexion FTPS, envoi sous nom temporaire)

RACINE = pathlib.Path(__file__).resolve().parents[1]
DATA = RACINE / "data"
DIST = RACINE / "dist" / "alwaysdata"
SITE = "https://aqualook.alwaysdata.net"
MANIFESTE = "aqualook-web-manifest.json"
# Les fichiers de l'editeur de scripts, recopies dans /editeur/ sur le site
# (AGENTS.md, section LittleFS) : l'editeur en ligne n'a que ceux-la.
EDITEUR = ("scripts-schema.html", "script-editeur.js", "script-lang.js",
           "script-schema.js", "session.js", "hmac.js", "style-base.css",
           "style-editeur.css")


def generer(version):
    """Copie data/ puis produit le manifeste. Rend le chemin du manifeste."""
    cible = DIST / "web" / ("v" + version)
    if cible.exists():
        shutil.rmtree(cible)
    cible.mkdir(parents=True)

    fichiers = sorted(p for p in DATA.iterdir() if p.is_file())
    convertis = 0
    for p in fichiers:
        octets = p.read_bytes()
        # Fins de ligne normalisees en LF dans la copie preparee, pas dans
        # data/ : un client FTP en mode ASCII retire les CR au transfert, et
        # le fichier servi ne correspond alors plus a l'empreinte annoncee.
        # Constate le 3 septembre 2026 sur app.js et index.html, que Python
        # avait reecrits en CRLF (write_text ouvre en mode texte sous
        # Windows). Un fichier deja en LF traverse les deux modes intact.
        if b'\r\n' in octets:
            octets = octets.replace(b'\r\n', b'\n')
            convertis += 1
        (cible / p.name).write_bytes(octets)
    if convertis:
        print('%d fichier(s) normalises en LF pour la publication' % convertis)
    print("%d fichier(s) copies dans %s" % (len(fichiers), cible.relative_to(RACINE)))

    sortie = DIST / MANIFESTE
    # Le manifeste est produit APRES la copie et A PARTIR DE LA COPIE, jamais
    # depuis data/. Les deux different des qu'une normalisation a eu lieu, et
    # ce sont les octets deposes qui seront televerses, donc eux qu'il faut
    # decrire.
    r = subprocess.run(
        [sys.executable, str(RACINE / "tools" / "generate_web_manifest.py"),
         "--version", version, "--tag", "v" + version,
         "--base-url", "%s/web/v%s" % (SITE, version),
         "--data-dir", str(cible), "--output", str(sortie)],
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


def aligner(ftp, fichiers, titre):
    """Envoie les fichiers absents ou differents du dossier FTP courant, puis
    relit tout par FTP. Ne s'arrete pas sur un fichier identique."""
    print(titre)
    ecarts = deploy_cloud.comparer(ftp, fichiers)
    if ecarts:
        deploy_cloud.publier(ftp, fichiers, ecarts)
        if deploy_cloud.comparer(ftp, fichiers):
            raise SystemExit("Relecture FTP en ecart : arret, manifeste NON envoye.")
    return ecarts


def relire_https(man):
    """Relit chaque fichier a l'adresse que le module utilisera."""
    for f in man["files"]:
        try:
            octets = urllib.request.urlopen(f["url"], timeout=30).read()
        except Exception as e:
            raise SystemExit("%s illisible en HTTPS (%s) : manifeste NON envoye." % (f["name"], e))
        if hashlib.sha256(octets).hexdigest() != f["sha256"]:
            raise SystemExit("%s servi different en HTTPS : manifeste NON envoye." % f["name"])
    print("  relus en HTTPS : %d/%d conformes" % (len(man["files"]), len(man["files"])))


def envoyer(version, man, dossier, manifeste, signature, banc):
    fichiers = {f["name"]: (dossier / f["name"]).read_bytes() for f in man["files"]}
    rep = "web/v" + version
    ftp = deploy_cloud.connecter(banc)
    try:
        racine = ftp.pwd()
        for d in ("web", rep):
            try:
                ftp.mkd(d)
            except ftplib.error_perm:
                pass  # deja present
        ftp.cwd(rep)
        # Une version deja publiee ne se reecrit pas : un module peut etre en
        # train de la telecharger. Changer de numero de version.
        autres = [n for n, o in fichiers.items()
                  if deploy_cloud.lire_distant(ftp, n) not in (None, o)]
        if autres:
            raise SystemExit("/%s/ existe avec un autre contenu (%s) : changer de version."
                             % (rep, ", ".join(autres)))
        aligner(ftp, fichiers, "1. Fichiers du paquet -> /%s/ :" % rep)
        if not banc:
            relire_https(man)

        ftp.cwd(racine)
        racine_fichiers = {signature.name: signature.read_bytes(),
                           manifeste.name: manifeste.read_bytes()}
        print("2. Signature puis 3. manifeste -> racine du site :")
        # Ordre de la liste = ordre d'envoi : le manifeste en dernier.
        deploy_cloud.publier(ftp, racine_fichiers, [signature.name, manifeste.name])
        if deploy_cloud.comparer(ftp, racine_fichiers):
            raise SystemExit("Relecture FTP du manifeste en ecart.")

        try:
            ftp.mkd("editeur")
        except ftplib.error_perm:
            pass
        ftp.cwd("editeur")
        aligner(ftp, {n: fichiers[n] for n in EDITEUR}, "4. Editeur en ligne -> /editeur/ :")
    finally:
        try:
            ftp.quit()
        except Exception:
            ftp.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("version", help="version des ressources, par exemple 5.9.18")
    ap.add_argument("--comparer", action="store_true",
                    help="interroger le serveur pour lister ce qui changera")
    ap.add_argument("--envoyer", action="store_true",
                    help="televerser en FTPS (accord du proprietaire), puis verifier")
    ap.add_argument("--banc", action="store_true", help=argparse.SUPPRESS)
    args = ap.parse_args()

    manifeste, dossier = generer(args.version)
    man = verifier(manifeste, dossier)
    # D018 : signature du manifeste final (cle privee hors depot), puis
    # relecture avec les cles publiques integrees au firmware. Sans elle, un
    # module a jour refuse la publication (signature-absente).
    signature = update_signing.signer(manifeste)
    if not update_signing.verifier(manifeste, signature):
        raise SystemExit("Signature non verifiable : ne pas televerser.")
    if args.comparer:
        comparer_au_serveur(man)

    if args.envoyer:
        envoyer(args.version, man, dossier, manifeste, signature, args.banc)
        if args.banc:
            print("Banc : envoi termine (pas de relecture HTTPS).")
            return
        r = subprocess.run([sys.executable, str(RACINE / "tools" / "check_published_web.py"),
                            "%s/%s" % (SITE, MANIFESTE)])
        raise SystemExit(r.returncode)

    print()
    print("A televerser, DANS CET ORDRE :")
    print("  1. %s  ->  %s/web/v%s/"
          % (dossier.relative_to(RACINE), SITE, args.version))
    print("  2. %s  ->  racine du site"
          % signature.relative_to(RACINE))
    print("  3. %s  ->  racine du site"
          % manifeste.relative_to(RACINE))
    print()
    print("Le manifeste en dernier, toujours : publie en premier, il annonce")
    print("une version dont aucun fichier n'est encore en ligne. Entre 2 et 3,")
    print("un module qui verifierait verrait une signature qui ne correspond")
    print("pas encore : il refuse et ne change rien, c'est sans danger.")
    print("Ensuite : python tools/check_published_web.py")


if __name__ == "__main__":
    main()
