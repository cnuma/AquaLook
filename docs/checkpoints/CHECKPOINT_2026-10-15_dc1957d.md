# Checkpoint AquaLook — 15 octobre 2026 — P1.6 accès physique, P3.2, P3.9, P3.13, P3.18

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-14_29fe2d1.md`.

> **En un coup d'œil :** journée de petits défauts et de sécurité, tous
> validés sur `.141` et fusionnés. P1.6 reformulé en risque d'accès physique
> (SEC-028, décision Secure Boot / chiffrement de flash **reportée par le
> propriétaire**). Un épisode SD isolé ne laisse plus d'alarme (P3.2), le
> code d'erreur de la carte est gardé ; un binaire non commité porte
> `+modif` dans son SHA (P3.9) ; plus aucun script inline dans le firmware,
> CSP envoyée sur ses pages (P3.13) ; la page Santé dit « Attention » quand
> une alarme attend (P3.18). **Paquet Web à publier** (`sante.js`).

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` ; branches `docs/p16-acces-physique`, `fix/p32-sd-episode-isole`, `fix/p39-build-modifie` (dans `fix/p313-scripts-firmware`), `fix/p313-scripts-firmware`, `fix/p318-sante-attention` fusionnées `--no-ff` puis supprimées |
| Commit fonctionnel | **`dc1957d`** |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-15_dc1957d.md`) |
| Version (`VERSION`) | `5.17.4` (inchangée) ; pas de tag, pas de release |
| Release GitHub publique (latest) | `v5.17.2`, signée (inchangée) |
| Firmware sur `.141` | `5.17.4`, build 1575, `dc1957d`, env `ProgrammeArrosage_s3_banc` (`devRoutes=true`) |
| Pages Web sur `.141` | paquet `5.17.4` + `sante.js` de P3.18 déposé directement sur la SD |
| Paquet Web en ligne | `5.17.4` (signé) — **ne contient pas** le `sante.js` de P3.18 |
| Service AlwaysData (PHP) | `deploy_cloud.py verifier` : ALIGNE sur `dc1957d` |
| Nom local de `.141` | `jardin-03` → `http://jardin-03.local`, port **COM4** (confirmé dans cette session, à reconfirmer) |
| Dernier numéro de réponse du chat source | `AQL-R029` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul. **Reste à faire : `docs/TODO.md`**
(fait foi). Pas de nouvelle décision `02_DECISIONS.md` ; nouveau risque
SEC-028 au registre.

## 3. Ce qui a été fait

### 3.1 P1.6 reformulé : accès physique au module (`2a67d12`)

- Analyse : par le port USB, n'importe qui peut flasher un programme de son
  choix (pas de Secure Boot) et lire la flash (pas de chiffrement) : mot de
  passe Web (`aq_apikey`), mot de passe Wi-Fi, jeton du service en ligne. Un
  programme malveillant devient un poste du Wi-Fi de la maison et une porte
  dérobée joignable depuis Internet.
