# Checkpoint AquaLook — 12 octobre 2026 — sécurité du service en ligne (P0.4c à P0.4f), référentiel vivant

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-11_47f3306.md`.

> **En un coup d'œil :** tout le chantier sécurité du service en ligne
> (AlwaysData) est **publié et vérifié** : CSP stricte sur tout le site,
> mot de passe SMTP chiffré, console derrière une authentification HTTP
> Basic, site `noindex`, page d'accueil neutre, route `/admin` retirée,
> fichiers PHP internes refusés, sessions de l'espace utilisateur limitées
> (case « Rester connecté » 7 jours). L'éditeur de scripts n'a plus de
> script inline et gagne un bouton « Réglages généraux ». Référentiel de
> sécurité documenté comme **règles vivantes** (D017). Firmware inchangé
> (5.13.2).

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` (toutes les branches de la session fusionnées `--no-ff` puis supprimées, localement et sur GitHub) |
| Commit fonctionnel | **`8a84ecb`** |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-12_8a84ecb.md`) |
| Version fonctionnelle (`VERSION`) | **`5.13.2`**, inchangée (aucun changement firmware) ; pas de tag |
| Firmware sur `.141` | `ad75d36`, build 1390, `5.13.2` (non reflashé) |
| SD de `.141` | `scripts-schema.html` + `script-editeur.js` de `main` déposés (empreintes vérifiées) |
| AlwaysData | **aligné sur `main`** pour tous les fichiers publics vérifiables (empreintes comparées) ; PHP vérifiés par leur comportement |
| Dernier numéro de réponse du chat source | `AQL-R022` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul, `ProgrammeArrosage_s3` seule cible
firmware. **Reste à faire : `docs/TODO.md`** (fait foi). Sécurité :
`docs/security/CYBERSECURITY_ARCHITECTURE.md` (§1.1 règles vivantes, §4.8
mesures du service en ligne) et `docs/security/SECURITY_RISK_REGISTER.md`.

## 3. Ce qui a été fait (commits de fusion sur `main`)

| Commit | Sujet | Validation |
|---|---|---|
| `43fb156` | TODO : 3 variables FTP obligatoires (+ `DIR` facultative) ; en-tête de `REPRISE_INGENIEUR.md` à jour (renvoi à `VERSION`) | doc |
| `0a09302` | **P0.4c** : secrets du registre `settings.php` chiffrés en XChaCha20-Poly1305 (libsodium), clé `SETTINGS_KEY` (`.env` du serveur), nom du paramètre en donnée associée, format `enc1:` ; ancienne valeur claire relue puis rechiffrée seule ; sans clé, secret refusé ; clé changée → « illisible » ; 128 octets max ; état affiché dans la console | banc PHP + MariaDB 25/25 ; production : « défini (chiffré) » confirmé |
| `039c583` | **Référentiel vivant (D017)** : `CYBERSECURITY_ARCHITECTURE.md` §1.1 (revue à chaque évolution, chaque faille publiée, chaque trimestre ; prochaine revue au plus tard le 12 janv. 2027) et §4.8 ; registre SEC-020 à SEC-023, SEC-001 réduit ; `04_DEVELOPMENT_RULES.md` section Sécurité ; `AGENTS.md` règle « Cybersécurité : référentiel vivant » | doc |
| `901113c` | TODO : idée de génération de scripts par IA (commit poussé depuis une autre session, intégré tel quel) | — |
| `df5498f` | **P0.4d** : script inline de `data/scripts-schema.html` sorti à l'octet près dans `data/script-editeur.js` ; bouton **« Réglages généraux »** dans le bandeau du haut (retour à la vue générale du panneau) ; partie « Mot de passe du module » expliquée (« Changer le mot de passe… », « Effacer du navigateur ») et masquée dans l'éditeur en ligne | `editeur_banc.py` 77/77 ; banc CSP stricte 0 violation + témoin négatif ; SD `.141` ; validé par le propriétaire |
| `0540e06` | **P0.4e** : console (`admin.html`, `admin.js`) réécrite vers `index.php` → `console_page.php`, authentification HTTP Basic (`ADMIN_PAGE_USER`, `ADMIN_PAGE_HASH` = `password_hash`), 503 si non configurée ; `robots.txt` + `X-Robots-Tag` ; **CSP générale sans `'unsafe-inline'`** (le `<FilesMatch>` de P0.4b retiré) | banc PHP + Edge (défi 401 puis 200) ; production vérifiée |
| `275d8d7` | PHP internes (`account|auth|console_page|db|enroll|env|mail|settings.php`) refusés en accès direct ; `web_security_probe.py` étendu (console 401, `X-Robots-Tag`, `script-src` sans `'unsafe-inline'` sur l'espace utilisateur et l'éditeur) ; **page d'accueil neutre** `accueil.html` servie sur `/` | banc PHP ; production : 403, accueil 200 |
| `b4c0d65` | Route `GET /admin` (redirection vers la console) **retirée** sur décision du propriétaire (D017) : `/admin` → 404 | banc ; production 404 |
| `76b4db0` | **P0.4f** : sessions de l'espace utilisateur — case « Rester connecté » : 7 jours fixes ; sinon cookie de navigateur, 2 h d'inactivité, 12 h max ; plafond appliqué aux anciennes sessions de 30 jours ; réglages `SESSION_REMEMBER_DAYS`, `SESSION_IDLE_MIN`, `SESSION_MAX_H` (console) ; `GET /app/login-options` public ; sans migration (type déduit de la durée, bornes 24 h / 2 jours) | banc PHP + MariaDB 14/14 ; écran Edge ; production : case « 7 jours » affichée, validé par le propriétaire |
| `8a84ecb` | Suivi de publication dans la TODO et le référentiel | doc |

### Publication (faite par le propriétaire, FTP manuel, 12 oct. 2026)

`.env` du serveur complété avec `ADMIN_PAGE_USER`, `ADMIN_PAGE_HASH`,
`SETTINGS_KEY` (valeurs jamais passées par le chat ; copies chez le
propriétaire). Fichiers publiés : `admin.*`, `app.*`, `settings.php`,
`console_page.php`, `robots.txt`, `accueil.html`, `auth.php`, `index.php`,
`.htaccess` ; `/editeur/` recopié (`scripts-schema.html`,
`script-editeur.js`, `session.js`, `script-schema.js`, `script-lang.js`,
`hmac.js`, `style-base.css`). `tools/deploy_cloud.py` **pas encore
utilisé** (identifiants FTP absents du `.env` local, P0.4a).

### Vérification de l'extérieur (fin de session)

Empreintes `accueil.html`, `app.html`, `app.js`, `robots.txt`, fichiers de
`/editeur/` = Git ; `/` 200 ; `/admin` 404 ; `/admin.html` et `/admin.js`
401 (realm « AquaLook console ») ; PHP internes et `.env` 403 ; `/health`
`ok` ; `/app/login-options` `{"rememberDays":7}` ; connexion bidon 401 sans
cookie ; CSP `script-src 'self'`, `X-Robots-Tag`, HSTS, `X-Frame-Options`
présents ; `web_security_probe.py` lancé par le propriétaire : tout PASS.

## 4. Fichiers et fonctions modifiés

| Fichier | Élément |
|---|---|
| `cloud/php-mutualized/settings.php` | `settings_key`, `settings_encrypt`, `settings_decrypt`, `settings_secret_state`, `settings_migrate_plain`, `setting_with_source`, `settings_list` (`stored`), `settings_save` ; registre `SESSION_*` |
| `cloud/php-mutualized/console_page.php` | nouveau : `console_basic_credentials`, `serve_console_page` |
| `cloud/php-mutualized/auth.php` | `session_short_minutes`, `open_session($remember)`, `current_user` (plafond, glissement), `set_session_cookie($remember)` |
| `cloud/php-mutualized/index.php` | routes console, `/` (accueil), `/app/login` (`remember`), `/app/login-options` ; `GET /admin` retirée |
| `cloud/php-mutualized/.htaccess` | réécriture de la console, CSP générale stricte, `X-Robots-Tag`, refus des PHP internes |
| `cloud/php-mutualized/router.php` | mêmes réécritures et refus pour le banc local |
| `cloud/php-mutualized/accueil.html`, `robots.txt` | nouveaux |
| `cloud/php-mutualized/admin.js` | `STOCKAGE` (état du secret) |
| `cloud/php-mutualized/app.html`, `app.js` | case « Rester connecté », libellé depuis `/app/login-options` |
| `cloud/php-mutualized/.env.example`, `README.md`, `schema-v4-parametres-mails.sql` (commentaire) | documentation |
| `data/scripts-schema.html`, `data/script-editeur.js` | script externalisé ; `renderProps` (vue générale), `secretRetenu`, `oublierSecret`, bouton `b-reglages` |
| `tools/editeur_banc/verifications*.js` | contrôles du panneau et du mot de passe |
| `tools/web_security_probe.py` | contrôles console, `/admin`, `X-Robots-Tag`, `script-src` |
| `AGENTS.md`, `docs/codex/02_DECISIONS.md` (D017), `docs/codex/04_DEVELOPMENT_RULES.md`, `docs/security/*`, `docs/TODO.md`, `docs/REPRISE_INGENIEUR.md` | documentation |

Volontairement non modifiés : firmware (`src/`, `include/`, `lib/`),
`littlefs/`, `platformio.ini`, `VERSION`, schéma SQL (aucune migration).

## 5. Invariants préservés

- Aucun changement firmware, relais, planificateur, NVS, LittleFS.
- Routes : seule `GET /admin` retirée (décision du propriétaire, D017) ;
  `/admin/*`, `/app/*`, `/v1/*` inchangées ; ajout de `GET /` (accueil) et
  `GET /app/login-options`. IDs HTML existants conservés.
- Aucun secret dans le dépôt ni dans le chat (`.env` local lu sans
  affichage ; empreinte, clé et identifiants restés chez le propriétaire).
- Paramètres de service réglables sans recompilation (registre
  `settings.php`).

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | non requise (aucun fichier firmware touché depuis `5b31083`) ; dernier build 1390 SUCCESS |
| buildfs | non requis (`littlefs/` inchangé) |
| Web module | éditeur déposé sur la SD de `.141`, validé par le propriétaire |
| LCD / matériel | inchangés, aucun relais sollicité |
| Service en ligne | publié et vérifié (§3) |
| `git diff --check` (`cr-at-eol`) | propre |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (`Capture d'écran 2026-10-11 161236.png`, au propriétaire) |

## 7. Risques et limites

- `.htaccess` jamais lue par un Apache local : chaque publication se
  vérifie aussitôt (en-têtes, absence de 500). Ancienne version dans Git
  pour revenir en arrière.
- Console : l'identifiant Basic est retenu par le navigateur jusqu'à sa
  fermeture complète ; « Déconnexion » n'efface que le jeton admin. Pas de
  limitation d'essais propre à l'authentification Basic (bcrypt + jeton
  admin derrière).
- Sessions courtes : un navigateur qui restaure les onglets peut garder le
  cookie de navigateur ; le serveur coupe de toute façon après 2 h
  d'inactivité / 12 h.
- `SETTINGS_KEY` perdue ou changée : ressaisir le mot de passe SMTP.
- Les copies des pages du module sous `/web/` gardent du script inline :
  inertes si ouvertes sur le site (elles sont destinées aux modules).
- Paquet Web des modules (`publish_web_assets.py`) **pas publié** : les
  modules ne reçoivent pas encore le nouvel éditeur par mise à jour Web
  (reste de P2.1).
- `deploy_cloud.py` jamais exécuté en production (P0.4a).

## 8. TODO

Voir **`docs/TODO.md`**. Prochaines étapes :

1. **P0.4a (propriétaire)** : ajouter `AQUALOOK_FTP_HOST`, `_USER`,
   `_PASSWORD` au `.env` racine, puis `python tools/deploy_cloud.py
   verifier` (lecture seule) pour prouver l'alignement serveur ↔ Git, PHP
   compris.
2. **P2.1** : publier le paquet Web des modules (`publish_web_assets.py`,
   version à fixer, fichiers puis manifeste, `check_published_web.py`).
3. **P2.9** : espace utilisateur plus propre (onglets Modules / Rattacher /
   Mon compte).
4. **P0.5** HTTPS LAN (étude), P2.8 notifications par mail.
5. P1 : Wi-Fi mesh, point 2 vers le 14 oct., migration Ubuntu 26 le 19 oct.
6. Revue périodique du référentiel de sécurité au plus tard le 12 janv. 2027.

## 9. Procédure exacte de reprise

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -4
git status --short
git -c core.whitespace=cr-at-eol diff --check
```

Rien à reflasher. Banc cloud local : MariaDB portable sur 3307, puis dans
`cloud/php-mutualized` : `$env:DB_NAME='aqualook_banc_d'; php -d
extension=sodium -S 127.0.0.1:8001 router.php` (la console exige
`ADMIN_PAGE_USER` et `ADMIN_PAGE_HASH`, en variables d'environnement ou
dans le `.env` local). Banc éditeur : `python tools/editeur_banc.py`.
Contrôle du site : `python tools/web_security_probe.py`. Pour un
développement firmware, demander le port COM, puis :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent 5b31083..main   # cette session
git show 0a09302 --stat                         # P0.4c
git show df5498f --stat                         # P0.4d + éditeur
git show 0540e06 --stat                         # P0.4e
git show 76b4db0 --stat                         # P0.4f sessions
git show 039c583 -- AGENTS.md                   # règle référentiel vivant
```
