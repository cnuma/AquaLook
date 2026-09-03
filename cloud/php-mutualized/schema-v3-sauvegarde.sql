-- ═══════════════════════════════════════════════════════════════════════════
--  AquaLook -- schema v3 : sauvegarde de l'installation
--
--  A injecter dans phpMyAdmin APRES schema.sql et schema-v2-comptes.sql.
--
--  Pourquoi une table de plus alors que module_config existe deja : cette
--  derniere ne garde qu'UNE ligne par module, ecrasee a chaque synchro. C'est
--  un miroir, pas une sauvegarde. Une configuration abimee sur le module
--  remplace la bonne dans le cloud en moins d'une minute, et il ne reste rien
--  a quoi revenir. Une sauvegarde suppose de pouvoir remonter le temps.
--
--  Ce fichier est rejouable : il ne contient que des CREATE TABLE IF NOT
--  EXISTS, contrairement a schema-v2-comptes.sql dont les ALTER TABLE
--  echouent au second passage.
-- ═══════════════════════════════════════════════════════════════════════════

-- ── Etats successifs de la configuration ───────────────────────────────────
--
-- Une ligne par etat DISTINCT. Le module renvoie sa configuration a chaque
-- cycle ; sans deduplication cette table grossirait de 1440 lignes par jour
-- pour decrire un jardin qui n'a pas bouge. payload_hash sert a cela : on
-- n'insere que si l'empreinte differe de la derniere enregistree.
--
-- Pas de contrainte d'unicite sur (module_id, payload_hash), volontairement.
-- Revenir a une configuration deja connue est un evenement en soi : il doit
-- produire une entree datee d'aujourd'hui, pas ressusciter celle d'il y a
-- trois mois.
--
-- La purge (les N plus recentes conservees) est faite en PHP a l'insertion,
-- pas par un evenement MySQL : l'hebergement mutualise ne garantit pas que
-- le planificateur d'evenements soit actif, et une purge qui ne tourne pas
-- en silence est pire que pas de purge du tout.
CREATE TABLE IF NOT EXISTS module_config_backup (
    id           BIGINT AUTO_INCREMENT PRIMARY KEY,
    module_id    VARCHAR(64) NOT NULL,
    revision     BIGINT NOT NULL,
    -- SHA-256 du payload JSON serialise, en hexadecimal.
    payload_hash CHAR(64) NOT NULL,
    payload      JSON NOT NULL,
    captured_at  DATETIME(3) NOT NULL,
    -- Renseigne quand l'utilisateur nomme une sauvegarde pour la retrouver
    -- ("avant refonte du massif"). NULL = capture automatique.
    label        VARCHAR(80) NULL,
    -- Marque les etats a conserver hors purge. Sans cela, une sauvegarde de
    -- reference disparaitrait au bout de N changements de creneaux.
    pinned       TINYINT(1) NOT NULL DEFAULT 0,
    KEY idx_backup_module (module_id, id DESC),
    CONSTRAINT fk_backup_module FOREIGN KEY (module_id)
        REFERENCES module(module_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
