#!/usr/bin/env python3
"""Verification, signature et publication d'une release du programme (P1.4).

GitHub Actions compile le binaire de production et cree une release
BROUILLON (.github/workflows/ota-release.yml), invisible des modules. La cle
de signature reste hors ligne (D018) : ce script, lance sur le poste du
proprietaire, termine la publication.

  python tools/release_firmware.py verifier X.Y.Z
      telecharge le manifeste et le binaire de la release vX.Y.Z (brouillon
      ou publiee), controle tout ce que le module controlera, ne change rien.

  python tools/release_firmware.py signer X.Y.Z
      verifier, puis signe le manifeste (aqualook-manifest.json.sig), ajoute
      la signature a la release, la publie comme derniere release si c'est
      un brouillon, et relit ce que les modules recevront.

  python tools/release_firmware.py publier-sans-signature X.Y.Z
      ESSAI SEULEMENT : publie le brouillon sans .sig, pour constater que le
      module refuse (signature-absente). Signer ensuite avec 'signer'.

Prerequis : gh (GitHub CLI) connecte au compte du depot, paquet Python
'cryptography', cle privee (voir tools/update_signing.py).
"""
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import generate_ota_manifest as gen  # noqa: E402
import update_signing as signing  # noqa: E402

DEPOT = "cnuma/AquaLook"
MANIFESTE = "aqualook-manifest.json"
SIG = MANIFESTE + ".sig"
# Ce que lit le module (src/OtaBuildIdentity.h, MANIFEST_PATH).
URL_MODULES = "https://github.com/%s/releases/latest/download/" % DEPOT
# En-tete d'image ESP-IDF : octet magique 0xE9, identifiant de puce a
# l'octet 12 (petit-boutiste), 9 = ESP32-S3.
IMAGE_MAGIC = 0xE9
CHIP_ID_S3 = 9


def gh(*args, capture=True):
    r = subprocess.run(["gh", *args, "-R", DEPOT], capture_output=capture, text=True)
    if r.returncode != 0:
        sys.exit("gh %s : echec\n%s" % (" ".join(args), (r.stderr or "").strip()))
    return r.stdout


def etat_release(tag):
    infos = json.loads(gh("release", "view", tag, "--json", "isDraft,assets"))
    return infos["isDraft"], [a["name"] for a in infos["assets"]]


def telecharger(tag, dossier, noms):
    args = ["release", "download", tag, "--dir", str(dossier), "--clobber"]
    for n in noms:
        args += ["--pattern", n]
    gh(*args)


def controler(version, dossier):
    """Rend les octets exacts du manifeste si tout est conforme, sinon quitte."""
    tag = "v" + version
    erreurs = []
    brut = (dossier / MANIFESTE).read_bytes()
    man = json.loads(brut)
    binaire_nom = "AquaLook-s3-%s.bin" % version
    binaire = (dossier / binaire_nom).read_bytes()

    def exiger(cond, msg):
        print(("  ok    " if cond else "  ECHEC ") + msg)
        if not cond:
            erreurs.append(msg)

    exiger(len(brut) <= 8192, "manifeste <= 8192 octets (%d)" % len(brut))
    exiger(man.get("schema") == gen.SCHEMA, "schema %s" % gen.SCHEMA)
    rel = man.get("release", {})
    exiger(rel.get("version") == version, "version %s" % version)
    exiger(rel.get("channel") == "stable", "canal stable")
    cibles = man.get("targets", {})
    exiger(list(cibles) == [gen.TARGET], "une seule cible : %s" % gen.TARGET)
    c = cibles.get(gen.TARGET, {})
    exiger(c.get("board") == gen.BOARD, "carte %s" % gen.BOARD)
    exiger(c.get("environment") == gen.ENVIRONMENT, "environnement %s" % gen.ENVIRONMENT)
    url = "https://github.com/%s/releases/download/%s/%s" % (DEPOT, tag, binaire_nom)
    exiger(c.get("firmwareUrl") == url, "adresse du binaire = cette release")
    exiger(c.get("size") == len(binaire), "taille du binaire (%d octets)" % len(binaire))
    exiger(len(binaire) <= gen.MAX_FIRMWARE_SIZE, "binaire <= emplacement OTA (%d)" % gen.MAX_FIRMWARE_SIZE)
    exiger(c.get("sha256") == hashlib.sha256(binaire).hexdigest(), "empreinte SHA-256 du binaire")
    exiger(len(binaire) > 16 and binaire[0] == IMAGE_MAGIC, "image ESP valide")
    exiger(len(binaire) > 16 and int.from_bytes(binaire[12:14], "little") == CHIP_ID_S3,
           "image pour ESP32-S3")
    exiger((version.encode() + b"\0") in binaire, "binaire en version %s" % version)
    exiger(b"ProgrammeArrosage_s3\0" in binaire, "binaire de production (ProgrammeArrosage_s3)")
    exiger(b"ProgrammeArrosage_s3_banc\0" not in binaire, "pas le binaire de banc")
    if erreurs:
        sys.exit("\nRelease %s NON CONFORME : rien n'a ete signe ni publie." % tag)
    return brut


