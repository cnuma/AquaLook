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
from datetime import datetime, timezone
from pathlib import Path
import re
import sys

SCHEMA = "aqualook-web-manifest-v1"
VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$")
MAX_MANIFEST_SIZE = 8192


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
    if not url.startswith("https://github.com/"):
        raise ValueError(f"Web asset URL must be HTTPS GitHub: {url}")
    return {
        "name": name,
        "url": url,
        "size": size,
        "sha256": sha256(path),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--repository", default="cnuma/AquaLook")
    parser.add_argument("--channel", default="stable")
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
