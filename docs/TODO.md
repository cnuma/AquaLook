# AquaLook — Reste à faire (liste unique, par priorité)

Créée le 11 octobre 2026 (base `main` = `5606609`, firmware 5.13.1) en
consolidant les §7 et §8 des checkpoints `370a6eb` → `c8b2a66` et
`docs/codex/08_RISKS_AND_DEBT.md`. **Cette liste remplace les TODO éparses des
checkpoints** : un checkpoint renvoie ici et ne recopie que ce qui a changé.
Tenir à jour à chaque palier (cocher, déplacer, dater).

Légende : **P0** en cours · **P1** avant toute mise en service réelle ·
**P2** fonctionnel attendu · **P3** petits défauts · **P4** dette et documentation.
Colonne « Qui » : *agent* (code), *propriétaire* (geste physique, FTP,
décision), *les deux*.

## P0 — En cours (session du 11 oct. 2026)

| # | Sujet | Qui | État |
|---|---|---|---|
| P0.1 | CloudSync : l'accusé d'une commande attendait un intervalle complet (`_ackSyncSoon` remettait `_lastSyncEpochSec` à 0 → « première échéance ») | agent | **validé** sur `.141` (build 1384) : accusé 15 s après application — `1e16639` |
| P0.2 | CloudSync : un sondage `/v1/pending-command` en échec (connexion, réponse, statut non 2xx) était muet quand rapport et config réussissaient | agent | corrigé, `00e8053` ; non-régression vérifiée, cas d'erreur non provoqué sur le matériel |
| P0.3 | Mot de passe Web de `.141` = mot de passe de banc divulgué dans un chat : le remplacer (sans le committer, `.env` local) | les deux | **fait** le 11 oct. 2026 : changé par l'éditeur (bouton « Secret », envoi chiffré), `.env` mis à jour ; nouveau accepté, ancien refusé (403) par `tools/module_session.py` |
| P0.4a | AlwaysData : déployer depuis Git (`tools/deploy_cloud.py`, FTPS, relecture « ALIGNE ») | agent ✔ / propriétaire | outil **fait et testé sur banc FTP local** le 11 oct. ; reste au propriétaire : ajouter au `.env` racine `AQUALOOK_FTP_HOST`, `AQUALOOK_FTP_USER`, `AQUALOOK_FTP_PASSWORD` (obligatoires) et, si le site n'est pas dans `www`, `AQUALOOK_FTP_DIR` (`AQUALOOK_FTP_PORT` ne sert qu'au banc local `--banc`), lancer `verifier` puis `publier` (premier passage : écarts « CRLF seulement » attendus, et `router.php` signalé présent sur le serveur) |
| P0.4b | AlwaysData : externaliser les scripts inline d'`admin.html`/`app.html` pour retirer `'unsafe-inline'` de `script-src` ; garder `style-src 'unsafe-inline'` | agent | **fait sur banc** le 11 oct. 2026 (branche `feature/csp-sans-unsafe-inline`) : `admin.js`/`app.js`, CSP stricte par `<FilesMatch>` ; aucun attribut `on*=` (gestionnaires déjà posés en JS) ; banc Edge sans interface (CDP) sur PHP + MariaDB locale (`aqualook_banc_d`) : parcours complet console + espace utilisateur sans violation, identique à `main`, témoin négatif détecté. **Reste : publier avec P0.4a** (`deploy_cloud.py` publie les `.js` avant les `.html`), puis vérifier les en-têtes en production. `/editeur/` garde `'unsafe-inline'` → P0.4d |
| P0.4d | `/editeur/` (copie de `data/scripts-schema.html`, script inline partagé avec la SD du module) : l'externaliser pour étendre la CSP stricte | agent ✔ / propriétaire (FTP) | **script externalisé** le 11 oct. 2026 (branche `feature/editeur-sans-script-inline`) : bloc inline (l. 498-2105) sorti **à l'octet près** dans `data/script-editeur.js` (reconstitution vérifiée) ; aucun attribut `on*=`. `tools/editeur_banc.py` : 65/65 avant et après ; banc CSP (page servie en HTTP avec la CSP stricte de la `.htaccess`, module simulé en fichiers externes, capteur `securitypolicyviolation`) : 65/65, **0 violation** sur les 4 scénarios ; témoin négatif (`main`) : page inerte, violation l. 499. Déposé sur la SD de `.141` (empreintes identiques). **Reste** : (1) contrôle visuel de l'éditeur sur `.141` ; (2) recopier `scripts-schema.html` **et** `script-editeur.js` dans `/editeur/` (P2.1) ; (3) **seulement ensuite**, retirer `'unsafe-inline'` de `script-src` dans la CSP générale de la `.htaccess` (le `<FilesMatch>` de P0.4b devient alors inutile) — publier la `.htaccess` stricte avant la recopie rendrait l'éditeur en ligne inerte |
| P0.4c | AlwaysData : chiffrer le mot de passe SMTP en base (`settings.php`, clé `SETTINGS_KEY` dans le `.env` du serveur, lecture des anciennes valeurs en clair) | agent ✔ + propriétaire (clé) | **fait sur banc** le 11 oct. 2026 (branche `feature/smtp-chiffre`) : XChaCha20-Poly1305 AEAD (libsodium) plutôt que `secretbox`, pour lier le chiffré au nom du paramètre ; format `enc1:` ; ancienne valeur claire lue puis **rechiffrée d'elle-même** à la première lecture dès que la clé existe ; sans clé, un nouveau secret est **refusé** (jamais écrit en clair) ; clé changée → secret vide et « illisible » dans la console (pas de repli silencieux) ; secrets limités à 128 octets (colonne de 255). Banc PHP + MariaDB (`aqualook_banc_d`) : 25 contrôles OK + API `/admin/settings` en HTTP. **Reste au propriétaire, AVANT de publier** : générer la clé (`php -r "echo base64_encode(random_bytes(32));"`) et l'ajouter comme `SETTINGS_KEY=` au `.env` **du serveur** (pas au dépôt) ; garder une copie de la clé hors du serveur ; après publication, ouvrir Paramètres dans la console : le mot de passe doit apparaître « chiffré » |
| P0.5 | HTTPS des pages sur le réseau local (aujourd'hui HTTP en clair : cookie de session captable 30 min). Chantier lourd : serveur TLS sur l'ESP32 (ESPAsyncWebServer n'en a pas), certificat par module, mémoire, confiance navigateur. Étude de faisabilité d'abord | agent | à étudier |

## P1 — Avant toute mise en service réelle

| # | Sujet | Qui |
|---|---|---|
| P1.1 | Wi-Fi mesh : test de bascule de nœud (rapprocher une borne secondaire, redémarrer le module, vérifier `wifi.bssid`, débrancher la borne) ; reconnexion après coupure réelle et après redémarrage logiciel | les deux |
| P1.2 | Vers le 14 oct. : relire l'historique `payload.wifi` de `Jardin-01` et décider du « point 2 » (changer de nœud sans déconnexion ; branche `feature/wifi-diag-bssid`) | les deux |
| P1.3 | Migration du serveur Ubuntu 26 (19 oct. 2026) ; jeton admin de production | propriétaire |
| P1.4 | D9 : release GitHub sans binaire S3 (`ota-release.yml`, `generate_ota_manifest.py`, `version_build.py` → `ota_target=unsupported`) ; cible `s3`, essai sur `.141`, puis retrait des alias `_legacy`/`_v4` | agent |
| ~~P1.5~~ | ~~Délivrabilité des mails (indésirables Gmail)~~ — **réglé** (confirmé par le propriétaire le 11 oct. 2026) | — |
| P1.6 | Secret `ApiAuth` en clair dans la NVS (accès physique) : chiffrement NVS / flash, à évaluer | agent |
| P1.7 | PIN LCD à 4 chiffres : évaluer l'allongement | les deux |

## P2 — Fonctionnel attendu

| # | Sujet | Qui |
|---|---|---|
| P2.1 | Recopier `data/scripts-schema.html` (volets repliables P2.6 compris), **`data/script-editeur.js` (nouveau, P0.4d)** et `data/session.js` dans `/editeur/` (AlwaysData) ; paquet Web `5.10.4` (`publish_web_assets.py`, fichiers puis manifeste, `check_published_web.py`) ; tester « changer le secret » depuis l'éditeur | propriétaire (FTP) + agent |
| P2.2 | Lot G de D016 : effacement du PIN depuis l'espace en ligne | agent |
| P2.3 | Confirmer en production : invitation depuis la console, changement de mot de passe connecté, durée de `forgot` sur adresse connue | les deux |
| P2.4 | Confirmer au doigt (lot E) : geste d'effacement du PIN au démarrage, attentes successives ; relever la durée PBKDF2 | propriétaire |
| P2.5 | Portail captif : enregistrer un réseau depuis le portail (non essayé) | les deux |
| ~~P2.6~~ | ~~Éditeur de scripts sur tablette : volets latéraux repliables~~ — **fait** le 11 oct. 2026 (`4e93e4e`, `7ff0df2`) : pastilles sur les jonctions en T, tout écran hors téléphone, validé par le propriétaire sur tablette (`.141`, dépôt SD) ; recopie dans `/editeur/` jointe à P2.1 | — |
| P2.7 | Tag / release `v5.13.x` (non demandé à ce jour) | propriétaire (décision) |
| P2.8 | **Nouvelle fonctionnalité : notifications par mail** (demande du 11 oct. 2026). Envoi par le serveur AlwaysData (pas par le module), réglé depuis le profil de l'utilisateur dans son espace en ligne (adresse, types d'événements, activation). Plafonds vérifiés côté serveur (par utilisateur, par module, par jour, en plus de `MAIL_MAX_PER_DAY` global), réglables sans recompilation (registre `settings.php` + console admin). Délivrabilité réglée (P1.5) | agent (+ décisions propriétaire) |
| P2.9 | **Espace utilisateur (`app.html`/`app.js`, AlwaysData) plus propre** (demande du 11 oct. 2026) : l'accueil (liste des modules) empile aujourd'hui les cartes « Rattacher un module » (`carteRattacher()`) et « Compte » (`carteCompte()`) sous la liste. Au minimum : un onglet (ou une page) « Rattacher un module » séparé de l'accueil ; mieux : repenser l'interface pour un rendu plus professionnel (navigation par onglets Modules / Rattacher / Mon compte, en-tête, hiérarchie visuelle). Garder les IDs et routes `/app/*` ; pas de script inline (CSP stricte P0.4b) ; à faire après la publication de P0.4b | agent (+ choix du propriétaire sur l'ampleur) |

## P3 — Petits défauts

| # | Sujet | Où |
|---|---|---|
| P3.1 | Bannière série du S3 : `Cible OTA : unsupported`, `Carte : esp32-2432S028` | sortie de boot, `version_build.py` |
| P3.2 | Erreur SD `health_check_failed` sur `/www/index.html` | `StorageManager` |
| P3.3 | Variables locales `a..h` : `indexOf` accepte `ab` | `data/script-lang.js` |
| P3.4 | Doublon d'ID `btn-toggle-activity` (D5) — 2 occurrences vérifiées le 11 oct. | `data/index.html` |
| P3.5 | `FORCE_SYNC_MIN_GAP_SEC` (60 s) en dur, pas réglable sans recompilation | `src/CloudSync.h` |
| P3.6 | Outils `tools/robustesse/fuzz*.py` sans session (401 depuis le lot F) | `tools/robustesse/` |
| P3.7 | Journal série bruyant sans SD (`vfs_api open(): /littlefs/... does not exist`) | `WebManager` (`serveStatic`) |
| P3.8 | Scan Web `?refresh=1` pendant une connexion Wi-Fi : coordination limitée à `_scanPending` | `WiFiManager` |
| P3.9 | Build/SHA identiques pour un binaire construit avec du code non committé (l'empreinte ELF les distingue) | `version_build.py` |
| P3.10 | « Oublier le compte » sur le LCD sans détachement en ligne → `403 already_enrolled` : voulu, à expliquer dans l'aide | aide utilisateur |

## P4 — Dette technique et documentation

| # | Sujet |
|---|---|
| P4.1 | D6 : pas de CI (compilation S3, buildfs, diff check, validation HTML) |
| P4.2 | D7 : peu de tests automatiques du planning (pluie, intervalle, minuit, durée max) |
| P4.3 | D8 : capacité 16 / fonctionnelle 8, garde `MAX_ACTIVE_ZONES` sans test |
| P4.4 | D1 : scan I²C de diagnostic au boot ; D3 : env `debug_boot` fragile |
| P4.5 | Documentation périmée : `docs/START_HERE.md`, `docs/engineering/01_PROJECT_STATUS.md` (en-tête de `docs/REPRISE_INGENIEUR.md` corrigé le 11 oct. 2026 : renvoie au fichier `VERSION` et à cette liste) |
| P4.6 | Sortir le dépôt d'OneDrive (`.pio/` ~830 Mo resynchronisé) |
| P4.7 | Patch AsyncTCP n°5 lié à l'ancre textuelle d'AsyncTCP 3.3.2 : revérifier après toute mise à jour de bibliothèque |
| P4.8 | Tests actifs du site cloud plus poussés (à scoper, faible débit, depuis la machine du propriétaire) |
