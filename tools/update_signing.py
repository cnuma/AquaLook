#!/usr/bin/env python3
"""Signature des mises a jour AquaLook (D018) : ECDSA P-256 / SHA-256.

La confiance ne repose plus sur l'hebergeur : le module n'installe un
manifeste que s'il est signe par une cle dont il connait la cle PUBLIQUE
(integree au firmware). Changer d'hebergeur ne change donc rien a la
securite, et un acces FTP vole ne suffit plus a publier.

  python tools/update_signing.py creer            # cree LA cle privee (une fois)
  python tools/update_signing.py signer FICHIER   # ecrit FICHIER.sig
  python tools/update_signing.py verifier FICHIER [FICHIER.sig]
  python tools/update_signing.py entete           # regenere src/UpdateSignatureKeys.h

Cle privee : JAMAIS dans le depot, ni sur l'hebergeur, ni dans un chat.
Emplacement : variable AQUALOOK_SIGNING_KEY (environnement ou .env a la
racine du depot), sinon %USERPROFILE%\\.aqualook\\signature\\cle-privee.pem
(hors OneDrive). La sauvegarder sur un support hors ligne : perdue, il faut
diffuser un nouveau firmware avant de pouvoir publier ; volee, il faut la
revoquer (retirer sa cle publique) par un nouveau firmware.

Cles publiques : tools/cles_publiques/<id>.pem (suivies par Git), d'ou est
genere src/UpdateSignatureKeys.h. Plusieurs cles peuvent etre acceptees en
meme temps : c'est ce qui permet d'en changer sans couper les modules.

Format du fichier .sig (texte, une ligne, sur des octets EXACTS du fichier) :
    aqualook-sig-v1 <id> <signature DER en base64>
<id> = 16 premiers caracteres hexadecimaux du SHA-256 de la cle publique
(DER SubjectPublicKeyInfo). Texte et non binaire : un client FTP en mode
ASCII ne l'abime pas.
"""
import base64
import hashlib
import os
import pathlib
import sys

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

RACINE = pathlib.Path(__file__).resolve().parents[1]
PUBLIQUES = RACINE / "tools" / "cles_publiques"
ENTETE = RACINE / "src" / "UpdateSignatureKeys.h"
FORMAT = "aqualook-sig-v1"


def lire_env(nom):
    val = os.environ.get(nom)
    if val:
        return val
    env = RACINE / ".env"
    if env.exists():
        for ligne in env.read_text(encoding="utf-8").splitlines():
            if ligne.startswith(nom + "="):
                return ligne.split("=", 1)[1].strip()
    return None


def chemin_cle():
    explicite = lire_env("AQUALOOK_SIGNING_KEY")
    if explicite:
        return pathlib.Path(explicite)
    return pathlib.Path.home() / ".aqualook" / "signature" / "cle-privee.pem"


def der_publique(cle_publique):
    return cle_publique.public_bytes(serialization.Encoding.DER,
                                     serialization.PublicFormat.SubjectPublicKeyInfo)


def identifiant(cle_publique):
    return hashlib.sha256(der_publique(cle_publique)).hexdigest()[:16]


def cles_publiques():
    cles = {}
    for p in sorted(PUBLIQUES.glob("*.pem")):
        cle = serialization.load_pem_public_key(p.read_bytes())
        if not isinstance(cle, ec.EllipticCurvePublicKey) or cle.curve.name != "secp256r1":
            sys.exit("%s : cle publique non P-256" % p.name)
        cles[identifiant(cle)] = cle
    return cles


def charger_privee():
    p = chemin_cle()
    if not p.exists():
        sys.exit("Cle privee absente (%s) : 'creer' d'abord, ou AQUALOOK_SIGNING_KEY." % p)
    cle = serialization.load_pem_private_key(p.read_bytes(), password=None)
    if not isinstance(cle, ec.EllipticCurvePrivateKey) or cle.curve.name != "secp256r1":
        sys.exit("%s : cle privee non P-256" % p)
    return cle


