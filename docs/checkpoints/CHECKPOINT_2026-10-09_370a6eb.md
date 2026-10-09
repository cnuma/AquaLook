# Checkpoint AquaLook — 9 octobre 2026 — D016 : mails, identifiant matériel, code PIN du LCD

Document de reprise autonome. Avec `AGENTS.md` et `docs/REPRISE_INGENIEUR.md`,
il suffit pour reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-08_bc9e568.md`, dont la TODO §8 est reprise ici (§8).

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public) |
| Branche | `main` ; branches de travail de la session fusionnées (fast-forward) et poussées : `docs/d016-enrolement-securite`, `feature/mails-alwaysdata`, `feature/hw-id-module`, `feature/pin-lcd` |
| Commit fonctionnel | `370a6eb` (firmware flashé) ; `main` = `6064dfd` avant ce document |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-09_370a6eb.md`) |
| Version fonctionnelle (`VERSION`) | `5.11.0-dev` (inchangée ; proposition : `5.12.0` à la fin du lot F) |
| Firmware sur `.141` | `5.11.0-dev`, build **1317**, `370a6eb`, branche de build `feature/pin-lcd` (contenu identique à `main`) |
| Identifiant matériel `.141` | `aql-44bd8d7acb88` |
| Dernier numéro de réponse du chat source | `AQL-R019` |
| Cible | `ProgrammeArrosage_s3`, module de test `192.168.1.141`, **COM4** (confirmé le 9 oct. 2026, à reconfirmer) |
| Matériel `.141` | ESP32-S3 N4R8 : 4 Mo de flash, 8 Mo de PSRAM, relais réels (5 zones câblées) |

## 2. Source de vérité

`main` au commit de ce document. Décision de référence : **D016**
(`docs/codex/02_DECISIONS.md`) et sa conception détaillée
`docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md` (lots A à G, état de
chaque lot, analyse des mails AlwaysData). Les nouveaux travaux repartent de
`main` sur une branche dédiée.

## 3. Ce qui a été fait (9 oct. 2026)

| Commit(s) | Contenu | Validation |
|---|---|---|
| — | Test du chemin « noms inchangés » (`bc9e568`, TODO §8.1 précédente) | `.141` : `[GVAR] noms inchanges`, pas de `miroir`, `valeurs remontees (crc 915c63aa)`, `config inchangee, non renvoyee` |
| `53049a9`, `2a40441`, `6957ba4`, `33ca946`, `b36c84a`, `6064dfd` | **D016** : enrôlement par code court, identité matérielle, comptes, mails, PIN LCD, session Web locale ; lots A..G ; suivi de l'état | — |
| `e6a7833` | TODO : erreur SD `health_check_failed` | — |
| `5a5fcf9` | **Lot A** (serveur) : `settings.php` (paramètres en base, `.env` en repli), `mail.php` (SMTP TLS + AUTH LOGIN, plafonds avant envoi, `mail_log`), routes `/admin/settings`, `/admin/mail-test`, `/admin/mail-log`, bouton « Paramètres » de la console, `schema-v4`, `.htaccess` bloque tous les `schema*.sql` | banc local 27/27 + refus après DATA + transparence des points ; **production : `/health` `mail: true`, mail reçu** |
| `0fbe4ec`, `482ce28`, `d2891f9` | **Lot C** : `DeviceIdentity::hwId()` (`aql-` + MAC eFuse) dans la bannière, la ligne de démarrage du journal, `/api/diagnostics` (`build.hwId`) et le rapport `diag` ; serveur `schema-v5` (`hw_id` unique, `hw_id_conflict`), `bind_module_hw_id()`, console | banc serveur 8/8 ; **bout en bout : console « Identifiant matériel aql-44bd8d7acb88 », sans conflit** |
| `290da2b` | **Lot E** stockage : `PinLock` (NVS `aqlsec`, PBKDF2-HMAC-SHA256 4000 itérations, sel 16 o, CRC, essais : 5 libres puis 30 s doublées, plafond 15 min, compteur persisté) | `[SEC] PIN: aucun (espace aqlsec vide)` au 1er démarrage |
| `d1924ad`, `ae16253`, `f9bf4e0`, `c5d8f8a` | **Lot E** interface : portes `requestAdmin()` / `requestStart()`, arrêt toujours libre, écran `Screen::PIN`, page ADMIN « Sécurité » (9/9), garde de toucher, déverrouillage sans lancement, pavé paysage à touches rondes, cadenas du bandeau, geste d'effacement au démarrage | testé au doigt par le propriétaire (pose, verrou, échecs, déverrouillage, arrêt libre) ; ajustements demandés intégrés |
| `ba188b3` | TODO (propriétaire, autre chat) : volets repliables de l'éditeur sur tablette | — |
| `e60ddf9`, `913c835`, `370a6eb` | Registre de soak (`tools/soak/ledger.json`) | — |

## 4. Fichiers et fonctions modifiés