- Par le réseau, **aucun** chemin d'installation d'un programme non signé
  (revue de `OtaStageUpdate.cpp` : manifeste signé D021, GitHub seul,
  SHA-256, aucune route d'envoi direct de firmware).
- `SECURITY_RISK_REGISTER.md` : **SEC-028** + fiche §2.1 ; SEC-004 passé à
  « Réduit » (manifeste du programme signé, validé le 14 oct.).
- Décision à prendre avant mise en service (reportée par le propriétaire) :
  Secure Boot v2 + chiffrement de flash + téléchargement USB sécurisé
  (eFuses irréversibles, ESP-IDF 5 au lieu d'Arduino précompilé, liée à
  P0.5). En attendant : Wi-Fi invité ou dédié, prise USB inaccessible.

### 3.2 P3.2 — épisode SD isolé sans alarme (`580e22e`)

- Le message `health_check_failed` vient du contrôle de santé (toutes les
  2 s, `/www/index.html` n'est que le fichier témoin) : 3 lectures en erreur
  d'affilée (~4 s). Cause de l'épisode du 9 oct. introuvable (journal RAM).
- Défaut corrigé : la décision du 28 sept. (épisode isolé silencieux) était
  contournée par le `LOG_ERROR` de `markUnavailable` et
  `FaultManager::setActive`, d'où LED rouge et triangle jusqu'à
  acquittement. `FaultManager::setActive(id, active, raiseAlarm=true)` ; la
  SD l'active sans alarme ; `IncidentManager` lève l'alarme en cas de rafale
  (`Incident SD: rafale`, ERR) ; l'échec des remontages l'escalade comme
  avant.
- Code d'erreur SdFat de la dernière occurrence gardé en NVS (ancien octet
  `reserved`, toujours 0 : ni taille ni schéma changés) et exposé :
  `storageSd.lastSdError`/`lastSdErrorText`/`occurrences` dans
  `/api/health`, et dans `/api/incidents/storage-sd`.
- Commentaire faux corrigé (« seconde lecture » qui n'a jamais existé).
- `.141` (build 1559) : retrait de 10 s → épisode isolé, `code=0x0C`
  (*Read multiple blocks*), reprise seule, `unacknowledged=false` ; trois
  retraits en 2 min → rafale, `unacknowledged=true`, acquittement à
  l'écran → `false`. LED et écran confirmés par le propriétaire.
- Le propriétaire a demandé s'il fallait retarder l'affichage furtif pendant
  l'indisponibilité : **non retenu** (état réel, pages en secours pendant
  ce temps).

### 3.3 P3.9 — SHA `+modif` (`ab5ff17`)

`tools/version_build.py` suffixe le SHA de `+modif` quand `src`, `include`,
`lib`, `platformio.ini`, `VERSION`, `aqualook_partitions.csv` ou les scripts
de build sont modifiés ou nouveaux. Visible partout où le SHA s'affiche.
Validé sur `.141` (build 1564 : `sha ab5ff17+modif`).

### 3.4 P3.13 — plus de script inline dans le firmware (`443a47d`, `d2a7a2c`)

- `/login`, portail captif (`/setup`), accueil de secours, `/ota` de
  secours : scripts servis par `/fw-login.js`, `/fw-setup.js`,
  `/fw-secours.js`, `/fw-ota.js` (constantes PROGMEM, contenu inchangé ;
  `onclick` → `addEventListener`), hors du compteur de pages en vol.
- CSP envoyée sur les pages intégrées (même politique que le service en
  ligne : `script-src 'self'`, sans `'unsafe-inline'`).
- `tools/csp_banc_pages.py` couvre ces pages, lues dans `src/` :
  `fw-login`, `fw-setup`, `fw-secours`, `fw-ota` 13/13 ; contre-épreuve sur
  l'ancien code : 38 échecs. Banc complet : tout passe.
- `.141` (build 1564) : routes 200, CSP sur `/login`, `/login` rendue par
  Edge sous la CSP du module (« Non connecte. »).
- `/setup` en mode station est servi par `data/setup.html` (SD).

### 3.5 P3.18 — état général « Attention » (`ea6ded8`)

- `src/HealthOverall.h` : source unique de `/api/health.overall` et de la
  page Santé du LCD. Priorité dégradé > essai > **attention** > normal ;
  attention = alarme non acquittée, défaut actif (hors boucle de
  démarrage), incident SD actif ou à acquitter. `data/sante.js` l'affiche
  en orange.
- `.141` (build 1572, `85a893c`), retrait SD de 2 min : 18:44:47 attention
  (défaut, sans alarme) → 18:46:44 alarme après 5 remontages → 18:56:45
  carte reprise (reprise lente), attention à acquitter → 18:58:14
  acquittement → normal. Puis essai libre du propriétaire : retrait court à
  18:59:12, reprise seule (épisode isolé) → normal à 19:01:07. LCD et page
  Web confirmés à l'œil.

### 3.6 P3.19 ajouté

Envoyer aussi la CSP sur les pages servies depuis la SD (`SdStaticHandler`).

## 4. Essais non faits

- P3.13 : portail captif réel (mode AP) et pages de secours sans SD sur le
  matériel (couverts par le banc CSP).
- P3.2 : cause de l'épisode du 9 oct. (désormais, le code d'erreur sera
  gardé au prochain).

## 5. Fichiers modifiés

