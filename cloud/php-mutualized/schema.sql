-- Schema AquaLook pour hebergement mutualise (MySQL/MariaDB).
--
-- Meme forme logique que cloud/db/init/01-schema.sql (PostgreSQL/TimescaleDB,
-- piste VPS) et cloud/api/app/db.py (SQLite, piste locale) : module /
-- module_message / command sont independants du moteur de base comme du
-- transport. Adapte ici pour MySQL/MariaDB, ce qu'un hebergement mutualise
-- OVH fournit reellement.

CREATE TABLE IF NOT EXISTS module (
    module_id  VARCHAR(64) PRIMARY KEY,
    label      VARCHAR(255) NULL,
    firmware   VARCHAR(64) NULL,
    last_seen  DATETIME NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS module_token (
    module_id  VARCHAR(64) PRIMARY KEY,
    token      VARCHAR(128) NOT NULL UNIQUE,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_module_token_module FOREIGN KEY (module_id)
        REFERENCES module(module_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Charge utile en JSON (type natif MySQL 5.7+/MariaDB 10.2+) : les contrats
-- evoluent, versionnes, sans migration destructive sur l'historique ancien.
CREATE TABLE IF NOT EXISTS module_message (
    id             BIGINT AUTO_INCREMENT PRIMARY KEY,
    ts             DATETIME(3) NOT NULL,
    module_id      VARCHAR(64) NOT NULL,
    proto_version  VARCHAR(8) NOT NULL,
    msg_type       VARCHAR(16) NOT NULL,   -- status | state | event | diag
    correlation_id VARCHAR(64) NULL,
    payload        JSON NOT NULL,
    INDEX idx_module_message_module_ts (module_id, ts DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Tracabilite exigee par SYSTEM_ARCHITECTURE.md Sec.7 : emetteur, resultat,
-- identifiant de correlation (protection contre le rejeu, voir db.php).
CREATE TABLE IF NOT EXISTS command (
    correlation_id VARCHAR(64) PRIMARY KEY,
    module_id      VARCHAR(64) NOT NULL,
    issued_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    issued_by      VARCHAR(64) NULL,
    command        JSON NOT NULL,
    state          VARCHAR(16) NOT NULL DEFAULT 'pending',  -- pending|accepted|refused|failed|expired
    settled_at     DATETIME NULL,
    result         JSON NULL,
    INDEX idx_command_module_state (module_id, state, issued_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