def preparer(version, dossier, avec_sig=False):
    if not gen.VERSION_PATTERN.fullmatch(version):
        sys.exit("Version invalide : %r (forme attendue 5.17.0)" % version)
    tag = "v" + version
    brouillon, assets = etat_release(tag)
    print("Release %s : %s ; fichiers : %s" % (tag, "BROUILLON" if brouillon else "PUBLIEE",
                                               ", ".join(assets)))
    noms = [MANIFESTE, "AquaLook-s3-%s.bin" % version]
    if avec_sig and SIG in assets:
        noms.append(SIG)
    telecharger(tag, dossier, noms)
    return tag, brouillon, assets


def cmd_verifier(version):
    with tempfile.TemporaryDirectory() as d:
        dossier = pathlib.Path(d)
        tag, _, assets = preparer(version, dossier, avec_sig=True)
        brut = controler(version, dossier)
        if SIG in assets:
            ok, motif = signing.verifier_octets(brut, (dossier / SIG).read_text("ascii", "replace"))
            print(("  ok    " if ok else "  ECHEC ") + "signature : " + motif)
            if not ok:
                sys.exit(1)
        else:
            print("  --    pas encore de signature")
    print("Release %s conforme." % tag)


def relire_cote_modules(brut):
    """Ce que les modules recevront (derniere release publique)."""
    def lire(nom):
        try:
            with urllib.request.urlopen(URL_MODULES + nom, timeout=30) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            return "http-%d" % e.code
    # Le cache de GitHub sert parfois l'etat precedent pendant ~1 min
    # (constate le 14 oct. 2026) : on relit jusqu'a 2 min avant de conclure.
    for essai in range(7):
        recu = lire(MANIFESTE)
        sig = lire(SIG)
        if recu == brut and isinstance(sig, bytes):
            break
        if essai < 6:
            time.sleep(20)
    if recu != brut:
        print("  ATTENTION : le manifeste servi aux modules n'est pas encore celui-ci "
              "(cache GitHub ?). Relancer 'verifier' dans une minute.")
        return
    if not isinstance(sig, bytes):
        print("  Modules : manifeste servi, signature absente (%s)." % sig)
        return
    ok, motif = signing.verifier_octets(recu, sig.decode("ascii", "replace"))
    print("  Modules : manifeste et signature servis, %s." % motif)


def cmd_signer(version):
    with tempfile.TemporaryDirectory() as d:
        dossier = pathlib.Path(d)
        tag, brouillon, _ = preparer(version, dossier)
        brut = controler(version, dossier)
        sig_chemin = signing.signer(dossier / MANIFESTE)
        ok, motif = signing.verifier_octets(brut, sig_chemin.read_text("ascii"))
        if not ok:
            sys.exit("Signature produite invalide (%s) : rien n'a ete envoye." % motif)
        gh("release", "upload", tag, str(sig_chemin), "--clobber")
        print("Signature ajoutee a la release %s." % tag)
        if brouillon:
            gh("release", "edit", tag, "--draft=false", "--latest")
            print("Release %s publiee comme derniere release." % tag)
        relire_cote_modules(brut)


def cmd_publier_sans_signature(version):
    with tempfile.TemporaryDirectory() as d:
        dossier = pathlib.Path(d)
        tag, brouillon, assets = preparer(version, dossier)
        if SIG in assets:
            sys.exit("La release %s a deja une signature : essai sans objet." % tag)
        if not brouillon:
            sys.exit("La release %s est deja publiee." % tag)
        brut = controler(version, dossier)
        print("ESSAI : publication de %s SANS signature. Les modules doivent la refuser "
              "(signature-absente). Signer ensuite : release_firmware.py signer %s" % (tag, version))
        gh("release", "edit", tag, "--draft=false", "--latest")
        print("Release %s publiee sans signature." % tag)
        relire_cote_modules(brut)


def main():
    commandes = {
        "verifier": cmd_verifier,
        "signer": cmd_signer,
        "publier-sans-signature": cmd_publier_sans_signature,
    }
    if len(sys.argv) != 3 or sys.argv[1] not in commandes:
        sys.exit(__doc__)
    commandes[sys.argv[1]](sys.argv[2].strip())


if __name__ == "__main__":
    main()
