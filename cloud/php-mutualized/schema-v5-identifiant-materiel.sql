-- ═══════════════════════════════════════════════════════════════════════════
--  AquaLook -- schema v5 : identifiant materiel des modules (D016, lot C)
--
--  A injecter dans phpMyAdmin APRES les schemas v1 a v4, la base selectionnee
--  dans la colonne de gauche (sinon : #1046 No database selected).
--
--  Rejouable sur MariaDB (IF NOT EXISTS sur colonnes et index), ce que fournit
--  AlwaysData.
-- ═══════════════════════════════════════════════════════════════════════════

-- hw_id : "aql-" + MAC eFuse du module (DeviceIdentity.h), 16 caracteres.
-- Lie au module_id au PREMIER rapport authentifie qui le porte, puis fige :
-- l'index unique garantit qu'un meme boitier ne peut pas exister sous deux
-- module_id. NULL tant que le module n'a pas envoye de rapport avec un
-- firmware qui le connait.
--
-- hw_id_conflict : dernier identifiant annonce qui ne correspond pas a
-- hw_id (jeton recopie dans un autre boitier, carte remplacee), ou deja pris
-- par un autre module. Le rapport est accepte quand meme -- on ne coupe pas
-- la synchronisation d'un jardin sur un soupcon -- mais la console l'affiche.
-- Remis a NULL des que le module annonce de nouveau le bon identifiant.
ALTER TABLE module ADD COLUMN IF NOT EXISTS hw_id VARCHAR(20) NULL;
ALTER TABLE module ADD COLUMN IF NOT EXISTS hw_id_conflict VARCHAR(20) NULL;
ALTER TABLE module ADD UNIQUE INDEX IF NOT EXISTS uq_module_hw_id (hw_id);
