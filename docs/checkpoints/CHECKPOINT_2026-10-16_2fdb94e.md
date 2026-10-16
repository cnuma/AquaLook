# Checkpoint AquaLook — 16 octobre 2026 (soir) — 5.17.10 : interface pilotée par le CSS (P4.11), D022

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-16_84e3aba.md`.

> **En un coup d'œil :** plus aucun `<style>` interne ni attribut `style=`
> fixe dans les pages du module et du site : l'aspect vient de feuilles
> communes (P4.11, 4 lots). Décision D022 sur l'accès physique (P1.6, choix A).
> Version **5.17.10** : site, paquet Web et `/editeur/` publiés ; `.141`
> flashé et mis à jour. Suite : voir `docs/TODO.md` (P3.25 badge de synchro
> sur téléphone, P4.12, P0.5 + verrouillage matériel D022…).

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` ; `feature/p411-css-commun`, `docs/todo-p325-badge-synchro`, `docs/d022-acces-physique` fusionnées `--no-ff` puis supprimées |
| Commit fonctionnel | **`2fdb94e`** (firmware de `.141`, serveur AlwaysData ALIGNE) ; après lui, documentation seule |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-16_2fdb94e.md`) |
| Version (`VERSION`) | **`5.17.10`** ; pas de tag |
| Release GitHub publique (latest) | `v5.17.2`, signée (inchangée) |
| Firmware sur `.141` | `5.17.10`, build **1638**, `2fdb94e`, env `ProgrammeArrosage_s3_banc` (`devRoutes=true`), santé normale |
| Pages Web sur `.141` | paquet **5.17.10** (30 fichiers), installé par `POST /api/webassets/update` |
| Paquet Web en ligne | **5.17.10**, signé, `check_published_web.py` 30/30, manifeste 7 006 o |
| Service AlwaysData (PHP + pages du site) | `deploy_cloud.py` : ALIGNE sur `2fdb94e` |
| `/editeur/` sur AlwaysData | **8 fichiers** (nouveau `style-editeur.css`), identiques à `data/` |
| Nom local de `.141` | `jardin-03` → `http://jardin-03.local`, port **COM4** (confirmé dans cette session, à reconfirmer) |
| Dernier numéro de réponse du chat source | `AQL-R005` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul. **Reste à faire : `docs/TODO.md`**.
Nouvelle décision **D022** (`docs/codex/02_DECISIONS.md`) ; registre des
risques mis à jour (SEC-028).

## 3. Ce qui a été fait

### P4.11 — interface pilotée par le CSS

| Lot | Commit | Contenu |
|---|---|---|
| 1 Site | `84461c3` | `cloud/php-mutualized/site.css`, feuille unique ; `accueil`, `maj`, `app`, `admin` sans `<style>`, classe `p-<page>` sur `<body>` ; couleurs = noms et valeurs de `style-base.css` ; console sombre par redéfinition des variables (`body.p-console`) ; `deploy_cloud.py` publie les `.css` en premier |
| 2 Module | `fa86da1`, `0caf089` | `sante`, `cloudsync`, `logs`, `urls`, `diagnostic` → `data/style-statut.css` (règle partagée = liste explicite des pages, pas de fuite) ; `ota`, `setup` → `data/style-sombre.css` (couleurs des pages du firmware) ; `index` → fin de `style.css` ; variables `--ok-text`, `--warn-text`, `--ok-bg`, `--warn-bg`, `--on-amber` dans `style-base.css` |
| 3 Éditeur | `d5f9d29` | `<style>` de `scripts-schema.html` → `data/style-editeur.css` tel quel ; AGENTS.md : `/editeur/` = 8 fichiers |
| 4 Accueil | `13cc0dd` | 114 attributs `style=` d'`index.html` → 34 classes `ix-*` en fin de `style.css` (`!important` = priorité de l'attribut) ; `#btn-zones-view` sans `!important` (recoloré par `app.js`) ; 5 `display:none` basculés par le JS restent en ligne |

**Pourquoi des feuilles séparées** : le module sert ses fichiers sans cache
(`no-store`) ; mesuré sur `.141`, chaque Ko de `style-base.css` coûte ~8 ms à
**chaque** page (51 Ko en 430 ms). `style-base.css` n'a donc pris que 7 lignes.

### P1.6 — D022 (choix A du propriétaire)

Wi-Fi invité ou dédié et boîtier sans accès USB dès maintenant (propriétaire) ;
Secure Boot v2 + chiffrement de la flash + USB sécurisé obligatoires avant
toute mise en service chez un tiers, avec P0.5 ; empreinte PBKDF2 du mot de
passe Web en secondaire (la clé conservée ouvrirait encore une session : gain
limité à ne pas révéler le mot de passe lui-même). P1.6 reste ouvert.

### Autres

- P3.25 ajouté au TODO (badge de synchro trop imposant sur téléphone : icône seule).
- P4.12 ajouté (couleurs en dur restantes, doublons, limite du manifeste).

## 4. Essais