| Zone | Fichiers |
|---|---|
| Firmware | `src/FaultManager.{h,cpp}`, `src/IncidentManager.{h,cpp}`, `src/StorageManager.{h,cpp}`, `src/WebManager.{h,cpp}`, `src/DisplayManager.cpp`, `src/HealthOverall.h` (nouveau), `src/test_ota_https.cpp` (stub) |
| Web (SD) | `data/sante.js` |
| Outils | `tools/version_build.py`, `tools/csp_banc_pages.py`, `tools/soak/ledger.json` |
| Docs | `docs/TODO.md`, `docs/security/SECURITY_RISK_REGISTER.md`, ce checkpoint |

Volontairement non modifiés : `littlefs/`, format NVS (taille et schéma),
routes existantes, IDs HTML, relais, service PHP, `/editeur/`.

## 6. Invariants préservés

- I6/I7 : aucune commande de vanne ni durée maximale touchée.
- F5/F6 : IDs et routes existants inchangés ; routes `/fw-*.js` ajoutées ;
  valeurs `overall` existantes inchangées (`attention` ajoutée).
- F8 : enregistrement NVS d'incident de même taille et même schéma.
- F14 : SHA unique, suffixé à la source (`version_build.py`).
- Règle « aucun script inline » désormais tenue aussi par le firmware.
- D014 : aucune mise à jour déclenchée à distance.

## 7. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (P3.2 : 4 min 41 ; P3.13 : RAM 27,1 %, Flash 80,2 %) ; le binaire de banc de `dc1957d` compile (§7.1) |
| buildfs | non requis (`littlefs/` inchangé) |
| Web | `.141` : 5.17.4 + `sante.js` P3.18 ; en ligne : 5.17.4 sans lui |
| LCD / matériel | page Santé « Attention » confirmée à l'œil ; LED et triangle P3.2 confirmés |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture d'écran du 11 oct.) |

### 7.1 Firmware de `.141` en fin de session

Flashé par USB (COM4) depuis `main` = `dc1957d` : `ProgrammeArrosage_s3_banc` 5.17.4, **build 1575, `sha dc1957d`** (sans `+modif`), branche `main`, `devRoutes=true` ; binaire de banc Flash 81,3 %, RAM 27,1 %. Après redémarrage : `overall=normal`, `unacknowledged=false`, incident SD `acknowledged` (`occurrences=1173`, `lastSdError=12`) ; `assets-version` = « depot direct » (marqueur posé par le dépôt de `sante.js`). `deploy_cloud.py verifier` : Serveur ALIGNE sur `dc1957d` ; `check_published_web.py` : 27/27.

## 8. Risques et limites

- **Paquet Web à publier** : le `sante.js` de P3.18 n'est que sur la SD de
  `.141`. Un module à jour du firmware mais avec le paquet 5.17.4 affiche
  « attention » brut, sans couleur. Publication (VERSION 5.17.5,
  `publish_web_assets.py`, FTPS, `.sig`, manifeste) **sur accord du
  propriétaire**.
- Le firmware de la session (P3.2, P3.9, P3.13, P3.18) n'est pas publié en
  release : une release exige `VERSION` > 5.17.2 et la signature du
  propriétaire (`release_firmware.py`).
- Dossiers résiduels dans `.git/worktrees/` (suppression refusée par
  OneDrive, `Permission denied`) : sans effet, `git worktree list` est
  propre.
- Compilation dans OneDrive lente : P4.6.
- Git Bash réécrit `/api/...` : lancer `module_session.py` depuis PowerShell.

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
python tools/csp_banc_pages.py
```

Attendu : « Serveur ALIGNE », 27/27 + `signature-ok` (5.17.4), banc CSP
« tout passe ». `.141` : `/api/diagnostics` → `build.env=ProgrammeArrosage_s3_banc`,
`/api/health` → `overall=normal`.

Firmware (port COM à reconfirmer) :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3_banc -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent 010326f..main
git show 580e22e            # P3.2
git show ab5ff17            # P3.9
git show 443a47d            # P3.13
git show ea6ded8            # P3.18
python tools/csp_banc_pages.py fw-login fw-setup fw-secours fw-ota -v
```
