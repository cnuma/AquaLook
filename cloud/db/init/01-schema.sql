-- Schema initial AquaLook (execute une seule fois, a la creation du volume).
--
-- Choix : PostgreSQL + TimescaleDB plutot qu'une base purement temporelle.
-- L'historique des messages est une serie temporelle (hypertable), mais le
-- reste du domaine — modules, sites, utilisateurs a venir — est relationnel
-- classique. Une seule base evite d'en administrer deux.

CREATE EXTENSION IF NOT EXISTS timescaledb;

-- ── Referentiel des modules ────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS module (
    module_id     text PRIMARY KEY,           -- identifiant unique, = utilisateur MQTT
    label         text,
    firmware      text,
    last_seen     timestamptz,
    created_at    timestamptz NOT NULL DEFAULT now()
);

-- ── Historique des messages recus ──────────────────────────────────────────
-- Charge utile conservee en JSONB : les contrats evoluent (versionnes), et on
-- veut pouvoir relire l'historique ancien sans migration destructive.
CREATE TABLE IF NOT EXISTS module_message (
    ts             timestamptz NOT NULL,
    module_id      text        NOT NULL,
    proto_version  text        NOT NULL,
    msg_type       text        NOT NULL,      -- status | state | event | diag | ack
    correlation_id text,                      -- rattache un acquittement a sa commande
    payload        jsonb       NOT NULL
);

SELECT create_hypertable('module_message', 'ts', if_not_exists => TRUE);

CREATE INDEX IF NOT EXISTS module_message_module_ts_idx
    ON module_message (module_id, ts DESC);
CREATE INDEX IF NOT EXISTS module_message_type_ts_idx
    ON module_message (msg_type, ts DESC);

-- ── Suivi des commandes ────────────────────────────────────────────────────
-- Tracabilite exigee par SYSTEM_ARCHITECTURE.md : emetteur, resultat,
-- identifiant de correlation, et protection contre le rejeu (correlation_id
-- unique).
CREATE TABLE IF NOT EXISTS command (
    correlation_id text PRIMARY KEY,
    module_id      text        NOT NULL REFERENCES module(module_id),
    issued_at      timestamptz NOT NULL DEFAULT now(),
    issued_by      text,
    command        jsonb       NOT NULL,
    state          text        NOT NULL DEFAULT 'pending',  -- pending|accepted|refused|failed|expired
    settled_at     timestamptz,
    result         jsonb
);

CREATE INDEX IF NOT EXISTS command_module_issued_idx
    ON command (module_id, issued_at DESC);

-- Retention : a activer une fois le volume reel connu, pas avant.
-- SELECT add_retention_policy('module_message', INTERVAL '18 months');