| Fichier | Éléments |
|---|---|
| `src/DeviceIdentity.h` (nouveau, en-tête seul) | `DeviceIdentity::hwId()` |
| `src/PinLock.h/.cpp` (nouveaux) | `begin`, `hasPin`, `isUnlocked`, `verify`, `set`, `remove`, `clear`, `lock`, `lockoutRemainingSec`, `failures` |
| `src/main.cpp` | `PinLock::begin()` avant `displayMgr.begin()` |
| `src/EventLog.h` | bannière série `Identifiant`, ligne de démarrage `hw=` |
| `src/SystemDiagnostics.cpp` | `build.hwId` |
| `src/CloudSync.cpp` | `payload.hwId` du rapport `diag` |
| `src/DisplayManager.h/.cpp` | `Screen::PIN`, `PinPurpose`, `AdminPage::SECURITE`, `requestAdmin`, `requestStart`, `openPin`, `drawPinFull`, `drawPinEntry`, `handleTouchPin`, `pinValidate`, `pinRecoveryGesture`, `drawAdminPageSecurite`, `armTouchGuard` (garde dans `handleTouch`), `lockIconState`, `renderLockIcon` (appelé par `renderCloudSprite`), verrou à la mise en veille (`update`), sites de démarrage manuel et d'entrée ADMIN remplacés |
| `cloud/php-mutualized/settings.php`, `mail.php`, `schema-v4-parametres-mails.sql`, `schema-v5-identifiant-materiel.sql` (nouveaux) | voir §3 |
| `cloud/php-mutualized/index.php`, `db.php`, `admin.html`, `cleanup.php`, `.htaccess`, `.env.example`, `README.md` | routes admin, `/health.config.mail`, `bind_module_hw_id`, `list_modules` en `SELECT *`, purge `mail_log`, console |
| `docs/codex/02_DECISIONS.md`, `docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md` | D016 |

Volontairement non modifiés : `littlefs/` (pas de buildfs), `data/` (pages
Web du module), format NVS `ALOK`, routes et IDs existants, `VERSION`.

## 5. Invariants préservés

