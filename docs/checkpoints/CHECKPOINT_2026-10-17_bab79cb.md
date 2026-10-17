# Checkpoint AquaLook — 17 octobre 2026 (soir) — 5.18.1 : un module neuf installe ses pages seul (P2.18), serveur AquaLook unique

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-17_2266374.md` (même jour, fin d'après-midi : 5.18.0).

> **En un coup d'œil :** essai « paquet Web sur une carte SD vide » fait par
> la page `/ota` de secours du second module. Il a révélé que **aucun module
> neuf ne pouvait installer ses pages** (catalogue cherché sur la release
> GitHub, qui ne le porte plus depuis D021 → 404). Corrigé en 5.18.1, avec
> un **seul paramètre « Serveur AquaLook »** (défaut écrit à un seul endroit,
> modifiable, suivi automatiquement si l'hébergeur change), la création de
> `/www` sur une carte neuve et une page `/ota` de secours lisible. Second
> module : pages 5.17.14 installées (30/30). `.141` : 5.18.1, synchro cloud
> intacte. Nouveau défaut **P3.34** (carte SD muette après l'installation,
> rendue par une coupure d'alimentation), vu une fois.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` ; `fix/p218-module-neuf-pages-web` fusionnée `--no-ff` puis supprimée (locale et distante) |
| Commit fonctionnel | **`bab79cb`** (fusion) ; dernier code `8bd1c28`, version et doc `fe8f9c7` ; après lui, documentation seule |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-17_bab79cb.md`) |
| Version (`VERSION`) | **`5.18.1`** (pas de tag) |
| Release GitHub publique (latest) | `v5.17.12`, signée, sans images de fabrication (la première à les porter sera ≥ 5.18.x) |
| Module de banc `.141` | COM4, `5.18.1` build **1688** `fe8f9c7`, `ProgrammeArrosage_s3_banc`, santé normale, synchro cloud OK (révision 121, serveur à jour), pages 5.17.14 |
| Second module | **COM5**, `aql-44bd8d7a64cc`, 192.168.1.109, production `5.18.1` build 1684 (`63345a0+modif`, contenu identique à `8bd1c28`), **pages 5.17.14 installées par lui-même** (carte 64 Go SDXC), sans carte relais, sans mot de passe Web ni PIN, non rattaché |
| Pages Web | en ligne : 5.17.14 (30/30 cohérentes) ; `.141` et second module : 5.17.14 |
| Service AlwaysData | inchangé, ALIGNE sur `bab79cb` |
| Dernier numéro de réponse du chat source | `AQL-R010` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul. **Reste à faire : `docs/TODO.md`**
(P2.18 lots 3 à 5, P3.31 à P3.34).

## 3. Ce qui a été fait

| Commit | Contenu |
|---|---|
| `023db96` | **Serveur AquaLook unique** : `AQ_SERVICE_HOST_DEFAULT` et `AQ_WEB_MANIFEST_PATH` dans `include/config.h` (seule mention de l'hébergeur). `CloudSync::loadConfig()` : hôte vide **ou espace NVS `aq_cloud` absent** = serveur par défaut, port 443, HTTPS ; `CloudSyncScheduler::save()` enregistre ce défaut **vide** (le module suit le défaut du programme installé) ; `set()` : hôte effacé = retour au défaut. `ConfigManager::loadWebAssetsUrl()` : catalogue déduit du serveur (`https://<serveur>/aqualook-web-manifest.json`) ; une clé `waUrl` identique, ou égale à l'ancien défaut GitHub, est **oubliée** ; plus de sortie anticipée si la NVS est illisible. `setWebAssetsUrl()` : adresse du serveur = oubli de la clé. `handleSetCloudSync()` réapplique la source des pages. `WebAssetsUpdater::DEFAULT_MANIFEST_URL` = serveur par défaut |
| `23825cc` | `StorageManager::commitAssetStaging()` crée `/www` s'il manque (carte neuve) |
| `8bd1c28` | Page `/ota` de secours (`WebManager.h`, `OTA_FALLBACK_JS`) : état en clair (programme, pages Web, serveur), message selon `/api/storage` (carte neuve, absente, illisible, complète, pages installées mais carte muette), une seule action mise en avant, attente du retour du module et résultat en français (`/api/maintenance/last-result`), lien « Ouvrir AquaLook », « Changer de serveur » (POST `/api/cloudSync` `{host}`), programme replié, navigation en haut. Accueil de secours : « carte neuve » au lieu de « absente ». Banc CSP : 6 scénarios |
| `fe8f9c7` | `VERSION` 5.18.1 ; `docs/TODO.md` (P2.18, P3.34), `docs/codex/05_BUILD_AND_TEST.md` (règle du serveur), registre SEC-024 |
| `bab79cb` | fusion dans `main` |

## 4. Essais

