#!/usr/bin/env python3
"""Prepare un module AquaLook neuf, sans se soucier des numeros de version (P2.18).

  python tools/preparer_module.py flasher --port COM5 --effacer
      telecharge la DERNIERE release publiee et signee sur GitHub, verifie la
      signature et chaque image, efface la flash (module neuf), ecrit
      chargeur de demarrage, table de partitions, otadata, programme de
      production et LittleFS, puis relit l'identite sur la sortie serie.

  python tools/preparer_module.py flasher --port COM5 --sauver-nvs avant.bin
      meme chose sur un module deja regle : la NVS (Wi-Fi, PIN, rattachement)
      est conservee, et sauvegardee d'abord (tools/nvs_backup.py).

Options de flasher :
  --version X.Y.Z   une release precise au lieu de la derniere ;
  --local           images de .pio/build/ProgrammeArrosage_s3 (apres
                    `pio run -e ProgrammeArrosage_s3` et `-t buildfs`), sans
                    signature : pour essayer un build local avant release.

  python tools/preparer_module.py fabrication DOSSIER --version X.Y.Z
      ecrit DOSSIER/aqualook-fabrication.json a partir des images deja
      copiees dans DOSSIER (utilise par .github/workflows/ota-release.yml).

Le manifeste de fabrication est signe avec la meme cle que le manifeste du
programme (D018), par `release_firmware.py signer`. Les modules ne le lisent
pas : il ne sert qu'a cet outil. Le programme flashe reste le binaire de
PRODUCTION (ProgrammeArrosage_s3) ; une fois sur le Wi-Fi, le module se tient
a jour par ses mises a jour habituelles, toujours declenchees par
l'utilisateur.
"""
import argparse
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

RACINE = pathlib.Path(__file__).resolve().parents[1]
DEPOT = "cnuma/AquaLook"
FAB = "aqualook-fabrication.json"
SCHEMA = "aqualook-fabrication-v1"
BUILD = RACINE / ".pio" / "build" / gen.ENVIRONMENT
BOOT_APP0 = (pathlib.Path.home() / ".platformio" / "packages" / "framework-arduinoespressif32"
             / "tools" / "partitions" / "boot_app0.bin")
