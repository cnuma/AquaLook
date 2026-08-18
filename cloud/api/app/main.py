"""API HTTP AquaLook — transport a jeton porteur, sans courtier permanent.

Trajectoire retenue depuis le 18 aout 2026 (voir docs/architecture/
SYSTEM_ARCHITECTURE.md Sec.5.0 et docs/architecture/CLOUD_ENVIRONMENT_EVALUATION.md
Sec.7). Le module reste toujours celui qui ouvre la connexion, en connexions
courtes -- jamais de connexion entrante acceptee. La logique metier
d'arrosage n'est PAS ici : l'ESP32 reste l'autorite locale, ce service
transporte, historise et met en attente (invariant de SYSTEM_ARCHITECTURE.md).
"""

from __future__ import annotations

import json
import os
import re
from typing import Any, Optional

from dotenv import load_dotenv

load_dotenv()  # avant l'import de `db` : DATABASE_PATH y est lu au chargement du module

from fastapi import Depends, FastAPI, Header, HTTPException  # noqa: E402

from . import db  # noqa: E402

PROTO_VERSION = "v1"
MAX_PAYLOAD_BYTES = 64 * 1024
# "config" : instantane de la configuration effective du module (creneaux et
# reglages systeme). Miroir en lecture seule -- le module reste l'autorite,
# voir SYSTEM_ARCHITECTURE.md Sec.9 invariants #1, #2, #3.
VALID_MSG_TYPES = {"status", "state", "event", "diag", "config"}

# Meme contrainte que la piste MQTT : un identifiant de module reste borne et
# sans surprise, il sert de cle primaire.
MODULE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$")

ADMIN_TOKEN = os.environ.get("ADMIN_TOKEN", "")


def _bearer_token(authorization: Optional[str]) -> str:
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(status_code=401, detail="jeton porteur manquant")
    return authorization.removeprefix("Bearer ").strip()


def require_module(authorization: Optional[str] = Header(None)) -> str:
    """Resout le jeton porteur en identifiant de module. Refus sur, jamais
    silencieux : SYSTEM_ARCHITECTURE.md Sec.7 exige un refus explicite en cas
    de message incomplet, invalide ou incompatible."""
    token = _bearer_token(authorization)
    module_id = db.module_id_for_token(token)
    if module_id is None:
        raise HTTPException(status_code=401, detail="jeton porteur invalide")
    return module_id


def require_admin(authorization: Optional[str] = Header(None)) -> None:
    if not ADMIN_TOKEN:
        raise HTTPException(status_code=503, detail="ADMIN_TOKEN non configure cote serveur")
    token = _bearer_token(authorization)
    if token != ADMIN_TOKEN:
        raise HTTPException(status_code=401, detail="jeton admin invalide")


def _check_payload_size(payload: dict) -> None:
    if len(json.dumps(payload)) > MAX_PAYLOAD_BYTES:
        raise HTTPException(status_code=413, detail="charge utile trop volumineuse")


app = FastAPI(title="AquaLook API", version=PROTO_VERSION)


@app.on_event("startup")
def _startup() -> None:
    db.init_db()


@app.get("/health")
def health() -> dict:
    return {"ok": True}


# ── Routes module (jeton porteur par module) ───────────────────────────────

@app.post("/v1/report")
def report(body: dict[str, Any], module_id: str = Depends(require_module)) -> dict:
    msg_type = body.get("type")
    if msg_type not in VALID_MSG_TYPES:
        raise HTTPException(status_code=400, detail=f"type invalide, attendu parmi {sorted(VALID_MSG_TYPES)}")
    payload = body.get("payload")
    if not isinstance(payload, dict):
        raise HTTPException(status_code=400, detail="payload doit etre un objet")
    _check_payload_size(payload)

    correlation_id = body.get("correlationId")
    db.insert_message(module_id, PROTO_VERSION, msg_type, correlation_id, payload)
    firmware = payload.get("firmware") if msg_type in {"status", "state"} else None
    db.touch_module(module_id, firmware=firmware)
    return {"ok": True}


@app.get("/v1/pending-command")
def pending_command(module_id: str = Depends(require_module)) -> dict:
    result = db.next_pending_command(module_id)
    if result is None:
        return {"correlationId": None, "command": None}
    return result


@app.post("/v1/command/ack")
def ack_command(body: dict[str, Any], module_id: str = Depends(require_module)) -> dict:
    correlation_id = body.get("correlationId")
    state = body.get("state")
    if not correlation_id or state not in {"accepted", "refused", "failed"}:
        raise HTTPException(status_code=400, detail="correlationId et state (accepted|refused|failed) requis")
    result = body.get("result")
    if result is not None and not isinstance(result, dict):
        raise HTTPException(status_code=400, detail="result doit etre un objet si present")

    final_state = db.settle_command(module_id, correlation_id, state, result)
    if final_state is None:
        raise HTTPException(status_code=404, detail="correlationId inconnu pour ce module")
    return {"ok": True, "state": final_state}


# ── Routes admin (jeton admin) ──────────────────────────────────────────────

@app.get("/admin/modules")
def admin_modules(_: None = Depends(require_admin)) -> list[dict]:
    return db.list_modules()


@app.post("/admin/module-token")
def admin_module_token(body: dict[str, Any], _: None = Depends(require_admin)) -> dict:
    module_id = body.get("moduleId", "")
    if not MODULE_ID_RE.match(module_id):
        raise HTTPException(status_code=400, detail="moduleId invalide")
    token = body.get("token") or __import__("secrets").token_urlsafe(32)
    label = body.get("label")
    db.upsert_module_token(module_id, token, label)
    # Montre le jeton une seule fois, en clair, dans cette reponse -- a noter
    # cote administrateur, jamais relisible depuis le serveur ensuite.
    return {"moduleId": module_id, "token": token}


@app.post("/admin/command")
def admin_command(body: dict[str, Any], _: None = Depends(require_admin)) -> dict:
    module_id = body.get("moduleId", "")
    if not MODULE_ID_RE.match(module_id):
        raise HTTPException(status_code=400, detail="moduleId invalide")
    command = body.get("command")
    if not isinstance(command, dict):
        raise HTTPException(status_code=400, detail="command doit etre un objet")
    _check_payload_size(command)

    correlation_id = db.create_command(module_id, command, issued_by=body.get("issuedBy"))
    return {"correlationId": correlation_id}
