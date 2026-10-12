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

## P0 — En cours (sessions des 11 et 12 oct. 2026)

| # | Sujet | Qui | État |
|---|---|---|---|
| P0.1 | CloudSync : l'accusé d'une commande attendait un intervalle complet (`_ackSyncSoon` remettait `_lastSyncEpochSec` à 0 → « première échéance ») | agent | **validé** sur `.141` (build 1384) : accusé 15 s après application — `1e16639` |
| P0.2 | CloudSync : un sondage `/v1/pending-command` en échec (connexion, réponse, statut non 2xx) était muet quand rapport et config réussissaient | agent | corrigé, `00e8053` ; non-régression vérifiée, cas d'erreur non provoqué sur le matériel |
| P0.3 | Mot de passe Web de `.141` = mot de passe de banc divulgué dans un chat : le remplacer (sans le committer, `.env` local) | les deux | **fait** le 11 oct. 2026 : changé par l'éditeur (bouton « Secret », envoi chiffré), `.env` mis à jour ; nouveau accepté, ancien refusé (403) par `tools/module_session.py` |
| ~~P0.4a~~ | ~~AlwaysData : déployer depuis Git (`tools/deploy_cloud.py`, FTPS, relecture « ALIGNE »)~~ | — | **fait** le 12 oct. 2026 : identifiants `AQUALOOK_FTP_HOST`, `_USER`, `_PASSWORD` dans le `.env` racine (FTPS TLS 1.3 vérifié) ; `deploy_cloud.py verifier` : les 17 fichiers de `cloud/php-mutualized`, PHP compris, **identiques** à `d92b424`, serveur ALIGNE. Hors dépôt sur le serveur : manifeste Web (normal), `php.log` et `router.php` (403) ; page d'exemple AlwaysData `index.html` supprimée (404). Incident du même jour : un `.env` local envoyé par FTP a remplacé celui du serveur, puis a été supprimé → service hors ligne (base injoignable, console 503) jusqu'à la remise du `.env` serveur ; jamais lisible depuis le Web (403) ; rétabli et vérifié (`/health` ok, console 401, SMTP « défini (chiffré) », synchro module). Règle : ne jamais envoyer de `.env` par FTP hors réparation ; préférer `deploy_cloud.py publier`, qui n'envoie que des fichiers suivis par Git et n'efface rien. Après chaque envoi manuel : `python tools/deploy_cloud.py verifier` |
| ~~P0.4b~~ | ~~AlwaysData : scripts d'`admin.html`/`app.html` externalisés, CSP sans `'unsafe-inline'`~~ | — | **en ligne** depuis le 12 oct. 2026 (fichiers identiques à Git, en-têtes vérifiés, `web_security_probe.py` tout PASS) |
| ~~P0.4d~~ | ~~`/editeur/` sans script inline (`data/script-editeur.js`), CSP stricte sur tout le site~~ | — | **en ligne** le 12 oct. 2026 : éditeur recopié dans `/editeur/` (identique à Git), CSP générale `script-src 'self'` vérifiée sur l'éditeur ; même lot : bouton « Réglages généraux » et textes du mot de passe (validés par le propriétaire) |
| ~~P0.4c~~ | ~~Mot de passe SMTP chiffré en base (XChaCha20-Poly1305, `SETTINGS_KEY`)~~ | — | **en ligne** le 12 oct. 2026 : clé posée dans le `.env` du serveur, console « défini (chiffré) » confirmée par le propriétaire |
| ~~P0.4e~~ | ~~Console admin derrière authentification HTTP Basic, site `noindex`~~ | — | **en ligne** le 12 oct. 2026 : `/admin.html` et `/admin.js` en 401 sans identifiant, `X-Robots-Tag` et `robots.txt` servis, parcours console / espace utilisateur / éditeur validé par le propriétaire. Suite (**en ligne** le 12 oct. 2026, vérifiée de l'extérieur) : page d'accueil neutre sur `/` (nom du produit et lien vers l'espace utilisateur seulement, au lieu du JSON « route inconnue ») ; route `GET /admin` (redirection vers la console) retirée, `/admin` en 404 ; fichiers PHP internes (`console_page.php`, `settings.php`, `db.php`…) refusés en accès direct (403, auparavant 200 vide), `web_security_probe.py` contrôle aussi la console, `X-Robots-Tag` et `script-src` de l'éditeur |
| ~~P0.4f~~ | ~~Espace utilisateur : fin de la connexion automatique de 30 jours ; case « Rester connecté »~~ | — | **en ligne** le 12 oct. 2026 (fichiers identiques à Git, case affichée « 7 jours », validé par le propriétaire) : case cochée → 7 jours fixes (cookie persistant) ; non cochée → cookie de navigateur, 2 h d'inactivité, 12 h au plus ; plafond de 7 jours appliqué aussi aux sessions déjà ouvertes (30 jours) ; durées réglables dans la console ; sans migration de schéma. Banc PHP + MariaDB : 14/14 (cookie, durées, glissement, plafond, ancienne session refusée) ; écran de connexion vérifié dans Edge. Publié par le propriétaire (FTP manuel) avec la page d'accueil, le retrait de `/admin` et le blocage des PHP internes (P0.4e), puis vérifié de l'extérieur (checkpoint `8a84ecb`) |
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
| P2.1 | **Publier le paquet Web des modules** (pages `data/` téléchargées par les modules lors d'une mise à jour Web). La recopie de `/editeur/` sur AlwaysData est faite (12 oct. 2026, P0.4d) ; les modules n'ont pas encore le nouvel éditeur. Paquet en ligne : `5.10.3` (manifeste du 8 oct. 2026). Numéro du prochain paquet à choisir à la publication : il doit seulement différer de `5.10.3` (le module compare les numéros par égalité, `WebAssetsUpdater.cpp`). Étapes : `python tools/publish_web_assets.py <version>`, envoi FTP des fichiers puis du manifeste en dernier, `check_published_web.py` ; mise à jour Web de `.141` ; tester « changer le secret » depuis l'éditeur | propriétaire (FTP) + agent |
| P2.2 | Lot G de D016 : effacement du PIN depuis l'espace en ligne | agent |
| P2.3 | Confirmer en production : invitation depuis la console, changement de mot de passe connecté, durée de `forgot` sur adresse connue | les deux |
| P2.4 | Confirmer au doigt (lot E) : geste d'effacement du PIN au démarrage, attentes successives ; relever la durée PBKDF2 | propriétaire |
| P2.5 | Portail captif : enregistrer un réseau depuis le portail (non essayé) | les deux |
| ~~P2.6~~ | ~~Éditeur de scripts sur tablette : volets latéraux repliables~~ — **fait** le 11 oct. 2026 (`4e93e4e`, `7ff0df2`) : pastilles sur les jonctions en T, tout écran hors téléphone, validé par le propriétaire sur tablette (`.141`, dépôt SD) ; recopie dans `/editeur/` jointe à P2.1 | — |
| P2.7 | Tag / release `v5.13.x` (non demandé à ce jour) | propriétaire (décision) |
| P2.8 | **Nouvelle fonctionnalité : notifications par mail** (demande du 11 oct. 2026). Envoi par le serveur AlwaysData (pas par le module), réglé depuis le profil de l'utilisateur dans son espace en ligne (adresse, types d'événements, activation). Plafonds vérifiés côté serveur (par utilisateur, par module, par jour, en plus de `MAIL_MAX_PER_DAY` global), réglables sans recompilation (registre `settings.php` + console admin). Délivrabilité réglée (P1.5) | agent (+ décisions propriétaire) |
| ~~P2.9~~ | ~~Espace utilisateur (`app.html`/`app.js`, AlwaysData) plus propre~~ | — | **fait** le 12 oct. 2026 (`443c749`, `c65a514`), **en ligne** (publié par `deploy_cloud.py publier`, empreintes identiques à Git) et validé par le propriétaire : l'accueil n'affiche que les modules ; bouton « Mon compte » dans l'en-tête, qui ouvre un **menu d'options** (Rattacher un module, Mot de passe), chaque option seule avec retour « ‹ Mon compte » (une option future = une entrée de `OPTIONS_COMPTE`) ; accueil vide → bouton qui ouvre directement le rattachement ; code de rattachement en majuscules avec tiret automatique après le 4e caractère (le serveur accepte avec ou sans tiret), Entrée valide. Sans changement serveur ni de route, IDs conservés, pas de script inline. Banc local isolé (copie du service, base `aqualook_banc_d`, Edge piloté) : 40/40 |
| P2.10 | **Idée, à décider par le propriétaire : génération de scripts par IA** (12 oct. 2026). L'utilisateur décrit en français ce que doit faire le script ; le serveur AlwaysData (jamais l'ESP32) appelle un modèle de langage (clé API dans le `.env` du serveur, modèle, clé et plafonds par utilisateur/jour réglables sans recompilation via `settings.php` + console) avec la grammaire du langage et le contexte du module (zones, entrées, phrases) ; le texte reçu est compilé par `script-lang.js` (erreur renvoyée au modèle, 2 essais max), dessiné en schéma, puis **relu et essayé à blanc par l'utilisateur avant tout « Enregistrer »** (rien n'est enregistré ni lancé automatiquement). Espace en ligne (`/editeur/?module=`) d'abord. Lots : 1) consigne + jeu d'essai (~30 phrases → scripts attendus) et banc Python hors ligne, qui décide de la suite ; 2) point d'entrée PHP (session, plafonds, journal) ; 3) interface de l'éditeur. Points d'attention : données envoyées au fournisseur (noms de zones, texte saisi) à signaler dans l'aide ; risque principal = script valide mais faux sur le sens (mauvaise zone), d'où la relecture obligatoire. Alternative sans IA : assistant à modèles de scripts | agent (+ décision propriétaire) |

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
