"""Passerelle MQTT vers PostgreSQL/TimescaleDB pour AquaLook.

Role : maintenir une souscription MQTT permanente, valider les messages recus
et les persister. C'est precisement la piece qu'un hebergement web mutualise
ne peut pas fournir (voir docs/architecture/CLOUD_ENVIRONMENT_EVALUATION.md,
section 2) : un script PHP s'execute par requete et s'arrete, il ne peut pas
tenir une connexion longue.

Expose en plus une petite API de lecture, utile a l'interface et aux controles
de sante. La logique metier d'arrosage n'est PAS ici : l'ESP32 reste l'autorite
locale, le cloud transporte, historise et supervise (invariant de
SYSTEM_ARCHITECTURE.md).
"""

from __future__ import annotations

import json
import logging
import os
import re
import threading
from contextlib import asynccontextmanager
from datetime import datetime, timezone

import paho.mqtt.client as mqtt
import psycopg
from fastapi import FastAPI, HTTPException
from psycopg_pool import ConnectionPool

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("bridge")

MQTT_HOST = os.environ.get("MQTT_HOST", "mosquitto")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_USERNAME = os.environ["MQTT_USERNAME"]
MQTT_PASSWORD = os.environ["MQTT_PASSWORD"]
DATABASE_URL = os.environ["DATABASE_URL"]
PROTO = os.environ.get("PROTO_VERSION", "v1")

# aqualook/<version>/<moduleId>/<type>[/<sous-type>]
TOPIC_RE = re.compile(r"^aqualook/(?P<ver>[^/]+)/(?P<module>[^/]+)/(?P<kind>[^/]+)(?:/(?P<sub>[^/]+))?$")

# Un identifiant de module doit rester borne et sans surprise : il sert de cle
# primaire et de nom d'utilisateur MQTT.
MODULE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$")

MAX_PAYLOAD_BYTES = 64 * 1024

pool = ConnectionPool(DATABASE_URL, min_size=1, max_size=5, open=False)


def persist(module_id: str, version: str, msg_type: str, payload: dict) -> None:
    correlation = payload.get("correlationId")
    # Horodatage : celui du module s'il est fourni et plausible, sinon celui de
    # reception. Un module dont l'heure est fausse ne doit pas polluer la serie.
    ts = datetime.now(timezone.utc)
    with pool.connection() as conn:
        conn.execute(
            "INSERT INTO module (module_id, last_seen) VALUES (%s, %s) "
            "ON CONFLICT (module_id) DO UPDATE SET last_seen = EXCLUDED.last_seen",
            (module_id, ts),
        )
        conn.execute(
            "INSERT INTO module_message (ts, module_id, proto_version, msg_type, correlation_id, payload) "
            "VALUES (%s, %s, %s, %s, %s, %s)",
            (ts, module_id, version, msg_type, correlation, json.dumps(payload)),
        )
        # Un acquittement solde la commande correspondante.
        if msg_type == "ack" and correlation:
            conn.execute(
                "UPDATE command SET state = %s, settled_at = %s, result = %s "
                "WHERE correlation_id = %s",
                (payload.get("state", "accepted"), ts, json.dumps(payload), correlation),
            )


def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code != 0:
        log.error("connexion MQTT refusee: %s", reason_code)
        return
    topic = f"aqualook/{PROTO}/+/#"
    client.subscribe(topic, qos=1)
    log.info("connecte au broker, abonne a %s", topic)


def on_message(client, userdata, msg):
    # Refus sur : sujet non conforme, version inattendue, identifiant invalide,
    # charge trop grosse, JSON illisible. « Refus sur en cas de message
    # incomplet, invalide ou incompatible » (SYSTEM_ARCHITECTURE.md, section 7).
    m = TOPIC_RE.match(msg.topic)
    if not m:
        log.warning("sujet ignore (forme inattendue): %s", msg.topic)
        return
    if m["ver"] != PROTO:
        log.warning("version de protocole ignoree: %s", msg.topic)
        return
    if not MODULE_ID_RE.match(m["module"]):
        log.warning("identifiant de module invalide: %s", m["module"])
        return
    if len(msg.payload) > MAX_PAYLOAD_BYTES:
        log.warning("charge trop volumineuse (%d octets) sur %s", len(msg.payload), msg.topic)
        return

    kind = "ack" if (m["kind"] == "cmd" and m["sub"] == "ack") else m["kind"]
    if kind not in {"status", "state", "event", "diag", "ack"}:
        log.warning("type de message inconnu: %s", kind)
        return

    try:
        payload = json.loads(msg.payload.decode("utf-8"))
        if not isinstance(payload, dict):
            raise ValueError("charge utile non objet")
    except Exception as exc:
        log.warning("charge illisible sur %s: %s", msg.topic, exc)
        return

    try:
        persist(m["module"], m["ver"], kind, payload)
    except psycopg.Error as exc:
        # Ne jamais faire tomber la passerelle sur une erreur de base : le
        # broker continue de recevoir, et la reconnexion du pool reprendra.
        log.error("ecriture en base impossible (%s): %s", msg.topic, exc)


def mqtt_loop() -> None:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="aqualook-bridge", clean_session=False)
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=60)
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_forever(retry_first_connection=True)


@asynccontextmanager
async def lifespan(app: FastAPI):
    pool.open()
    threading.Thread(target=mqtt_loop, name="mqtt", daemon=True).start()
    yield
    pool.close()


app = FastAPI(title="AquaLook bridge", lifespan=lifespan)


@app.get("/health")
def health():
    with pool.connection() as conn:
        conn.execute("SELECT 1")
    return {"ok": True}


@app.get("/modules")
def modules():
    with pool.connection() as conn:
        rows = conn.execute(
            "SELECT module_id, label, firmware, last_seen FROM module ORDER BY module_id"
        ).fetchall()
    return [{"moduleId": r[0], "label": r[1], "firmware": r[2], "lastSeen": r[3]} for r in rows]


@app.get("/modules/{module_id}/messages")
def messages(module_id: str, limit: int = 100):
    if not MODULE_ID_RE.match(module_id):
        raise HTTPException(status_code=400, detail="identifiant invalide")
    limit = max(1, min(limit, 1000))
    with pool.connection() as conn:
        rows = conn.execute(
            "SELECT ts, msg_type, correlation_id, payload FROM module_message "
            "WHERE module_id = %s ORDER BY ts DESC LIMIT %s",
            (module_id, limit),
        ).fetchall()
    return [{"ts": r[0], "type": r[1], "correlationId": r[2], "payload": r[3]} for r in rows]