- **Sécurité relais** : l'arrêt d'une zone ne passe par aucune porte ; aucune
  action automatique après le PIN (rien n'est lancé) ; durée maximale
  intacte ; le geste d'effacement ne touche aucun relais et n'est actif que
  si un PIN est posé et l'écran déjà touché (≤ 10 s, borné).
- **Sans PIN, comportement identique à avant le lot E.**
- Persistance : PIN dans `aqlsec` (hors `ALOK`, pas de `configRevision`) ;
  `resetConfig` ne l'efface pas ; bloc illisible = pas de PIN, jamais d'échec.
- I4 (pas de `fillScreen` hors redraw complet), I26 (marche manuelle → HOME
  quand le PIN n'intervient pas), F5/F6 (routes et IDs conservés).
- Paramètres du service modifiables sans recompilation ni redéploiement
  (demande du propriétaire) ; aucun secret dans Git.
- Identité de build vérifiée sur `.141` : `5.11.0-dev`, 1317, `370a6eb`.

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (RAM 26,0 %, Flash 77,6 %) |
| buildfs | non requis (`littlefs/` inchangé) |
| `.141` | flashé `370a6eb` (COM4) ; démarrage complet ; `[SEC] PIN: pose (6 chiffres)` ; CloudSync `rapport=ok` |
| AlwaysData | `mail.php`, `settings.php`, `index.php`, `db.php`, `admin.html`, `cleanup.php`, `.htaccess` publiés ; `schema-v4` et `schema-v5` importés ; réglages SMTP saisis dans la console (`aqualook@alwaysdata.net`, `smtp-aqualook.alwaysdata.net:465`) |
| Web module (`data/`) | inchangé ; paquet `5.10.4` toujours non préparé |
| LCD | pavé PIN, page Sécurité, cadenas : S3 seulement validé ; CYD non testée (abandon prévu) |

## 7. Risques et limites

- **Délivrabilité des mails** : l'essai est arrivé dans les **indésirables
  Gmail**, alors que SPF et DKIM passent et sont alignés (« envoyé par /
  signé par alwaysdata.net »). Facteurs : réputation partagée
  d'`alwaysdata.net`, premier envoi court. Piste : domaine d'expédition
  propre (réglable depuis la console, sans code). **À trancher avant le lot B.**
- Le mot de passe SMTP saisi dans la console est stocké en clair en base
  (`app_setting`) ; l'API ne le relit jamais, une sauvegarde de base le contient.
- **Lot E non confirmé explicitement** : geste d'effacement au démarrage,
  attentes au-delà de la première (60 s, 120 s…), cadenas vu à l'écran.
- PBKDF2 4000 itérations : durée mesurée à la pose dans la ligne
  `[SEC] PIN: pose (… derivation N ms)` — non relevée dans ce chat.
- Un PIN de 4 chiffres reste faible face à un bloc NVS extrait (lecture de la
  flash) ; la limitation des essais ne protège que l'écran.
- La session Web locale n'existe pas encore (lot F) : le PIN ne protège que
  le LCD ; les routes `/api/*` restent ouvertes au LAN (hors écritures HMAC).
- D016 annonce un mot de passe Web de 10 caractères ; `ApiAuth::setSecret`
  exige déjà **12** : à aligner au lot F.
- Bannière série de démarrage invisible sur le S3 (imprimée avant la
  réouverture du port USB natif) ; la ligne `demarrage … hw=` du journal la
  remplace. La même ligne affiche `target=unsupported` pour le S3 (défaut
  d'affichage antérieur, non corrigé).
- Restent valables : risques `bc9e568` §7, `11322d5` §7 et `1534dde` §7.

## 8. TODO — reste à faire

1. **Prochain lot D016** — ordre retenu : A ✔ → C ✔ → E ✔ → **D** (enrôlement
   par code court affiché sur le LCD, protégé par le PIN ; désenrôlement,
   transfert) → B (mot de passe oublié, invitation ; après décision sur la
   délivrabilité) → F (session Web locale, retrait de `1598753`) → G
   (effacement du PIN depuis l'espace en ligne).
2. Délivrabilité des mails : décider d'un domaine d'expédition propre (§7).
3. Confirmer au doigt : geste d'effacement au démarrage, cadenas, attentes
   successives ; relever la durée de dérivation PBKDF2.
4. Paquet Web `5.10.4` (`python tools/publish_web_assets.py 5.10.4
   --comparer`, fichiers puis manifeste, `check_published_web.py`).
5. Documentation codex périmée : `docs/codex/00_CONTEXT.md`,
   `05_BUILD_AND_TEST.md`, `06_ANTI_REGRESSION.md`, `10_TASK_HANDOFF.md`
   (encore Legacy/V4 CYD ou branche OTA de juillet), `docs/REPRISE_INGENIEUR.md`
   §1.1 (`feat/moteur-de-regles`, archivée).
6. `script-lang.js` : test des variables locales `a..h`
   (`'abcdefgh'.indexOf(mot)` accepte `ab`, `gh`…).
7. Reste de la TODO `fade726` §8 (jeton admin de production, CloudSync 8 s
   sur plusieurs heures, CI, migration Ubuntu 26 le 19 oct.).
8. **Erreur SD à examiner** : `/api/faults` sur `.141` rendait
   `unacknowledged: true`, `lastErrorMessage` « Stockage: SD indisponible
   raison=health_check_failed chemin=/www/index.html » ; épisode isolé vers
   17:46:30 le 9 oct., `SD recuperee essai=1 lentes=0 indisponible=2s`. À
   examiner : déclencheur du contrôle de santé sur `/www/index.html`,
   activité concurrente, pertinence d'un « non acquitté » pour une erreur
   récupérée en 2 s. Capture série au prochain épisode.
9. **Éditeur de scripts sur tablette** : volets latéraux repliables
   (`aside.palette`, `aside.props-panel`) dans `data/scripts-schema.html`,
   même triangle que `#b-replier` (classe `.replie`, préférence mémorisée) ;
   recopier dans `/editeur/` sur AlwaysData.
10. Version : proposer `5.12.0` à la fin du lot F (palier sécurité).

## 9. Procédure exacte de reprise

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -3
git status --short
git checkout -b <feature/...>
```

Demander le port COM courant, puis, pour un changement firmware :

```powershell
git -c core.whitespace=cr-at-eol diff --check
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

Contrôles rapides sans secret : `GET /api/diagnostics` (`build`, `build.hwId`),
`GET /api/logs.txt` (lignes `demarrage … hw=`, `[SEC]`, `[GVAR]`,
`CloudSync: cycle`), `GET https://aqualook.alwaysdata.net/health`
(`config.mail`). La bannière série étant perdue sur le S3, pour une capture
de démarrage : `esptool … --after hard_reset chip_id` puis le moniteur aussitôt.

Serveur PHP en local : MariaDB portable `C:\Users\Emman\mariadb-local`
(port 3307), `php -d extension=openssl -S 127.0.0.1:8001 router.php`
(l'extension `openssl` n'est pas chargée par défaut dans le PHP du poste).
Migration SQL dans phpMyAdmin : **sélectionner la base d'abord** (sinon
`#1046 No database selected`).

Pièges : `git diff --check` simple signale à tort les CR des fichiers CRLF
(utiliser `-c core.whitespace=cr-at-eol`) ; les heredocs bash mangent les
antislashs (passer par l'outil d'édition ou `chr(92)`).

## 10. Commandes Git utiles

```powershell
git log --oneline 11b3f46..HEAD        # toute la session du 9 octobre
git show 5a5fcf9                       # lot A : paramètres et mails
git show 482ce28                       # lot C : liaison hw_id serveur
git show 290da2b                       # lot E : PinLock
git show ae16253                       # garde de toucher, déverrouillage sans lancement
git tag -l 'archive/*'                 # branches archivées
```
