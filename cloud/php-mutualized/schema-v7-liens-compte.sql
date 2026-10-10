-- ═══════════════════════════════════════════════════════════════════════════
--  AquaLook -- schema v7 : liens de compte (D016, lot B)
--
--  Mot de passe oublie et invitation. A injecter dans phpMyAdmin APRES les
--  schemas v1 a v6, la base selectionnee dans la colonne de gauche (sinon :
--  #1046 No database selected).
--
--  Rejouable : uniquement CREATE TABLE IF NOT EXISTS.
-- ═══════════════════════════════════════════════════════════════════════════

-- ── Liens a usage unique envoyes par mail ──────────────────────────────────
--
-- Une seule table pour les deux usages : un lien de reinitialisation et un
-- lien d'invitation font la meme chose (poser le mot de passe d'un compte),
-- seules leur duree de vie et la formulation du mail different.
--
-- Seule l'empreinte SHA-256 du jeton est stockee, comme pour les sessions
-- et les jetons de module : une sauvegarde egaree de la base ne doit pas
-- permettre de prendre un compte. 256 bits aleatoires, donc SHA-256 suffit
-- (pas de password_hash : rien a ralentir, et il faut une recherche par
-- index).
--
-- used_at : un lien consomme reste en base jusqu'a la purge (cleanup.php),
-- pour qu'un deuxieme clic reponde "deja utilise" plutot que "inconnu".
CREATE TABLE IF NOT EXISTS account_token (
    id           BIGINT AUTO_INCREMENT PRIMARY KEY,
    token_sha256 CHAR(64) NOT NULL UNIQUE,
    user_id      BIGINT NOT NULL,
    purpose      VARCHAR(8) NOT NULL,          -- 'reset' ou 'invite'
    created_at   DATETIME(3) NOT NULL,
    expires_at   DATETIME(3) NOT NULL,
    used_at      DATETIME(3) NULL,
    ip           VARCHAR(45) NULL,
    CONSTRAINT fk_account_token_user FOREIGN KEY (user_id)
        REFERENCES app_user(user_id) ON DELETE CASCADE,
    INDEX idx_account_token_user (user_id, purpose),
    INDEX idx_account_token_expiry (expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
