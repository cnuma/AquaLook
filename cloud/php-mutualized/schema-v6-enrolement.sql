-- ═══════════════════════════════════════════════════════════════════════════
--  AquaLook -- schema v6 : enrolement par code court (D016, lot D)
--
--  A injecter dans phpMyAdmin APRES les schemas v1 a v5, la base selectionnee
--  dans la colonne de gauche (sinon : #1046 No database selected).
--
--  Rejouable sur MariaDB (IF NOT EXISTS sur tables et colonne), ce que
--  fournit AlwaysData.
-- ═══════════════════════════════════════════════════════════════════════════

-- ── Dernier proprietaire d'un module detache ───────────────────────────────
--
-- Un proprietaire qui detache son module peut garder l'historique cote
-- serveur (pour le rattacher de nouveau plus tard). Si c'est un AUTRE compte
-- qui le rattache ensuite (module revendu), cet historique doit etre purge
-- comme lors d'un transfert : sans cette colonne, owner_user_id NULL ne
-- permettait plus de savoir a qui il appartenait.
ALTER TABLE module ADD COLUMN IF NOT EXISTS released_owner_user_id BIGINT NULL;

-- ── Demandes d'enrolement (modele RFC 8628) ────────────────────────────────
--
-- Le module demande un code (POST /v1/enroll/start), l'affiche sur son LCD ;
-- l'utilisateur le saisit dans son espace (POST /app/module/claim) ; le
-- module recoit son jeton au poll suivant (POST /v1/enroll/poll).
--
-- Aucun code en clair : seules les empreintes SHA-256 sont stockees, comme
-- pour les jetons de module. Le device_code fait 256 bits ; le user_code
-- n'en fait que 40 (8 caracteres sur 32), ce qui suffit parce qu'il expire
-- en 10 minutes, ne sert qu'une fois et que les essais sont limites
-- (enroll_claim_attempt).
--
-- Le jeton du module n'est PAS stocke ici : il est tire au premier poll qui
-- suit l'approbation, enregistre en empreinte dans module_token, rendu une
-- seule fois, et la demande passe a 'consumed'.
--
-- state : pending -> approved -> consumed, ou expired (remplacee par une
-- demande plus recente du meme hw_id, ou perimee).
CREATE TABLE IF NOT EXISTS enroll_request (
    id                 BIGINT AUTO_INCREMENT PRIMARY KEY,
    device_code_sha256 CHAR(64) NOT NULL UNIQUE,
    user_code_sha256   CHAR(64) NOT NULL,
    hw_id              VARCHAR(20) NOT NULL,
    firmware           VARCHAR(64) NULL,
    ip                 VARCHAR(45) NULL,
    created_at         DATETIME(3) NOT NULL,
    expires_at         DATETIME(3) NOT NULL,
    last_poll_at       DATETIME(3) NULL,
    state              VARCHAR(16) NOT NULL DEFAULT 'pending',
    -- Renseignes a l'approbation : le compte qui a saisi le code et le
    -- module_id attribue (hw_id pour un nouveau module, l'existant sinon).
    user_id            BIGINT NULL,
    module_id          VARCHAR(64) NULL,
    approved_at        DATETIME(3) NULL,
    INDEX idx_enroll_user_code (user_code_sha256, state),
    INDEX idx_enroll_hw_ts (hw_id, created_at),
    INDEX idx_enroll_ip_ts (ip, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ── Essais de saisie d'un code ─────────────────────────────────────────────
--
-- Meme principe que login_attempt : les echecs sont comptes par compte ET
-- par adresse, pour qu'un compte ne puisse pas balayer l'espace des codes
-- en cours de validite.
CREATE TABLE IF NOT EXISTS enroll_claim_attempt (
    id        BIGINT AUTO_INCREMENT PRIMARY KEY,
    ts        DATETIME(3) NOT NULL,
    user_id   BIGINT NULL,
    ip        VARCHAR(45) NULL,
    succeeded TINYINT(1) NOT NULL DEFAULT 0,
    INDEX idx_claim_user_ts (user_id, ts),
    INDEX idx_claim_ip_ts (ip, ts)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
