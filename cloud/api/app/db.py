"""Acces SQLite pour l'API AquaLook.

Meme schema logique que cloud/db/init/01-schema.sql (piste MQTT, differee) :
module / module_message / command sont independants du transport. Seule
difference : SQLite plutot que PostgreSQL/TimescaleDB, pour demarrer sans
service supplementaire. Voir cloud/api/README.md pour le raisonnement complet.

Une connexion par appel plutot qu'un pool : le volume attendu (un module,
puis quelques-uns) ne le justifie pas, et ca evite une classe entiere de bugs
de connexion partagee entre threads. Le mode WAL permet des lectures
concurrentes pendant une ecriture.
"""

from __future__ import annotations

import json
import os
import sqlite3
import uuid
from contextlib import contextmanager
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterator, Optional

DATABASE_PATH = Path(os.environ.get("DATABASE_PATH", "data/aqualook.db"))

SCHEMA = """
CREATE TABLE IF NOT EXISTS module (
    module_id  TEXT PRIMARY KEY,
    label      TEXT,
    firmware   TEXT,
    last_seen  TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS module_token (
    module_id  TEXT PRIMARY KEY REFERENCES module(module_id),
    token      TEXT NOT NULL UNIQUE,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS module_message (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    ts             TEXT NOT NULL,
    module_id      TEXT NOT NULL,
    proto_version  TEXT NOT NULL,
    msg_type       TEXT NOT NULL,      -- status | state | event | diag
    correlation_id TEXT,
    payload        TEXT NOT NULL       -- JSON serialise
);
CREATE INDEX IF NOT EXISTS module_message_module_ts_idx
    ON module_message (module_id, ts DESC);

CREATE TABLE IF NOT EXISTS command (
    correlation_id TEXT PRIMARY KEY,
    module_id      TEXT NOT NULL,
    issued_at      TEXT NOT NULL DEFAULT (datetime('now')),
    issued_by      TEXT,
    command        TEXT NOT NULL,      -- JSON serialise
    state          TEXT NOT NULL DEFAULT 'pending',  -- pending|accepted|refused|failed|expired
    settled_at     TEXT,
    result         TEXT
);
CREATE INDEX IF NOT EXISTS command_module_state_idx
    ON command (module_id, state, issued_at);
"""


def init_db() -> None:
    DATABASE_PATH.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(DATABASE_PATH) as conn:
        conn.execute("PRAGMA journal_mode=WAL")
        conn.executescript(SCHEMA)


@contextmanager
def get_conn() -> Iterator[sqlite3.Connection]:
    conn = sqlite3.connect(DATABASE_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys=ON")
    try:
        yield conn
        conn.commit()
    finally:
        conn.close()


def utcnow() -> str:
    return datetime.now(timezone.utc).isoformat()


# ── Modules et jetons ──────────────────────────────────────────────────────

def module_id_for_token(token: str) -> Optional[str]:
    with get_conn() as conn:
        row = conn.execute(
            "SELECT module_id FROM module_token WHERE token = ?", (token,)
        ).fetchone()
        return row["module_id"] if row else None


def upsert_module_token(module_id: str, token: str, label: Optional[str] = None) -> None:
    with get_conn() as conn:
        conn.execute(
            "INSERT INTO module (module_id, label, created_at) VALUES (?, ?, ?) "
            "ON CONFLICT(module_id) DO UPDATE SET label = COALESCE(excluded.label, module.label)",
            (module_id, label, utcnow()),
        )
        conn.execute(
            "INSERT INTO module_token (module_id, token, created_at) VALUES (?, ?, ?) "
            "ON CONFLICT(module_id) DO UPDATE SET token = excluded.token, created_at = excluded.created_at",
            (module_id, token, utcnow()),
        )


def touch_module(module_id: str, firmware: Optional[str] = None) -> None:
    with get_conn() as conn:
        conn.execute(
            "UPDATE module SET last_seen = ?, firmware = COALESCE(?, firmware) WHERE module_id = ?",
            (utcnow(), firmware, module_id),
        )


def list_modules() -> list[dict[str, Any]]:
    with get_conn() as conn:
        rows = conn.execute(
            "SELECT module_id, label, firmware, last_seen FROM module ORDER BY module_id"
        ).fetchall()
        return [dict(r) for r in rows]


# ── Messages (telemetrie, etats, evenements, diagnostics) ─────────────────

def insert_message(module_id: str, proto_version: str, msg_type: str,
                    correlation_id: Optional[str], payload: dict) -> None:
    with get_conn() as conn:
        conn.execute(
            "INSERT INTO module_message (ts, module_id, proto_version, msg_type, correlation_id, payload) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            (utcnow(), module_id, proto_version, msg_type, correlation_id, json.dumps(payload)),
        )


# ── Commandes ───────────────────────────────────────────────────────────────

def create_command(module_id: str, command: dict, issued_by: Optional[str] = None) -> str:
    correlation_id = str(uuid.uuid4())
    with get_conn() as conn:
        conn.execute(
            "INSERT INTO command (correlation_id, module_id, issued_at, issued_by, command, state) "
            "VALUES (?, ?, ?, ?, ?, 'pending')",
            (correlation_id, module_id, utcnow(), issued_by, json.dumps(command)),
        )
    return correlation_id


def next_pending_command(module_id: str) -> Optional[dict[str, Any]]:
    with get_conn() as conn:
        row = conn.execute(
            "SELECT correlation_id, command FROM command "
            "WHERE module_id = ? AND state = 'pending' "
            "ORDER BY issued_at ASC LIMIT 1",
            (module_id,),
        ).fetchone()
        if not row:
            return None
        return {"correlationId": row["correlation_id"], "command": json.loads(row["command"])}


def settle_command(module_id: str, correlation_id: str, state: str,
                    result: Optional[dict]) -> Optional[str]:
    """Retourne l'etat final (celui deja enregistre si deja regle -- pas de
    retraitement, protection contre le rejeu), ou None si l'identifiant est
    inconnu ou n'appartient pas a ce module."""
    with get_conn() as conn:
        row = conn.execute(
            "SELECT state FROM command WHERE correlation_id = ? AND module_id = ?",
            (correlation_id, module_id),
        ).fetchone()
        if not row:
            return None
        if row["state"] != "pending":
            return row["state"]  # deja regle : no-op, on renvoie l'etat existant
        conn.execute(
            "UPDATE command SET state = ?, settled_at = ?, result = ? "
            "WHERE correlation_id = ? AND module_id = ?",
            (state, utcnow(), json.dumps(result) if result is not None else None,
             correlation_id, module_id),
        )
        return state
