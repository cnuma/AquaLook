# Checkpoint AquaLook — 14 octobre 2026 (nuit) — 5.17.4 : progression du programme confirmée à l'œil, P3.17, paquet Web 5.17.4

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-14_87737bb.md` ; `CHECKPOINT_2026-10-14_2d49381.md` reste
la référence pour la chaîne de release signée (P1.4, P1.9 lot 2b, D021).

> **En un coup d'œil :** une vraie mise à jour du programme (5.17.1 → 5.17.2,
> release GitHub signée) a été lancée par le propriétaire depuis `ota.html`
> sur `.141` ; il a vu à l'œil la progression en Ko : P1.4 et P2.14 n'ont plus
> de point visuel ouvert. Défaut trouvé en route et corrigé (P3.17) : après
> l'installation, `ota.html` affichait l'ancienne version. Paquet Web
> **5.17.4** publié, signé et installé sur `.141`.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` ; branches `docs/p14-progression-ko-confirmee` et `fix/p317-version-installee-ota` fusionnées `--no-ff` puis supprimées |
| Commit fonctionnel | **`29fe2d1`** |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-14_29fe2d1.md`) |
| Version (`VERSION`) | **`5.17.4`** (montée pour le seul paquet Web) ; pas de tag, pas de release du programme |
| Release GitHub publique (latest) | **`v5.17.2`**, signée (inchangée) |
| Firmware sur `.141` | `5.17.3`, build 1549, `dcc6804`, env `ProgrammeArrosage_s3_banc` (`devRoutes=true`, `app0`) |
| Pages Web sur `.141` / paquet en ligne | **`5.17.4`** / **`5.17.4`** (signé, clé `37c2c8068db00bd9`) |
| Service AlwaysData (PHP) | `deploy_cloud.py verifier` : ALIGNE |
| Nom local de `.141` | `jardin-03` → `http://jardin-03.local`, port **COM4** (confirmé dans cette session, à reconfirmer) |
| Dernier numéro de réponse du chat source | `AQL-R008` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul. **Reste à faire : `docs/TODO.md`**
(fait foi). Pas de nouvelle décision.

## 3. Ce qui a été fait

### 3.1 Progression du programme en Ko confirmée à l'œil (P1.4, P2.14)

`.141` étant déjà en 5.17.2 (dernière release), il fallait une version de
départ plus ancienne **avec** le correctif de lecture du manifeste (une
5.17.1 sans lui refuse toute release signée) : le commit `3c0ee1e`
(`VERSION` 5.17.1) a été compilé en `ProgrammeArrosage_s3` dans un worktree
temporaire hors OneDrive (identité exacte, `main` et `VERSION` intacts), puis
flashé par USB (COM4).

| Étape | Résultat observé |
|---|---|
| Flash USB | 5.17.1, build 1534, `3c0ee1e`, `ProgrammeArrosage_s3`, `devRoutes=false` |
| Vérifier puis Installer, par le propriétaire dans `ota.html` | 1 575 296 o en 10,2 s, SHA-256 calculé = attendu (`65ac284e…`), `activated-app1-pending-validation` |
| Redémarrage | `app1`, garde « firmware valide après 45006 ms, retour arrière désarmé » ; 5.17.2 `c14db25` (binaire GitHub) en marche, santé normale |
| Observation du propriétaire | compteur en Ko et pourcentage, écran et LED violettes **confirmés à l'œil** |
| Retour au banc par USB | `ProgrammeArrosage_s3_banc` 5.17.3 build 1549, `dcc6804`, `app0`, santé normale |

Note : la première annonce « tout est ok » du propriétaire précédait
l'installation (le journal ne montrait aucune maintenance) ; l'essai n'a été
consigné qu'après relecture du module.

### 3.2 P3.17 — `ota.html` affiche la version en marche (`12f61aa`)

- Constat : après l'installation, « Programme du module · installée 5.17.1 ·
  non vérifié » et verdict « Tout est à jour ». `installedVersion` vient de
  `/api/maintenance/last-result`, écrit pendant l'installation, avant le
  redémarrage (`src/MaintenanceBoot.cpp`).
- `data/ota.js` : variable `FW` = `build.version` de `/api/diagnostics`
  (même source que `sante.js`, F14), relue au chargement et à chaque
  nouvelle opération (clé `command|recordedEpoch`) ; la ligne du programme
  utilise `FW`, repli sur la valeur enregistrée si la route ne répond pas ;
  puce « installe » quand `install_update` a réussi et que `FW` =
  `availableVersion` ; une version disponible égale à `FW` n'est plus
  proposée. Le « Détail technique » garde les valeurs enregistrées.
