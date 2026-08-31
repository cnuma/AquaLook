-- Structure de la base AquaLook -- a importer tel quel dans phpMyAdmin.
--
-- Mode d emploi (AlwaysData) :
--   1. Creer la base depuis le panneau : Bases de donnees > MySQL > Ajouter.
--   2. Ouvrir phpMyAdmin et SELECTIONNER cette base dans la colonne de gauche.
--   3. Onglet SQL, coller ce fichier, executer.
--
-- Ce fichier ne contient volontairement ni CREATE DATABASE ni USE : la base
-- est creee par le panneau de l hebergeur, et phpMyAdmin importe dans celle
-- qui est selectionnee. Les inclure ferait echouer l import sur un compte
-- mutualise, ou l on n a pas le droit de creer une base en SQL.
--
-- ATTENTION : ce fichier n est PAS une migration. Il ne contient que des
-- CREATE TABLE IF NOT EXISTS. Sur une base vierge il fait le travail ; sur
-- une base existante il ne modifie RIEN, en silence. Le piege s est deja
-- referme le 31 aout 2026 : une table module_token gardait son ancienne
-- colonne token en clair alors que le code attendait token_sha256, et le
-- serveur repondait 500 sans que rien ne signale la cause.
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
    last_seen  DATETIME(3) NULL,
    created_at DATETIME(3) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Le jeton n'est PAS stocke. Seule son empreinte l'est : quelqu'un qui
-- lirait cette table - sauvegarde egaree, injection, acces prestataire -
-- ne pourrait pas se faire passer pour un module.
--
-- SHA-256 et non bcrypt/argon2, et c'est deliberé. Ces derniers existent
-- pour ralentir la force brute sur des mots de passe humains, a faible
-- entropie. Un jeton est ici 256 bits aleatoires : le forcer est hors de
-- portee quel que soit le cout du hachage. En revanche un hachage rapide
-- et deterministe permet l'index UNIQUE ci-dessous, donc une recherche en
-- O(1) - impossible avec bcrypt, dont le sel interdit toute comparaison
-- par index.
CREATE TABLE IF NOT EXISTS module_token (
    module_id    VARCHAR(64) PRIMARY KEY,
    token_sha256 CHAR(64) NOT NULL UNIQUE,
    created_at DATETIME(3) NOT NULL,
    CONSTRAINT fk_module_token_module FOREIGN KEY (module_id)
        REFERENCES module(module_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Tracabilite exigee par SYSTEM_ARCHITECTURE.md Sec.7 : emetteur, resultat,
-- identifiant de correlation (protection contre le rejeu, voir db.php).
CREATE TABLE IF NOT EXISTS command (
    -- Ordre d'emission, source de verite du FIFO.
    --
    -- Un horodatage ne suffit pas : utc_now() produit des millisecondes,
    -- mais un DATETIME sans fraction les arrondit a la seconde, et rien ne
    -- departageait deux commandes emises dans la meme seconde. Une sequence
    -- monotone rend l'ordre exact et deterministe, quel que soit le rythme
    -- d'emission. AUTO_INCREMENT impose une cle : d'ou le UNIQUE.
    seq            BIGINT AUTO_INCREMENT UNIQUE,
    correlation_id VARCHAR(64) PRIMARY KEY,
    module_id      VARCHAR(64) NOT NULL,
    -- DATETIME(3) comme module_message.ts : la precision milliseconde de
    -- utc_now() est desormais reellement conservee, et non arrondie.
    issued_at      DATETIME(3) NOT NULL,
    issued_by      VARCHAR(64) NULL,
    command        JSON NOT NULL,
    state          VARCHAR(16) NOT NULL DEFAULT 'pending',  -- pending|accepted|refused|failed|expired
    settled_at     DATETIME(3) NULL,
    result         JSON NULL,
    INDEX idx_command_module_state (module_id, state, seq)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
