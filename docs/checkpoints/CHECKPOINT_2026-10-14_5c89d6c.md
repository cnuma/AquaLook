# Checkpoint AquaLook — 14 octobre 2026 (nuit) — 5.15.4 : routes de développement hors production, anti-rejeu des mises à jour Web

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-13_d71cd0d.md`.

> **En un coup d'œil :** version **5.15.4**, tout est aligné (dépôt, serveur
> AlwaysData, paquet Web en ligne, firmware et pages de `.141`). Deux
> chantiers fusionnés dans `main` : **P1.8** (les routes `/api/debug/*`
> n'existent plus dans le binaire de production, D020) et **P1.9 lot 2a**
> (un manifeste Web signé mais plus ancien que la version installée est
> refusé). **Changement de chaîne : `.141` se flashe désormais avec
> `ProgrammeArrosage_s3_banc`.** Clé privée de signature sauvegardée hors
> ligne par le propriétaire.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` (branches `feature/p18-debug-routes` et `feature/p19-anti-rejeu-web` fusionnées `--no-ff` puis supprimées) |
| Commit fonctionnel | **`5c89d6c`** |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-14_5c89d6c.md`) |
| Version fonctionnelle (`VERSION`) | **`5.15.4`** ; pas de tag |
| Firmware sur `.141` | `5.15.4`, build **1503**, `gitSha` `4a4cf93`, env **`ProgrammeArrosage_s3_banc`** (`devRoutes=true`) ; code identique à `5c89d6c` (les commits suivants ne touchent que la documentation et le registre de soak) |
| Pages Web sur `.141` | `5.15.4`, installées par OTA depuis le paquet signé (27/27 conformes) |
| Paquet Web en ligne | `5.15.4`, signé (clé `37c2c8068db00bd9`), `check_published_web.py` 27/27 + signature ok |
| Service AlwaysData (PHP) | `deploy_cloud.py verifier` : ALIGNE (aucun fichier PHP modifié dans cette session) |
| `/editeur/` | inchangé (aucun de ses fichiers modifié depuis 5.15.2) |
| Nom local de `.141` | `jardin-03` → `http://jardin-03.local` |
| Dernier numéro de réponse du chat source | `AQL-R007` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul. **Reste à faire : `docs/TODO.md`**
(fait foi). Décisions : D018 (complétée par l'anti-rejeu), D020 dans
`docs/codex/02_DECISIONS.md`. Sécurité :
`docs/security/SECURITY_RISK_REGISTER.md` (SEC-024 mis à jour, SEC-025
réduit, SEC-027 nouveau).

## 3. Ce qui a été fait

### 3.1 P1.8 — routes de développement absentes de la production (5.15.3, fusion `215d5d7`, D020)

- `AQUALOOK_DEV_ROUTES` (défaut **0**, `src/OtaBuildIdentity.h`) conditionne
  dans `WebManager::setupRoutes()` l'enregistrement des 11 routes
  `/api/debug/*` : `deploy-begin`, `deploy-file`, `deploy-commit`,
  `log-messages`, `sd-selftest`, `verify-web-asset` (+ `/status`),
  `heap-info`, `nvs-stats`, `script-selftest`, `script-dryrun`. Absentes →
  404. Un oubli ne peut jamais les ouvrir en production.
- **Deux environnements** : `ProgrammeArrosage_s3` = binaire de production ;
  `ProgrammeArrosage_s3_banc` (`extends` + `-DAQUALOOK_DEV_ROUTES=1`) =
  binaire du banc `.141`. Répertoires de build distincts (~5 min chacun).
- Identité : ligne série `Routes debug : OUVERTES (binaire de banc)` /
  `fermees (production)` ; `/api/diagnostics` → `build.env`,
  `build.devRoutes`.
- `tools/module_session.py` explique un 404 sur `deploy-begin` ;
  `ota.html` précise que le dépôt direct est réservé au programme de
  développement.
- Effet de bord documenté : `MaintenanceBoot` compare l'environnement du
  manifeste firmware à celui du binaire → banc et production ne
  s'échangeront jamais un firmware par OTA (à reprendre avec P1.4).

### 3.2 P1.9 lot 2a — anti-rejeu des mises à jour Web (5.15.4, fusion `43e4891`)

- `replayRefused()` (`src/WebAssetsUpdater.cpp`), appelée dans
  `checkForUpdate()` et `deployFromManifest()` juste après la vérification de
  signature : version publiée **antérieure** à l'installée →
  `version-anterieure` ; numéro non `X.Y.Z` → `version-illisible` ; `/www`
  jamais touché. Comparateur `src/VersionCompare.h` (suffixe `-dev`, `-rc`…
  ignoré). Version installée non comparable (inconnue, « depot direct »,
  SHA) → rien n'est refusé.
- **Revenir en arrière = republier sous un numéro supérieur.**
- Aides `version-anterieure`, `version-illisible` dans `data/ota-help.json` ;
  code `[MAJ-VERSION]` dans `tools/log_messages.tsv`.
- Lot 2b (signature du manifeste **firmware**) reporté avec P1.4 : la clé
  privée reste hors ligne, la CI ne peut pas signer ; il faudra une
  signature locale de chaque release. Le firmware refuse déjà une version
  antérieure (`OtaStageUpdate` exige `updateAvailable`).

### 3.3 Publication et autres

- Paquet Web **5.15.4** publié le 14 oct. 2026 par l'agent (FTPS, accord
  explicite du propriétaire) : fichiers relus en HTTPS (27/27) avant `.sig`
  puis manifeste ; installé sur `.141`.
- Clé privée `C:\Users\Emman\.aqualook\signature\cle-privee.pem` sauvegardée
  hors ligne par le propriétaire (SEC-025 → réduit).
- Nouvelle demande notée **P2.14** : progression lisible sur le LCD pendant
  une mise à jour + toutes les LED des zones en violet `#6633cc`.

## 4. Fichiers et fonctions modifiés

| Zone | Fichiers |
|---|---|
| Firmware | `src/OtaBuildIdentity.h` (`AQUALOOK_DEV_ROUTES`, `DEV_ROUTES`), `src/WebManager.cpp` (`setupRoutes`, 6 blocs `#if`), `src/EventLog.h` (bannière), `src/SystemDiagnostics.cpp` (`build.env`, `build.devRoutes`), `src/WebAssetsUpdater.cpp` (`replayRefused`), `src/VersionCompare.h` (nouveau) |
| Build | `platformio.ini` (`[env:ProgrammeArrosage_s3_banc]`), `VERSION` |
| Pages module (`data/`) | `ota.html` (texte du dépôt direct), `ota-help.json` (2 codes) |
| Outils | `tools/module_session.py`, `tools/log_messages.tsv`, `tools/soak/ledger.json` |
| Docs | `AGENTS.md` (chaîne `_banc`), `docs/codex/05_BUILD_AND_TEST.md`, `docs/codex/02_DECISIONS.md` (D018, D020), `docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md`, registre (SEC-024, -025, -027), `docs/TODO.md` (P1.8, P1.9, P2.14) |

Volontairement non modifiés : `littlefs/` (buildfs non requis), schéma NVS,
routes non-debug, service PHP, `/editeur/`. Les deux autres
`compareVersions()` (`CloudSync.cpp`, `MaintenanceBoot.cpp`) n'ont pas été
remplacées par `VersionCompare.h` (périmètre minimal ; dette légère).

## 5. Invariants préservés

- I6/I7 : relais, callback et durée maximale intacts ; aucun essai relais.
- D014 : aucune mise à jour déclenchée à distance.
- D016 : toute écriture sous session ; filtre d'hôte inchangé.
- D018 : signature vérifiée **avant** l'anti-rejeu ; refus sans repli.
- Identité de build : `VERSION` source unique ; `.141` vérifié par
  `/api/diagnostics` et par la bannière série.

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (code `47dfa9e`, Flash 79,6 %, 20,6 Ko de moins que le banc) |
| Compilation `ProgrammeArrosage_s3_banc` | SUCCESS (build 1503, flashé) |
| buildfs | non requis |
| Web module | 5.15.4 sur `.141` par OTA signé, 27/27 |
| LCD | non modifié |
| Matériel | `.141`, COM4 |
| Essais P1.8 | production : 11 routes en 404 même sous session, témoins 200, pages et santé normales ; banc : bannière `OUVERTES`, dépôt direct d'`ota.html` relu identique. Bannière du binaire de production non capturée (même constante que `build.devRoutes=false`, vérifié) |
| Essais P1.9 | installation légitime 5.15.2 depuis « depot direct » (27/27) ; manifeste de banc « 5.15.1 » signé, hébergé temporairement dans `/banc-rejeu/` (supprimé ensuite) → `version-anterieure`, 0/27, pages intactes ; `check-version` normal ; montée 5.15.2 → 5.15.4 acceptée. Non essayés : refus au stade vérification ; lecture de la ligne `[MAJ-VERSION]` (journal RAM vidé après la maintenance) |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture d'écran) |

## 7. Risques et limites

- Signature du manifeste firmware non faite (P1.9 lot 2b, avec P1.4).
- Retour arrière d'un paquet Web impossible sans nouveau numéro (voulu).
- Le binaire de production n'a jamais tourné longtemps : `.141` reste en
  binaire de banc. Avant toute mise en service, flasher et observer
  `ProgrammeArrosage_s3`.
- Commit `f13f1fd` : message de documentation mais contenu = registre de soak
  seulement (script échoué) ; la vraie modification est `5c89d6c`.
  Historique non réécrit (déjà poussé).
- `/api/maintenance/last-result` : le champ `environment` y est périmé
  pour une mise à jour Web ; attendre que `recordedEpoch` change avant de
  lire un résultat.
- Restent : P3.12 (Google Fonts), P3.13 (pages firmware inline), P3.14
  (`/editeur/`), P0.5 (HTTPS LAN), P1.6 (secret NVS en clair).

## 8. TODO

Voir **`docs/TODO.md`**. Prochaines étapes suggérées : P3.12, P3.14, P2.14,
puis P0.5 (étude HTTPS LAN), P2.8 (mails), P2.10 (IA), P1.4 + P1.9 lot 2b.

## 9. Procédure exacte de reprise

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -4
git status --short
git -c core.whitespace=cr-at-eol diff --check
python tools/deploy_cloud.py verifier
python tools/check_published_web.py https://aqualook.alwaysdata.net/aqualook-web-manifest.json
python tools/update_signing.py verifier dist/alwaysdata/aqualook-web-manifest.json
```

Attendu : « Serveur ALIGNE », 27/27 + signature ok, version 5.15.4.
Vérifier `.141` : `/api/diagnostics` → `build.version=5.15.4`,
`build.env=ProgrammeArrosage_s3_banc`, `build.devRoutes=true`.

Pour un développement firmware, demander le port COM, puis :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3_banc -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

Validation de compilation du binaire de production :
`pio run -e ProgrammeArrosage_s3`. Publier un paquet Web :
`python tools/publish_web_assets.py X.Y.Z` (signe), puis fichiers → `.sig`
→ manifeste, puis `check_published_web.py` ; le numéro doit être
**supérieur** à celui en service (anti-rejeu).

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent 9401084..main   # cette session
git show 215d5d7 --stat                          # P1.8
git show 43e4891 --stat                          # P1.9 lot 2a
```
