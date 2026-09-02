-- Comptes utilisateurs, sessions et propriete des modules.
--
-- A importer dans phpMyAdmin APRES schema.sql, sur la base deja en service.
-- Ce fichier est une MIGRATION : il modifie une table existante (module), il
-- ne peut donc pas etre rejoue a l'identique sans precaution -- l'ajout de
-- colonne echouerait la seconde fois. Chaque instruction est commentee pour
-- que l'echec eventuel soit lisible.
--
-- Rappel : ne contient ni CREATE DATABASE ni USE. Selectionner la base dans
-- phpMyAdmin avant d'executer.

-- ── Comptes ────────────────────────────────────────────────────────────────
--
-- Le mot de passe n'est PAS stocke : seule une empreinte password_hash() de
-- PHP l'est, en Argon2id ou bcrypt selon la version du serveur. Contrairement
-- aux jetons de module -- 256 bits aleatoires, haches en SHA-256 pour
-- permettre une recherche par index -- un mot de passe humain a une entropie
-- faible et doit etre ralenti deliberement. C'est exactement le raisonnement
-- inverse de celui note sur module_token, et pour la meme raison de fond.
--
-- 255 caracteres : password_hash() n'en produit que 60 en bcrypt, mais la
-- documentation PHP recommande cette largeur pour survivre a un changement
-- d'algorithme sans migration.
CREATE TABLE IF NOT EXISTS app_user (
    user_id       BIGINT AUTO_INCREMENT PRIMARY KEY,
    email         VARCHAR(190) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    label         VARCHAR(120) NULL,
    created_at    DATETIME(3) NOT NULL,
    last_login    DATETIME(3) NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
-- 190 et non 255 pour email : avec utf8mb4 (4 octets par caractere) un index
-- UNIQUE est plafonne a 767 octets sur les anciennes versions de MySQL.
-- 190 x 4 = 760, ce qui passe partout.

-- ── Sessions ───────────────────────────────────────────────────────────────
--
-- Session en base et non en fichier PHP : sur un hebergement mutualise, les
-- sessions fichier vivent dans un repertoire partage dont on ne maitrise ni
-- la duree de vie ni le nettoyage. En base, l'expiration est explicite et la
-- deconnexion est reelle.
--
-- Le cookie porte un jeton aleatoire de 256 bits ; seule son empreinte est
-- stockee, pour la meme raison que les jetons de module : une sauvegarde
-- egaree ne doit pas permettre d'usurper une session.
CREATE TABLE IF NOT EXISTS app_session (
    token_sha256 CHAR(64) PRIMARY KEY,
    user_id      BIGINT NOT NULL,
    created_at   DATETIME(3) NOT NULL,
    expires_at   DATETIME(3) NOT NULL,
    last_seen    DATETIME(3) NOT NULL,
    CONSTRAINT fk_session_user FOREIGN KEY (user_id)
        REFERENCES app_user(user_id) ON DELETE CASCADE,
    INDEX idx_session_expiry (expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ── Tentatives de connexion ────────────────────────────────────────────────
--
-- Le README listait l'absence de limitation de debit parmi les manques
-- assumes. Elle cesse d'etre acceptable des qu'un mot de passe protege
-- l'acces : sans elle, un formulaire de connexion se force en quelques
-- heures. On compte les echecs par identifiant ET par adresse, pour qu'un
-- attaquant ne puisse ni marteler un compte, ni balayer plusieurs comptes
-- depuis une meme source.
CREATE TABLE IF NOT EXISTS login_attempt (
    id         BIGINT AUTO_INCREMENT PRIMARY KEY,
    ts         DATETIME(3) NOT NULL,
    email      VARCHAR(190) NULL,
    ip         VARCHAR(45) NULL,   -- 45 : longueur maximale d'une adresse IPv6
    succeeded  TINYINT(1) NOT NULL DEFAULT 0,
    INDEX idx_attempt_email_ts (email, ts),
    INDEX idx_attempt_ip_ts (ip, ts)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ── Configuration courante d'un module ─────────────────────────────────────
--
-- Instantane, distinct de l'historique de module_message.
--
-- La configuration arrive deja a chaque cycle sous forme de message de type
-- "config", et latest_config() sait retrouver le plus recent. Mais cet
-- historique est destine a etre elague un jour -- un module qui se synchronise
-- tous les quarts d'heure produit une centaine de messages par jour. Le jour
-- ou la purge existera, un module hors ligne depuis longtemps perdrait sa
-- configuration, et l'interface n'aurait plus rien a afficher.
--
-- Une ligne par module, remplacee a chaque remontee : la configuration
-- courante survit a toute purge de l'historique.
CREATE TABLE IF NOT EXISTS module_config (
    module_id  VARCHAR(64) PRIMARY KEY,
    revision   BIGINT NOT NULL,
    payload    JSON NOT NULL,
    updated_at DATETIME(3) NOT NULL,
    CONSTRAINT fk_module_config_module FOREIGN KEY (module_id)
        REFERENCES module(module_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ── Propriete des modules ──────────────────────────────────────────────────
--
-- Colonne ajoutee a une table existante : c'est la seule instruction de ce
-- fichier qui echouera si on le rejoue ("Duplicate column name"). L'erreur
-- est sans consequence, et la signaler vaut mieux que la masquer par un
-- bricolage de procedure stockee.
--
-- NULL autorise : les modules declares avant l'arrivee des comptes n'ont pas
-- de proprietaire. Ils restent visibles de la console d'administration, et
-- invisibles de l'espace utilisateur tant qu'ils ne sont pas rattaches.
ALTER TABLE module ADD COLUMN owner_user_id BIGINT NULL;
ALTER TABLE module ADD CONSTRAINT fk_module_owner FOREIGN KEY (owner_user_id)
    REFERENCES app_user(user_id) ON DELETE SET NULL;
ALTER TABLE module ADD INDEX idx_module_owner (owner_user_id);