- `tools/csp_banc_pages.py` : scénarios `ota`, `ota-repli`, `ota-dispo`
  (enregistrement réel du 14 oct. simulé). Témoin : l'ancien `ota.js` échoue
  sur `ota` (2 échecs), le nouveau passe ; banc complet tout passe.
- Ni firmware, ni `littlefs/`, ni route, ni ID. +0,6 Ko sur la SD.

### 3.3 Paquet Web 5.17.4

`publish_web_assets.py 5.17.4 --comparer` (seul `ota.js` change,
signature-ok) ; envoi FTPS par l'agent sur demande du propriétaire, avec
`deploy_cloud.connecter`/`publier` (nom temporaire puis renommage) : 27
fichiers dans `/web/v5.17.4/`, relus en HTTPS 27/27, puis `.sig`, puis
manifeste. `check_published_web.py` 27/27 + `signature-ok` ; serveur ALIGNE.
Installé sur `.141` par `POST /api/webassets/update` (HTTP 202) :
`deploye version=5.17.4 fichiers=27/27`, `assets-version` 5.17.4, 27/27
servis identiques au paquet, santé normale.

## 4. Essais non faits

- Nouvel affichage de P3.17 vu à l'œil après une **vraie** installation du
  programme (vérifié au banc CSP avec l'enregistrement réel, et sur `.141`
  la page lit bien `/api/diagnostics`, HTTP 200).
- Écran d'échec d'une mise à jour ; texte d'aide d'un refus de signature du
  programme (aucun refus provoqué).

## 5. Fichiers modifiés

| Zone | Fichiers |
|---|---|
| Web (SD) | `data/ota.js` |
| Outils | `tools/csp_banc_pages.py`, `tools/soak/ledger.json` |
| Docs | `docs/TODO.md`, ce checkpoint |
| Version | `VERSION` |

Volontairement non modifiés : firmware `src/`, `littlefs/`, NVS, routes, IDs,
relais, service PHP, `/editeur/` (`ota.js` n'en fait pas partie).

## 6. Invariants préservés

- F5/F6 : IDs et routes inchangés ; pas de script inline ni ressource externe
  (banc CSP, témoin négatif).
- F14 : la version affichée vient de la même source compile-time que le LCD
  et la page Santé.
- D014 : mises à jour lancées par l'utilisateur (installation du programme
  cliquée par le propriétaire ; publication et installation Web sur sa
  demande).
- D018 : paquet signé sur le poste du propriétaire ; manifeste en dernier.
- I6/I7 : aucune commande de vanne touchée.

## 7. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | **SUCCESS** sur l'arbre de `29fe2d1` (voir §7.1) |
| buildfs | non requis (`littlefs/` inchangé) |
| Web | 5.17.4 en ligne et sur `.141` |
| LCD / matériel | inchangés ; progression du programme confirmée à l'œil |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture d'écran) |

### 7.1 Compilation

`pio run -e ProgrammeArrosage_s3` : **SUCCESS** en 5 min 49 s, RAM 27,1 %, Flash 80,1 % (1 575 137 o sur 1 966 080). Lancée pendant la branche `fix/p317-version-installee-ota` ; `src/`, `include/`, `lib/`, `littlefs/` et `platformio.ini` sont identiques à `29fe2d1` (seuls `VERSION`, `data/`, `tools/` et `docs/` diffèrent de `dcc6804`).

## 8. Risques et limites

- `.141` exécute le firmware de banc 5.17.3 alors que `VERSION` vaut 5.17.4
  (montée pour le paquet Web seul) : normal. Une release du programme exige
  `VERSION` = étiquette, supérieure à 5.17.2.
- Un module en 5.17.1 **sans** le correctif (`0211f29`) ne peut plus suivre
  l'OTA : flash USB. Pour rejouer l'essai de ce checkpoint, partir de
  `3c0ee1e` (worktree), jamais de `v5.17.1`.
- Cache GitHub et AlwaysData : attendre ~1 min après une publication.
- Compilation dans OneDrive lente (13 min pour un build complet) : P4.6.
- Git Bash réécrit `/api/...` en chemin Windows pour `module_session.py
  post` : lancer cette commande depuis PowerShell.

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
python tools/release_firmware.py verifier 5.17.2
```

Attendu : « Serveur ALIGNE », 27/27 + `signature-ok` (version 5.17.4),
release v5.17.2 conforme. `.141` : `/assets-version.json` → 5.17.4 ;
`/api/diagnostics` → `build.version=5.17.3`, `build.env=ProgrammeArrosage_s3_banc`.

Firmware (port COM à reconfirmer) :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3_banc -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent dcc6804..main
git show 12f61aa            # P3.17 : ota.js + banc CSP
python tools/csp_banc_pages.py ota ota-repli ota-dispo -v
gh release view v5.17.2 -R cnuma/AquaLook
```
