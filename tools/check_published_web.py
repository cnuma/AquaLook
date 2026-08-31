#!/usr/bin/env python3
"""Compare le manifeste REELLEMENT en ligne aux fichiers REELLEMENT servis.

Pourquoi cet outil existe. Deux publications de suite ont echoue le
1er septembre 2026, pour deux variantes de la meme erreur :

  - un fichier modifie dans un paquet deja genere, sans regenerer le
    manifeste : le module a refuse un fichier de 7941 octets annonce a 7686 ;
  - un manifeste regenere pour une NOUVELLE version, ecrasant le precedent au
    meme chemin, puis publie seul : les onze fichiers sont partis en 404
    parce que le dossier de la nouvelle version n'etait pas encore en ligne.

Dans les deux cas le module a eu raison de refuser, et dans les deux cas le
defaut etait cote publication. Verifier localement ne suffit pas : ce qui
compte est ce que le serveur sert, pas ce que le poste contient.

    python tools/check_published_web.py https://aqualook.alwaysdata.net/aqualook-web-manifest.json

Sortie 0 si tout concorde, 1 sinon.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import urllib.error
import urllib.request

TIMEOUT = 30


def fetch(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=TIMEOUT) as response:
        return response.read()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest_url")
    args = parser.parse_args()

    try:
        raw = fetch(args.manifest_url)
    except Exception as exc:  # noqa: BLE001 - message lisible plutot que trace
        print(f"manifeste injoignable : {exc}")
        return 1

    try:
        manifest = json.loads(raw)
    except json.JSONDecodeError as exc:
        print(f"manifeste illisible : {exc}")
        return 1

    version = manifest.get("release", {}).get("version", "?")
    files = manifest.get("files", [])
    print(f"manifeste en ligne : version {version}, {len(files)} fichier(s)")
    print(f"  {len(raw)} octets, lu depuis {args.manifest_url}")
    print()

    faults = 0
    for entry in files:
        name = entry.get("name", "?")
        try:
            body = fetch(entry["url"])
        except urllib.error.HTTPError as exc:
            print(f"  ABSENT    {name:<18} HTTP {exc.code}")
            faults += 1
            continue
        except Exception as exc:  # noqa: BLE001
            print(f"  ECHEC     {name:<18} {exc}")
            faults += 1
            continue

        digest = hashlib.sha256(body).hexdigest()
        if len(body) != entry.get("size"):
            print(f"  TAILLE    {name:<18} annonce {entry.get('size')} servi {len(body)}")
            faults += 1
        elif digest != entry.get("sha256"):
            print(f"  EMPREINTE {name:<18} le contenu differe de ce qui est annonce")
            faults += 1
        else:
            print(f"  conforme  {name:<18} {len(body)} octets")

    print()
    if faults:
        print(f"{faults} defaut(s) : le module refusera cette publication, et il aura raison.")
        return 1
    print(f"Publication coherente : {len(files)}/{len(files)}. Le module peut se mettre a jour.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