def creer():
    p = chemin_cle()
    if p.exists():
        sys.exit("Une cle privee existe deja (%s) : refus de l'ecraser." % p)
    try:
        p.resolve().relative_to(RACINE)
        sys.exit("Refus : la cle privee ne doit pas etre dans le depot (%s)." % p)
    except ValueError:
        pass
    if "onedrive" in str(p.resolve()).lower():
        sys.exit("Refus : la cle privee ne doit pas etre synchronisee par OneDrive (%s)." % p)
    cle = ec.generate_private_key(ec.SECP256R1())
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(cle.private_bytes(serialization.Encoding.PEM,
                                    serialization.PrivateFormat.PKCS8,
                                    serialization.NoEncryption()))
    ident = identifiant(cle.public_key())
    PUBLIQUES.mkdir(parents=True, exist_ok=True)
    (PUBLIQUES / (ident + ".pem")).write_bytes(cle.public_key().public_bytes(
        serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo))
    print("Cle privee creee : %s" % p)
    print("Cle publique     : tools/cles_publiques/%s.pem" % ident)
    print("A FAIRE : copier la cle privee sur un support hors ligne (cle USB rangee).")
    entete()


def entete():
    cles = cles_publiques()
    if not cles:
        sys.exit("Aucune cle publique dans tools/cles_publiques.")
    lignes = [
        "#pragma once",
        "// GENERE par tools/update_signing.py entete -- ne pas modifier a la main.",
        "// Cles PUBLIQUES acceptees pour les manifestes de mise a jour (D018).",
        "// Retirer une cle ici (et son .pem) la revoque au prochain firmware.",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "struct UpdateSigningKey {",
        "    const char*    id;",
        "    const uint8_t* der;",
        "    size_t         len;",
        "};",
        "",
    ]
    for ident, cle in cles.items():
        der = der_publique(cle)
        lignes.append("static const uint8_t UPDATE_KEY_%s[] = {" % ident.upper())
        for i in range(0, len(der), 12):
            lignes.append("    " + ", ".join("0x%02x" % b for b in der[i:i + 12]) + ",")
        lignes.append("};")
    lignes.append("")
    lignes.append("static const UpdateSigningKey UPDATE_SIGNING_KEYS[] = {")
    for ident in cles:
        lignes.append('    {"%s", UPDATE_KEY_%s, sizeof(UPDATE_KEY_%s)},' % (ident, ident.upper(), ident.upper()))
    lignes.append("};")
    lignes.append("static constexpr size_t UPDATE_SIGNING_KEY_COUNT =")
    lignes.append("    sizeof(UPDATE_SIGNING_KEYS) / sizeof(UPDATE_SIGNING_KEYS[0]);")
    ENTETE.write_text("\n".join(lignes) + "\n", encoding="utf-8", newline="\n")
    print("%s regenere (%d cle(s) : %s)" % (ENTETE.relative_to(RACINE), len(cles), ", ".join(cles)))


def signer(fichier):
    cle = charger_privee()
    ident = identifiant(cle.public_key())
    if ident not in cles_publiques():
        sys.exit("La cle privee (%s) n'a pas sa cle publique dans tools/cles_publiques : "
                 "aucun module ne l'accepterait." % ident)
    donnees = pathlib.Path(fichier).read_bytes()
    sig = cle.sign(donnees, ec.ECDSA(hashes.SHA256()))
    sortie = pathlib.Path(str(fichier) + ".sig")
    sortie.write_bytes(("%s %s %s\n" % (FORMAT, ident, base64.b64encode(sig).decode())).encode("ascii"))
    print("Signe : %s (cle %s)" % (sortie.name, ident))
    return sortie


def verifier_octets(donnees, texte_sig):
    """Meme regle que le module (UpdateSignature::verify). Rend (ok, motif)."""
    morceaux = texte_sig.strip().split()
    if len(morceaux) != 3 or morceaux[0] != FORMAT:
        return False, "signature-illisible"
    cle = cles_publiques().get(morceaux[1])
    if cle is None:
        return False, "signature-cle-inconnue"
    try:
        cle.verify(base64.b64decode(morceaux[2], validate=True), donnees, ec.ECDSA(hashes.SHA256()))
    except (InvalidSignature, ValueError):
        return False, "signature-invalide"
    return True, "signature-ok cle=" + morceaux[1]


def verifier(fichier, sig=None):
    donnees = pathlib.Path(fichier).read_bytes()
    texte = pathlib.Path(sig or (str(fichier) + ".sig")).read_text(encoding="ascii", errors="replace")
    ok, motif = verifier_octets(donnees, texte)
    print(motif)
    return ok


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("creer", "signer", "verifier", "entete"):
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd == "creer":
        creer()
    elif cmd == "entete":
        entete()
    elif cmd == "signer" and len(sys.argv) == 3:
        signer(sys.argv[2])
    elif cmd == "verifier" and len(sys.argv) in (3, 4):
        sys.exit(0 if verifier(*sys.argv[2:]) else 1)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
