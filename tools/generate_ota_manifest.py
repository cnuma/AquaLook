#!/usr/bin/env python3
"""Generate the AquaLook OTA manifest from compiled firmware binaries."""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
import re
import sys

SCHEMA = "aqualook-ota-manifest-v1"
# Release S3 seule (P1.4) : cible, carte et environnement doivent etre
# exactement ceux que tools/version_build.py integre au binaire de
# production, sinon le module refuse (target-missing, board-mismatch,
# environment-mismatch).
TARGET = "s3"
BOARD = "jc4827w543c_i"
ENVIRONMENT = "ProgrammeArrosage_s3"
# Doit suivre la taille des slots OTA dans aqualook_partitions.csv : ramenee de
# 2 031 616 (0x1F0000) a 1 966 080 (0x1E0000) le 16 aout 2026 pour agrandir la
# partition NVS. Publier un firmware plus gros que le slot le rendrait
# ininstallable sur le module.
MAX_FIRMWARE_SIZE = 1_966_080
VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def firmware_entry(path: Path, environment: str, url: str) -> dict[str, object]:
    if not path.is_file():
        raise FileNotFoundError(f"Firmware not found: {path}")
    size = path.stat().st_size
    if size <= 0:
        raise ValueError(f"Firmware is empty: {path}")
    if size > MAX_FIRMWARE_SIZE:
        raise ValueError(
            f"Firmware exceeds OTA partition: {path} ({size} > {MAX_FIRMWARE_SIZE})"
        )
    if not url.startswith("https://github.com/"):
        raise ValueError(f"Firmware URL must be HTTPS GitHub: {url}")
    return {
        "board": BOARD,
        "environment": environment,
        "firmwareUrl": url,
        "size": size,
        "sha256": sha256(path),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--repository", default="cnuma/AquaLook")
    parser.add_argument("--channel", default="stable")
    parser.add_argument("--s3", type=Path, required=True)
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

    base = f"https://github.com/{args.repository}/releases/download/{args.tag}"
    s3_name = f"AquaLook-s3-{version}.bin"

    manifest = {
        "schema": SCHEMA,
        "release": {
            "version": version,
            "channel": args.channel,
            "publishedAt": datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
            "notesUrl": f"https://github.com/{args.repository}/releases/tag/{args.tag}",
        },
        "targets": {
            TARGET: firmware_entry(
                args.s3,
                ENVIRONMENT,
                f"{base}/{s3_name}",
            ),
        },
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    encoded = json.dumps(manifest, indent=2, ensure_ascii=True) + "\n"
    if len(encoded.encode("utf-8")) > 8192:
        raise ValueError("Manifest exceeds firmware limit of 8192 bytes")
    args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
