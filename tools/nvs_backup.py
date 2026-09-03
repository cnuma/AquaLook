"""Sauvegarde et restauration de la partition NVS d'un module AquaLook.

Pourquoi ce fichier existe : le 3 septembre 2026, une migration de schema
ajoutee sans mettre a jour la garde de longueur a fait rejeter le bloc de
configuration, puis ecraser par des valeurs d'usine. WiFi et planning perdus
sur le module d'essai. La cause est corrigee, mais la lecon est ailleurs : une
migration de configuration ne doit jamais etre tentee sans filet, surtout sur
un module en service qui arrose un vrai jardin.

La NVS ne contient pas que le planning. On y trouve aussi les identifiants
WiFi, le sujet et le jeton ntfy, et le jeton d'appairage cloud -- ce dernier
n'etant PAS reconstituable, le serveur n'en gardant qu'une empreinte. Perdre
cette partition, c'est reappairer le module a la main.

Marche a suivre avant de flasher un module dont la configuration compte :

    python tools/nvs_backup.py save COM4 avant-maj.bin
    ... flash, verification ...
    python tools/nvs_backup.py restore COM4 avant-maj.bin   # si besoin

La restauration a ete verifiee de bout en bout le 3 septembre 2026 : SSID,
synchronisation cloud et notifications sont revenus intacts.

L'adresse et la taille sont lues dans aqualook_partitions.csv, pour qu'un
changement de table de partitions ne rende pas ce fichier silencieusement faux.
"""
import argparse
import pathlib
import subprocess
import sys

RACINE = pathlib.Path(__file__).resolve().parents[1]
TABLE = RACINE / "aqualook_partitions.csv"


def region_nvs():
    """Rend (offset, taille) de la partition nvs, en entiers."""
    if not TABLE.exists():
        raise SystemExit("Table de partitions introuvable : %s" % TABLE)
    for ligne in TABLE.read_text(encoding="utf-8").splitlines():
        ligne = ligne.strip()
        if not ligne or ligne.startswith("#"):
            continue
        champs = [c.strip() for c in ligne.split(",")]
        if len(champs) >= 5 and champs[0] == "nvs":
            return int(champs[3], 0), int(champs[4], 0)
    raise SystemExit("Aucune partition 'nvs' dans %s" % TABLE)


def esptool(args):
    commande = [sys.executable, "-m", "esptool"] + args
    print("  " + " ".join(commande[2:]))
    r = subprocess.run(commande, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit("esptool a echoue (code %d)" % r.returncode)
    return r.stdout


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("action", choices=["save", "restore"])
    ap.add_argument("port", help="port serie du module, par exemple COM4")
    ap.add_argument("fichier", help="fichier de sauvegarde")
    args = ap.parse_args()

    offset, taille = region_nvs()
    chemin = pathlib.Path(args.fichier)
    print("Partition nvs : offset 0x%X, taille 0x%X (%d octets)"
          % (offset, taille, taille))

    if args.action == "save":
        esptool(["--port", args.port, "--baud", "460800",
                 "read_flash", hex(offset), hex(taille), str(chemin)])
        donnees = chemin.read_bytes()
        utiles = sum(1 for b in donnees if b != 0xFF)
        print("Sauvegarde : %s (%d octets, dont %d ecrits)"
              % (chemin, len(donnees), utiles))
        if utiles == 0:
            print("ATTENTION : partition entierement vierge. Le module "
                  "a-t-il bien une configuration ?")
        return

    donnees = chemin.read_bytes()
    if len(donnees) != taille:
        raise SystemExit("Taille inattendue : %d octets pour une partition de %d. "
                         "Ce fichier ne vient pas de cette table de partitions."
                         % (len(donnees), taille))
    # L'effacement prealable n'est pas optionnel : ecrire sur une NVS non
    # effacee melange les anciennes et les nouvelles pages, et le resultat
    # n'est pas lisible.
    esptool(["--port", args.port, "erase_region", hex(offset), hex(taille)])
    esptool(["--port", args.port, "--baud", "921600",
             "write_flash", hex(offset), str(chemin)])
    print("Restauration terminee. Verifiez la ligne \"Config: charge depuis "
          "NVS\" au demarrage suivant.")


if __name__ == "__main__":
    main()