| Profil / cible | Port | Essai | Attendu | Observé |
|---|---|---|---|---|
| Site (copie locale) | — | captures Edge avant/après, PC et téléphone, + page témoin des vues connectées | même mise en page | identique ; teintes alignées sur le module |
| Pages du module via relais en lecture seule vers `.141` | — | captures avant/après des 8 pages | identique | pixels identiques `sante`, `urls`, `ota`, toutes vues téléphone ; écarts = données en direct ; `diagnostic` : couleurs d'état voulues |
| `index.html` via relais | — | styles calculés de tous les éléments, après rendu `app.js` | identiques | 766/766 identiques (PC et téléphone) |
| Banc CSP `tools/csp_banc_pages.py` | — | toutes les pages + témoin négatif | tout passe | tout passe (avant et après installation du paquet) |
| Banc éditeur `tools/editeur_banc.py` | — | 4 scénarios | 77/77 | 77/77 |
| `ProgrammeArrosage_s3` | — | compilation production | SUCCESS | SUCCESS (Flash 80,3 %, RAM 27,1 %), non flashé |
| `ProgrammeArrosage_s3_banc` | COM4 | flash + identité | 5.17.10 / 1638 / `2fdb94e` | conforme, santé normale |
| AlwaysData | — | `deploy_cloud.py publier` | ALIGNE | ALIGNE `2fdb94e` ; `site.css` 200 `text/css` ; `/admin.html` 401 ; `web_security_probe.py` 32 PASS ; capture `app.html` en ligne correcte |
| Paquet Web | — | FTPS : 30 fichiers relus en HTTPS → `.sig` → manifeste | 30/30 | 30/30 ; `check_published_web.py` 30/30 |
| `.141` | — | `POST /api/webassets/update` | `deploye 5.17.10 30/30` | conforme ; 30/30 servis identiques à `data/` |
| `/editeur/` | — | envoi de 3 fichiers, relecture des 8 | 8/8 | 8/8 ; anciens fichiers sauvegardés dans `C:\Users\Emman\.aqualook\sauvegardes\editeur_2026-10-16` |

**Non essayé :** vues connectées de l'espace en ligne avec un vrai compte
(vérifiées sur page témoin locale seulement) ; console d'administration
connectée ; `setup.html` en vrai mode portail captif ; validation à l'œil
par le propriétaire.

## 5. Fichiers modifiés

| Zone | Fichiers |
|---|---|
| Site | `cloud/php-mutualized/site.css` (nouveau), `accueil.html`, `maj.html`, `app.html`, `admin.html`, `app.js`, `admin.js` (`var(--muted)` → `var(--text2)`) |
| Module | `data/style-statut.css`, `data/style-sombre.css`, `data/style-editeur.css` (nouveaux), `style-base.css` (variables), `style.css`, `index.html`, `sante.html`, `cloudsync.html`, `logs.html`, `urls.html`, `diagnostic.html`, `ota.html`, `setup.html`, `scripts-schema.html` |
| Outils | `tools/deploy_cloud.py` (`.css` en premier), `tools/soak/ledger.json` |
| Docs | `AGENTS.md` (8 fichiers de `/editeur/`), `docs/codex/02_DECISIONS.md` (D022), `docs/security/SECURITY_RISK_REGISTER.md` (SEC-028), `docs/TODO.md`, ce checkpoint |
| Version | `VERSION` = 5.17.10 |

Volontairement non modifiés : firmware C++ (dont les 3 pages intégrées,
sombres par décision du propriétaire), `littlefs/`, NVS, routes, IDs HTML, JS
(hors 2 noms de variable CSS), relais.

## 6. Invariants préservés

- F5/F6 : IDs HTML et routes inchangés.
- Pages Web : aucun script inline, aucun attribut `on*=`, aucune ressource externe (banc CSP).
- F14 : version de source unique (`VERSION`).
- I1/I2 : `littlefs/` inchangé ; buildfs non requis.
- Publication : fichiers d'abord, manifeste en dernier ; site depuis Git uniquement.

## 7. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS sur `2fdb94e` |
| Banc `ProgrammeArrosage_s3_banc` | flashé sur COM4, `2fdb94e`, build 1638 |
| buildfs | non requis (`littlefs/` inchangé) |
| Web | 5.17.10 en ligne et sur `.141` |
| Service AlwaysData | ALIGNE sur `2fdb94e` |
| LCD / matériel | aucun changement ; santé normale |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture du 11 oct.) |

## 8. Risques et limites

- Le manifeste Web fait 7 006 o pour une limite firmware de 8 192 o : environ 5 fichiers de plus au maximum (P4.12).
- `site.css` n'a pas de paramètre de version : après une future modification, un navigateur peut garder l'ancienne feuille un moment (cache heuristique d'Apache).
- Les classes `ix-*` d'`index.html` sont en `!important` : une future règle de thème sur ces éléments doit modifier la classe elle-même.
- `.141` sur le Wi-Fi principal tant que le propriétaire ne l'a pas déplacé (D022).
- Git Bash convertit les arguments commençant par `/` : `MSYS_NO_PATHCONV=1`. Une capture série tient COM4.

## 9. Procédure exacte de reprise

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -6
git status --short
git -c core.whitespace=cr-at-eol diff --check
python tools/deploy_cloud.py verifier
python tools/check_published_web.py https://aqualook.alwaysdata.net/aqualook-web-manifest.json
```

Attendu : « Serveur ALIGNE », 30/30 (5.17.10). `.141` : `/api/diagnostics`
→ `build.version=5.17.10`, `number=1638`, `gitSha=2fdb94e`,
`env=ProgrammeArrosage_s3_banc` ; `/api/health` → `overall=normal`.

Firmware (port COM à reconfirmer) :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3_banc -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent 6742c58..main
git show 84461c3 --stat     # lot 1 site
git show 13cc0dd --stat     # lot 4 index
git show 48fd9f1            # D022
```