# Valeurs de PlatformIO pour cette carte (qio_opi -> dio, 80 MHz, 4 Mo) :
# celles de l'en-tete du chargeur de demarrage compile.
FLASH_ARGS = ["--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "4MB"]
VITESSE = "921600"


def partitions():
    """Adresses lues dans aqualook_partitions.csv, la seule source."""
    t = {}
    for ligne in (RACINE / "aqualook_partitions.csv").read_text(encoding="utf-8").splitlines():
        champs = [c.strip() for c in ligne.split(",")]
        if len(champs) >= 5 and not ligne.lstrip().startswith("#"):
            t[champs[0]] = int(champs[3], 16)
    return t


def images_attendues(version):
    """(role, nom du fichier de release, adresse) dans l'ordre d'ecriture."""
    p = partitions()
    base = "AquaLook-s3-%s" % version
    return [
        ("bootloader", base + "-bootloader.bin", 0x0),      # ESP32-S3 : 0x0
        ("partitions", base + "-partitions.bin", 0x8000),
        ("otadata", base + "-boot_app0.bin", p["otadata"]),
        ("programme", base + ".bin", p["app0"]),
        ("littlefs", base + "-littlefs.bin", p["spiffs"]),
    ]


def sha(octets):
    return hashlib.sha256(octets).hexdigest()


# ---------------------------------------------------------------- fabrication

def cmd_fabrication(args):
    dossier = pathlib.Path(args.dossier)
    images = []
    for role, nom, adresse in images_attendues(args.version):
        octets = (dossier / nom).read_bytes()
        images.append({"role": role, "name": nom, "offset": "0x%x" % adresse,
                       "size": len(octets), "sha256": sha(octets)})
    man = {"schema": SCHEMA, "version": args.version, "board": gen.BOARD,
           "environment": gen.ENVIRONMENT, "images": images}
    (dossier / FAB).write_text(json.dumps(man, indent=2) + "\n", encoding="utf-8")
    print("%s : %d images" % (dossier / FAB, len(images)))


# ---------------------------------------------------------------- controles

def controler(man, fichiers, version=None):
    """fichiers : nom -> octets. Quitte au premier ecart."""
    erreurs = []

    def exiger(cond, msg):
        print(("  ok    " if cond else "  ECHEC ") + msg)
        if not cond:
            erreurs.append(msg)

    exiger(man.get("schema") == SCHEMA, "schema %s" % SCHEMA)
    v = man.get("version", "")
    exiger(bool(gen.VERSION_PATTERN.fullmatch(v)), "version %s" % v)
    if version:
        exiger(v == version, "version demandee %s" % version)
    exiger(man.get("board") == gen.BOARD, "carte %s" % gen.BOARD)
    exiger(man.get("environment") == gen.ENVIRONMENT, "environnement %s" % gen.ENVIRONMENT)
    attendu = [(r, n, "0x%x" % a) for r, n, a in images_attendues(v)]
    recu = [(i.get("role"), i.get("name"), i.get("offset")) for i in man.get("images", [])]
    exiger(recu == attendu, "images et adresses conformes a aqualook_partitions.csv")
    for i in man.get("images", []):
        o = fichiers.get(i.get("name"))
        exiger(o is not None and len(o) == i.get("size") and sha(o) == i.get("sha256"),
               "%-10s %s (%s octets)" % (i.get("role"), "empreinte SHA-256", i.get("size")))
    prog = fichiers.get("AquaLook-s3-%s.bin" % v, b"")
    exiger(len(prog) > 16 and prog[0] == 0xE9 and int.from_bytes(prog[12:14], "little") == 9,
           "programme : image ESP32-S3")
    exiger(len(prog) <= gen.MAX_FIRMWARE_SIZE, "programme <= emplacement OTA")
    exiger((v.encode() + b"\0") in prog, "programme en version %s" % v)
    exiger(b"ProgrammeArrosage_s3\0" in prog and b"ProgrammeArrosage_s3_banc\0" not in prog,
           "programme de production (pas le binaire de banc)")
    if erreurs:
        sys.exit("\n%d ecart(s) : RIEN n'a ete ecrit sur le module." % len(erreurs))


def lire_url(url):
    with urllib.request.urlopen(url, timeout=60) as r:
        return r.read()


def depuis_github(version, dossier):
    if version:
        base = "https://github.com/%s/releases/download/v%s/" % (DEPOT, version)
    else:
        base = "https://github.com/%s/releases/latest/download/" % DEPOT
    try:
        brut = lire_url(base + FAB)
        sig = lire_url(base + FAB + ".sig").decode("ascii", "replace")
    except urllib.error.HTTPError as e:
        sys.exit("Manifeste de fabrication absent de la release (HTTP %d) : release "
                 "anterieure a P2.18. Utiliser --version d'une release plus recente, "
                 "ou --local." % e.code)
    ok, motif = signing.verifier_octets(brut, sig)
    print("  %s signature : %s" % ("ok   " if ok else "ECHEC", motif))
    if not ok:
        sys.exit("Signature refusee : RIEN n'a ete ecrit sur le module.")
    man = json.loads(brut)
    print("Release v%s :" % man.get("version"))
    base = "https://github.com/%s/releases/download/v%s/" % (DEPOT, man.get("version"))
    fichiers = {}
    for i in man.get("images", []):
        fichiers[i["name"]] = lire_url(base + i["name"])
        (dossier / i["name"]).write_bytes(fichiers[i["name"]])
    controler(man, fichiers, version)
    return man


def depuis_local(dossier):
    version = (RACINE / "VERSION").read_text(encoding="utf-8").strip()
    sources = {"bootloader": BUILD / "bootloader.bin", "partitions": BUILD / "partitions.bin",
               "otadata": BOOT_APP0, "programme": BUILD / "firmware.bin",
               "littlefs": BUILD / "littlefs.bin"}
    manque = [str(p) for p in sources.values() if not p.exists()]
    if manque:
        sys.exit("Images absentes : %s\nCompiler d'abord : pio run -e %s ; pio run -e %s -t buildfs"
                 % (", ".join(manque), gen.ENVIRONMENT, gen.ENVIRONMENT))
    fichiers = {}
    for role, nom, _ in images_attendues(version):
        fichiers[nom] = sources[role].read_bytes()
        (dossier / nom).write_bytes(fichiers[nom])
    print("Build local %s (sans signature) :" % version)
    cmd_fabrication(argparse.Namespace(dossier=str(dossier), version=version))
    man = json.loads((dossier / FAB).read_text(encoding="utf-8"))
    controler(man, fichiers)
    return man


# ---------------------------------------------------------------- flash

def esptool():
    pio = pathlib.Path.home() / ".platformio" / "packages" / "tool-esptoolpy" / "esptool.py"
    return [sys.executable, str(pio)] if pio.exists() else [sys.executable, "-m", "esptool"]


def executer(cmd):
    print("> " + " ".join(cmd[1:]))
    if subprocess.run(cmd).returncode != 0:
        sys.exit("esptool a echoue (port %s occupe par un moniteur ? cable de donnees ?)."
                 % cmd[cmd.index("--port") + 1] if "--port" in cmd else "esptool a echoue.")


def lire_banniere(port, attente=30):
    """Relit la banniere de demarrage. USB natif du S3 : le port disparait
    pendant le redemarrage, et sans DTR asserte rien ne sort."""
    try:
        import serial
    except ImportError:
        print("pyserial absent : relire l'identite avec pio device monitor.")
        return {}
    fin = time.time() + attente
    vu = {}
    while time.time() < fin:
        try:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = port, 115200, 0.5
            s.dtr, s.rts = True, False
            s.open()
        except Exception:
            time.sleep(0.5)
            continue
        with s:
            while time.time() < fin:
                ligne = s.readline().decode("utf-8", "replace").strip()
                for cle in ("Version", "Environnement", "Build", "Git SHA", "Routes debug",
                            "Identifiant"):
                    if ligne.startswith(cle + " ") and ":" in ligne:
                        vu[cle] = ligne.split(":", 1)[1].strip()
                if "Identifiant" in vu:
                    return vu
    return vu


def cmd_flasher(args):
    with tempfile.TemporaryDirectory() as d:
        dossier = pathlib.Path(d)
        man = depuis_local(dossier) if args.local else depuis_github(args.version, dossier)
        version = man["version"]
        if args.sauver_nvs:
            r = subprocess.run([sys.executable, str(RACINE / "tools" / "nvs_backup.py"),
                                "save", args.port, args.sauver_nvs])
            if r.returncode != 0:
                sys.exit("Sauvegarde NVS en echec : rien n'a ete ecrit.")
        base = esptool() + ["--chip", "esp32s3", "--port", args.port, "--baud", VITESSE]
        if args.effacer:
            print("Effacement complet de la flash (module neuf : NVS comprise).")
            executer(base + ["--after", "no_reset", "erase_flash"])
        ecriture = base + ["--before", "default_reset", "--after", "hard_reset",
                           "write_flash", "-z"] + FLASH_ARGS
        for i in man["images"]:
            ecriture += [i["offset"], str(dossier / i["name"])]
        executer(ecriture)

    print("Relecture de l'identite sur %s ..." % args.port)
    vu = lire_banniere(args.port)
    for cle, val in vu.items():
        print("  %-13s : %s" % (cle, val))
    if vu.get("Version") == version and vu.get("Environnement") == gen.ENVIRONMENT:
        print("Module pret : AquaLook %s, binaire de production." % version)
        print("Suite : carte SD, puis Wi-Fi (portail captif) -- voir le guide P2.18.")
        return 0
    if not vu:
        print("Banniere non lue (le PC a rouvert le port trop tard). Appuyer sur RESET puis :")
        print("  pio device monitor -p %s -b 115200 --dtr 1 --rts 0" % args.port)
        return 2
    print("ECART : version ou environnement inattendu (attendu %s, %s)."
          % (version, gen.ENVIRONMENT))
    return 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sous = ap.add_subparsers(dest="commande", required=True)
    f = sous.add_parser("flasher", help="flasher la derniere release sur un module")
    f.add_argument("--port", required=True, help="port serie du module, par exemple COM5")
    f.add_argument("--version", help="release precise X.Y.Z (defaut : la derniere)")
    f.add_argument("--local", action="store_true", help="images du build local, sans signature")
    g = f.add_mutually_exclusive_group()
    g.add_argument("--effacer", action="store_true", help="module neuf : effacer toute la flash")
    g.add_argument("--sauver-nvs", metavar="FICHIER", help="sauvegarder la NVS avant d'ecrire")
    fab = sous.add_parser("fabrication", help="ecrire le manifeste de fabrication")
    fab.add_argument("dossier")
    fab.add_argument("--version", required=True)
    args = ap.parse_args()
    if args.commande == "fabrication":
        cmd_fabrication(args)
        return 0
    return cmd_flasher(args)


if __name__ == "__main__":
    sys.exit(main())