| Cible | Port | Essai | Attendu | Observé |
|---|---|---|---|---|
| Second module, 5.18.0 | COM5 | `/ota` de secours : Vérifier, puis Pages Web | pages installées | **`http-404`** deux fois : catalogue cherché sur `github.com/.../releases/latest/download/aqualook-web-manifest.json` |
| Second module, 5.18.1 (1er flash) | COM5 | serveur effectif | `aqualook.alwaysdata.net` | **hôte vide** (`https:///…`, port 80) : espace `aq_cloud` absent → corrigé, reflashé |
| Second module, 5.18.1 | COM5 | serveur et catalogue | défaut, HTTPS 443 | `aqualook.alwaysdata.net`, catalogue AlwaysData |
| Second module | COM5 | `/ota` → « Installer les pages Web » (propriétaire) | 30 fichiers, résultat lisible | transit, 30 vérifiés, **bascule 31 fichiers**, « Pages Web 5.17.14 installées (30 fichiers) » affiché par la page |
| Second module | COM5 | retour de maintenance | SD montée | **`sd_begin_failed`** ×5 puis redémarrage logiciel : toujours muette ; **coupure 10 s** : montée, pages servies (P3.34) |
| Second module | COM5 | flash (réinitialisation de la puce, sans écritures avant) | SD montée | montée |
| `.141` | COM4 | banc 5.18.1 sur un module rattaché | réglages gardés, synchro OK | serveur, `Jardin-01`, jeton, source des pages gardés ; synchro forcée HTTP 200, révision 121 (inchangée), serveur à jour ; santé normale |
| Banc CSP | — | 7 scénarios `fw-*` à 390 px | tout passe | tout passe (dont `fw-ota-carte-muette`) |
| `ProgrammeArrosage_s3` | — | compilation | SUCCESS | SUCCESS, Flash 81,0 % (+0,5 %) |

**Non essayé :** lien « Ouvrir AquaLook » et consigne de coupure sur le
module (banc seulement) ; « Changer de serveur » vers un autre hébergeur
réel ; rattachement d'un module neuf avec le serveur par défaut (lot 4) ;
page `/ota` de secours vue sur iPhone.

## 5. Fichiers modifiés (depuis `63345a0`)

| Zone | Fichiers |
|---|---|
| Firmware | `include/config.h`, `src/CloudSync.cpp`, `src/ConfigManager.cpp`, `src/WebAssetsUpdater.h`, `src/StorageManager.cpp`, `src/WebManager.cpp`, `src/WebManager.h` |
| Outils | `tools/csp_banc_pages.py`, `tools/soak/ledger.json` |
| Docs | `docs/TODO.md`, `docs/codex/05_BUILD_AND_TEST.md`, `docs/security/SECURITY_RISK_REGISTER.md`, ce checkpoint |
| Version | `VERSION` = 5.18.1 |

Volontairement non modifiés : `data/` (pages SD), `littlefs/`, schéma NVS
`ALOK`, routes (aucune ajoutée ni renommée ; `/api/webAssetsUrl` refuse
toujours une adresse vide), relais, PHP, page `/ota.html` de la SD.

## 6. Invariants préservés

- I1/I2 : `littlefs/` inchangé (buildfs refait pour l'image de fabrication, contenu identique).
- F8/NVS : `ALOK` inchangé ; clés existantes (`aq_cloud/host`, `aqualook/waUrl`) réinterprétées sans changement de format ; valeur vide = défaut.
- F5/F6 : routes et IDs inchangés ; la page `/ota` de secours garde `b1`…`b4` et `msg`.
- Sécurité : signature des pages (D018) indépendante du serveur ; HTTPS imposé hors réseau local ; changement de serveur sous session (D016) ; aucun script inline (banc CSP) ; aucune mise à jour déclenchée à distance (D014).
- Relais : non touchés.

## 7. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (contenu de `fe8f9c7`) |
| Banc `ProgrammeArrosage_s3_banc` | `.141` 5.18.1 build 1688 `fe8f9c7` |
| buildfs | SUCCESS (contenu inchangé) |
| Web | pages SD inchangées (5.17.14) ; pages intégrées `/ota` et accueil de secours nouvelles |
| LCD | inchangé |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture du 11 oct.) |

## 8. Risques et limites

- **P3.34** : carte SD muette après une installation de pages (vu une fois) ; contournement : coupure d'alimentation, consigne affichée par `/ota`.
- Un module rattaché envoie son jeton au serveur réglé : ne changer de serveur que vers un serveur de confiance (texte de la page ; registre SEC-024).
- Paquet de confiance TLS (`OtaTlsTrust`) : DigiCert, USERTrust/Sectigo, ISRG/Let's Encrypt. Un hébergeur sous une autre autorité demanderait une release (GitHub, toujours joignable).
- Le binaire du second module porte `63345a0+modif` (flashé avant les commits ; contenu identique à `8bd1c28`).
- Lot 1 (outil SD) devenu facultatif pour les pages ; reste utile hors réseau.
- P3.31, P3.32, P3.33 inchangés.

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

Modules (ports à reconfirmer, `pio device list`) :

```powershell
curl http://192.168.1.141/api/diagnostics   # build 1688 fe8f9c7, banc
curl http://192.168.1.109/api/diagnostics   # second module, production 5.18.1
curl http://192.168.1.109/api/display       # webAssetsUrl = serveur AquaLook
```

Suite proposée :
1. Release **v5.18.1** (tag, workflow de fabrication, `release_firmware.py verifier` puis `signer` par le propriétaire), puis `preparer_module.py flasher --port <PORT_COM> --effacer` sans `--local` sur le second module, et nouvel essai complet « module neuf » (carte vidée).
2. P2.18 lot 3 (mot de passe Web et PIN au premier démarrage), lot 4 (rattachement avec le serveur par défaut), lot 5 (guide).
3. P3.34 si la carte redevient muette ; P3.31–P3.33.

Firmware :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3_banc -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

Second module (production, garde la NVS) :

```powershell
pio run -e ProgrammeArrosage_s3
pio run -e ProgrammeArrosage_s3 -t buildfs
python tools/preparer_module.py flasher --port <PORT_COM> --local --sauver-nvs nvs.bin
```

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent 63345a0..main
git show 023db96 --stat     # serveur AquaLook unique
git show 23825cc            # /www a la bascule
git show 8bd1c28 --stat     # page /ota de secours
```
