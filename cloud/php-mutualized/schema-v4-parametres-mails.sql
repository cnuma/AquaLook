-- ═══════════════════════════════════════════════════════════════════════════
--  AquaLook -- schema v4 : parametres du service et journal des mails (D016)
--
--  A injecter dans phpMyAdmin APRES schema.sql, schema-v2-comptes.sql et
--  schema-v3-sauvegarde.sql. Selectionner la base avant d'executer.
--
--  Rejouable : uniquement des CREATE TABLE IF NOT EXISTS.
-- ═══════════════════════════════════════════════════════════════════════════

-- ── Parametres modifiables depuis la console ───────────────────────────────
--
-- Une ligne par parametre surcharge ; un parametre absent retombe sur le
-- .env puis sur la valeur par defaut (voir settings.php, qui tient la liste
-- des noms acceptes). Les acces a la base et le jeton administrateur n'y
-- vont jamais : il faut les connaitre AVANT de pouvoir lire cette table.
--
-- value contient en clair le mot de passe SMTP lorsqu'il est regle ici :
-- l'API ne le relit jamais, mais une sauvegarde de la base le contient.
CREATE TABLE IF NOT EXISTS app_setting (
    name       VARCHAR(64) PRIMARY KEY,
    value      VARCHAR(255) NOT NULL,
    updated_at DATETIME(3) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ── Journal des envois ─────────────────────────────────────────────────────
--
-- Sert a limiter le debit AVANT d'envoyer (l'hebergeur coupe le site en cas
-- d'abus), et a diagnostiquer un echec sans acces aux journaux SMTP.
-- Chaque demande y entre, envoyee ou refusee par une limite.
--
-- L'adresse n'est pas conservee : seule son empreinte SHA-256 (adresse en
-- minuscules) sert a compter. error est une categorie (auth, contenu,
-- limite_adresse...), jamais un message du serveur SMTP.
CREATE TABLE IF NOT EXISTS mail_log (
    id               BIGINT AUTO_INCREMENT PRIMARY KEY,
    ts               DATETIME(3) NOT NULL,
    kind             VARCHAR(16) NOT NULL,
    recipient_sha256 CHAR(64) NOT NULL,
    ip               VARCHAR(45) NULL,
    ok               TINYINT(1) NOT NULL DEFAULT 0,
    error            VARCHAR(32) NULL,
    INDEX idx_mail_ts (ts),
    INDEX idx_mail_recipient_ts (recipient_sha256, ts),
    INDEX idx_mail_ip_ts (ip, ts)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
