#!/usr/bin/env python3
"""Generate the AquaLook Web-assets manifest from the data/ directory.

Second, independent manifest alongside tools/generate_ota_manifest.py
(firmware). Deliberately mirrors its structure and conventions (schema tag,
HTTPS-only GitHub Release URLs, SHA-256 per file) so the two channels stay
consistent, even though they are published, checked and triggered
separately (see ROADMAP.md, "Mise a jour distante des ressources Web").

Files are listed individually - no zip/tar archive - because the ESP32
side has no embedded decompressor and the file set is small. Each file in
data/ is uploaded as its own GitHub Release asset (same download-URL
pattern as the firmware binaries) and referenced here by name, size and
SHA-256, so the module can compare against its own assets-version.json and
download+verify only what actually changed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
import re
import sys

SCHEMA = "aqualook-web-manifest-v1"
VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$")
MAX_MANIFEST_SIZE = 8192

# Declarations de fonctions au premier niveau : ancrees colonne 0, donc les
# fonctions imbriquees (indentees) sont ignorees - seules celles qui vivent
# dans l'espace global peuvent s'ecraser entre elles.
JS_FUNCTION = re.compile(
    r"^(?:async\s+)?function\s+([A-Za-z_$][A-Za-z0-9_$]*)\s*\(",
    re.MULTILINE,
)
JS_ASSIGNED = re.compile(
    r"^(?:const|let|var)\s+([A-Za-z_$][A-Za-z0-9_$]*)\s*=\s*"
    r"(?:async\s+)?(?:function\b|\([^()]*\)\s*=>)",
    re.MULTILINE,
)
HTML_SCRIPT_SRC = re.compile(
    r"""<script[^>]*\ssrc=["']([^"']+)["']""", re.IGNORECASE
)
HTML_SCRIPT_INLINE = re.compile(
    r"<script(?![^>]*\ssrc=)[^>]*>(.*?)</script>", re.IGNORECASE | re.DOTALL
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def file_entry(path: Path, name: str, url: str) -> dict[str, object]:
    size = path.stat().st_size
    if size <= 0:
        raise ValueError(f"Web asset is empty: {path}")
    # HTTPS obligatoire, quel que soit l'hebergeur. La contrainte etait
    # auparavant "https://github.com/", ce qui melait deux exigences : le
    # chiffrement, qui est le vrai invariant de securite, et l'identite de
    # l'hebergeur, qui n'en est pas un. Publier ailleurs est desormais un
    # besoin reel - un module installe sur site doit pouvoir changer de
    # source sans etre reflashe.
    if not url.startswith("https://"):
        raise ValueError(f"Web asset URL must be HTTPS: {url}")
    return {
        "name": name,
        "url": url,
        "size": size,
        "sha256": sha256(path),
    }


def top_level_names(code: str) -> list[str]:
    return JS_FUNCTION.findall(code) + JS_ASSIGNED.findall(code)


def check_no_shadowed_functions(files: list[Path]) -> None:
    """Refuse de publier si une fonction est declaree deux fois.

    Incident du 31 aout 2026 : le regroupement des reglages en rubriques
    avait laisse en place l'ancien niveau 2 du menu Parametres.
    openCfgPage, backToCfgMenu et toggleSection existaient en double dans
    app.js, et JavaScript retient la DERNIERE declaration - la perimee.
    Le menu s'ouvrait, aucune rubrique ne s'ouvrait ensuite.

    Rien ne signale ce defaut : pas d'erreur, pas d'exception, juste un
    bouton inerte. Il ne se voit qu'a l'usage, donc apres publication.
    D'ou ce controle ici, au seul passage obligatoire avant diffusion.
    """
    by_name = {p.name: p for p in files}

    def read(path: Path) -> str:
        return path.read_text(encoding="utf-8", errors="replace")

    # Une page HTML forme une portee : ses scripts en ligne et les fichiers
    # qu'elle charge partagent le meme espace global, donc s'y ecrasent.
    # Deux pages distinctes peuvent en revanche reutiliser un nom sans
    # conflit - les regrouper produirait de fausses alertes.
    scopes: dict[str, list[tuple[str, str]]] = {}
    for path in files:
        if path.suffix.lower() not in (".html", ".htm"):
            continue
        text = read(path)
        units = [
            (path.name + " (script en ligne)", block)
            for block in HTML_SCRIPT_INLINE.findall(text)
        ]
        for src in HTML_SCRIPT_SRC.findall(text):
            dep = by_name.get(src.split("?", 1)[0].rsplit("/", 1)[-1])
            if dep is not None:
                units.append((dep.name, read(dep)))
        if units:
            scopes[path.name] = units

    # Un .js que plus aucune page ne charge est verifie seul : un doublon
    # interne y est deja un defaut, et le fichier reste publie.
    loaded = {origin for units in scopes.values() for origin, _ in units}
    for path in files:
        if path.suffix.lower() == ".js" and path.name not in loaded:
            scopes[path.name] = [(path.name, read(path))]

    problems: list[str] = []
    for scope, units in sorted(scopes.items()):
        seen: dict[str, list[str]] = {}
        for origin, code in units:
            for name in top_level_names(code):
                seen.setdefault(name, []).append(origin)
        for name, origins in sorted(seen.items()):
            if len(origins) < 2:
                continue
            where = ", ".join(
                f"{origin} x{count}" if count > 1 else origin
                for origin, count in Counter(origins).items()
            )
            problems.append(
                f"{scope} : {name}() declaree {len(origins)} fois ({where})"
            )

    if problems:
        raise ValueError(
            "Fonction(s) declaree(s) plusieurs fois dans une meme portee - "
            "la derniere ecrase les precedentes, sans erreur ni avertissement :"
            + "".join("\n  - " + p for p in problems)
        )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--repository", default="cnuma/AquaLook")
    parser.add_argument("--channel", default="stable")
    # Base d'URL des fichiers publies. Par defaut la release GitHub, donc le
    # flux de publication existant est inchange.
    #
    # La rendre reglable permet de publier ailleurs - un hebergement propre,
    # par exemple - ce que le firmware sait desormais consommer depuis que
    # l'URL du manifeste y est configurable (ConfigManager::setWebAssetsUrl).
    # Les deux vont de pair : une source configurable cote module ne sert a
    # rien si le generateur ne sait produire que des URL GitHub.
    parser.add_argument(
        "--base-url",
        default=None,
        help="Base URL des fichiers (defaut : la release GitHub). "
             "HTTPS obligatoire, comme cote module.",
    )
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    version = args.version.strip()
    if not VERSION_PATTERN.fullmatch(version):
        raise ValueError(f"Invalid version: {version!r}")
    expected_tag = f"v{version}"
    if args.tag != expected_tag:
        raise ValueError(f"Tag {args.tag!r} must equal {expected_tag!r}")

    if not args.data_dir.is_dir():
        raise FileNotFoundError(f"Data directory not found: {args.data_dir}")

    # Meme perimetre que tools/sync-sd-assets.ps1 : tout data/, a plat, pas
    # de sous-dossiers aujourd'hui (SdStaticHandler ne sert que /www/<nom>).
    # Un futur sous-dossier casserait cette hypothese - a revoir alors.
    source_files = sorted(
        p for p in args.data_dir.iterdir() if p.is_file()
    )
    if not source_files:
        raise ValueError(f"No files to publish in {args.data_dir}")

    names = [p.name for p in source_files]
    if len(names) != len(set(names)):
        raise ValueError("Duplicate file names in data/ - release assets must be unique")

    # Avant de calculer la moindre empreinte : un doublon de declaration
    # produirait un manifeste parfaitement valide pour un code casse.
    check_no_shadowed_functions(source_files)

    if args.base_url:
        base = args.base_url.rstrip("/")
        # Meme exigence que le module : le manifeste porte les SHA-256 qui
        # authentifient les fichiers. Servi ou reference en clair, il peut
        # etre remplace par un autre, avec ses propres hashes - la
        # verification validerait alors l'attaque au lieu de l'empecher.
        if not base.startswith("https://"):
            raise ValueError(f"--base-url doit etre en https:// (recu {base!r})")
    else:
        base = f"https://github.com/{args.repository}/releases/download/{args.tag}"

    manifest = {
        "schema": SCHEMA,
        "release": {
            "version": version,
            "channel": args.channel,
            "publishedAt": datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
            "notesUrl": f"https://github.com/{args.repository}/releases/tag/{args.tag}",
        },
        "files": [
            file_entry(p, p.name, f"{base}/{p.name}")
            for p in source_files
        ],
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    encoded = json.dumps(manifest, indent=2, ensure_ascii=True) + "\n"
    if len(encoded.encode("utf-8")) > MAX_MANIFEST_SIZE:
        raise ValueError(
            f"Manifest exceeds {MAX_MANIFEST_SIZE} bytes budget "
            f"({len(encoded.encode('utf-8'))} bytes) - split or trim data/"
        )
    args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
