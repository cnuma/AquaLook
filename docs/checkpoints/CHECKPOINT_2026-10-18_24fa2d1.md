# Checkpoint AquaLook — 18 octobre 2026 — 5.20.0 : une seule couche d'entrées/sorties (D023), carte SD neuve sans incident, guide du module neuf

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-17_d69d652.md` (17 oct. 2026 nuit : 5.19.0).

> **En un coup d'œil :** releases **v5.19.0** puis **v5.20.0** publiées et
> signées ; paquet Web **5.20.0** en ligne (30/30). **D023** : `IoExpander`
> (MCP23017 seul, désactivée partout) retirée, « Cartes interfaces » est
> l'unique couche d'entrées/sorties, en trois parties (Cartes, Sorties,
> Entrées). **P3.33** : une carte SD saine sans pages n'est plus un incident.
> **P3.35** : libellés choisis par le propriétaire. **P2.18 lot 5** : guide
> `docs/guides/MONTER_UN_MODULE_NEUF.md`. Lot 4 relu sans code, essai à faire.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` ; `fix/p335-libelles-io`, `fix/p333-carte-neuve`, `docs/p218-lot5-guide-module-neuf`, `feature/d023-entrees-sorties-unifiees` fusionnées `--no-ff` puis supprimées |
| Commit fonctionnel | **`24fa2d1`** (fusion de D023) |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-18_24fa2d1.md`) |
| Version (`VERSION`) | **`5.20.0`**, tag `v5.20.0` sur `24fa2d1` |
| Release GitHub publique (latest) | **`v5.20.0`**, signée (clé `37c2c8068db00bd9`), images de fabrication et `aqualook-fabrication.json` signé ; `v5.19.0` (tag sur `c4bbca3`) publiée le même jour |
| Pages Web en ligne | **5.20.0**, `check_published_web.py` 30/30, `/editeur/` identique ; service AlwaysData ALIGNE sur `24fa2d1` |
| Module de banc `.141` | COM4, `5.20.0` build **1718** `24fa2d1` (`main`), `ProgrammeArrosage_s3_banc`, pages 5.20.0 installées par `/api/webassets/update` (30/30), santé normale, carte prête, synchro cloud OK (révision 121), `Jardin-01` |
| Second module | COM5, 192.168.1.109, production **5.19.0** build 1700 `d4a5691` (inchangé), carte 32 Go, pages 5.17.14, PIN et mot de passe Web posés, sans carte relais, **non rattaché** |
| Dernier numéro de réponse du chat source | `AQL-R014` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul. **Reste à faire : `docs/TODO.md`**.

## 3. Ce qui a été fait

| Commit | Contenu |
|---|---|
| tag `v5.19.0` | release 5.19.0 (workflow 2 min, `verifier` conforme, `signer`) |
| `7b694cc`, `b3d9e35` → `c807e19` | **P3.35** : rubriques « Cartes interfaces » (`g-relais`, placée en premier) et « Capteurs et actionneurs » (`g-io`) ; banc CSP `index-menu`, `index-raccordement` |
| `12e3224`, `b37ebf6` → `23c1d6b` | **P3.33** : `StorageManager` — carte montée et saine sans `/www/index.html` (démarrage ou remontage) : ni `STORAGE_SD`, ni incident, ni remontage ; `enterWebAssetsMissing()` (ligne `[SD-PAGES]`, catalogue `tools/log_messages.tsv`) ; `watchWebAssets()` : `exists` toutes les 2 s, `ready` dès l'arrivée des pages ; vraie erreur → chemin d'incident |
| `21379c3`, `c3f9d31` → `2174217` | **P2.18 lot 5** : guide `docs/guides/MONTER_UN_MODULE_NEUF.md`, lien dans `START_HERE.md` |
| `524e601`, `899839f`, `9adb254`, `d5ef801` → `24fa2d1` | **D023** : retrait d'`IoExpander` (`main.cpp`, routes `GET /api/io`, `POST /api/io/config`, `/api/io/output`, rubrique `g-io`/`sec-io`, poste `ioExpander` de `RuntimeProfiler`, fichiers `src/IoExpander*`) ; NVS `aq_io` laissée sans lecture ; `VERSION` 5.20.0 ; `renderTopoEditor()` en trois parties (Cartes, Sorties, Entrées, un bouton par liste ; une seule table `topoAssign`, « Affectation N » inchangé) ; correctif des lignes perdues (« undefined ») |
| paquet Web 5.20.0, tag `v5.20.0` | publiés sur accord du propriétaire |

## 4. Essais

| Cible | Port | Essai | Attendu | Observé |
|---|---|---|---|---|
| GitHub Actions | — | releases v5.19.0, v5.20.0 | brouillon conforme | 2 min et 2 min 40 ; `verifier` conforme ; `signer` : publiée, `signature-ok` |
| `.141` | COM4 | P3.33, binaire d'essai **non committé** (témoin `/www/index-p333.html` absent) | `[SD-PAGES]`, aucun incident ni remontage | `[SD-PAGES] demarrage : carte saine...`, santé normale, 4 min de capture série sans remontage ni redémarrage |
| `.141` | COM4 | dépôt du témoin par `/api/debug/deploy-*` | `ready` sans redémarrage | `ready` 1 s après la bascule, `/app.js` servi |
| `.141` | COM4 | D023, banc 5.20.0 | `/api/io` absent, V4 intact | `/api/io` 404 (même sous session), `/api/relay/topology` 200, « moteur V4 actif sur les sorties, 3 carte(s), 4 zone(s) », santé normale, synchro révision 121 |
| `.141` | — | « Cartes interfaces » dans le navigateur (propriétaire) | trois parties | 1re capture : lignes remplacées par « undefined » → corrigé (`d5ef801`) ; 2e capture : « Nickel » |
| `.141` | COM4 | banc `main` `24fa2d1` + `/api/webassets/update` | 5.20.0, pages 30/30 | build 1718 `24fa2d1`, `deploye version=5.20.0 fichiers=30/30`, pages identiques au paquet, santé normale, synchro OK révision 121 |
| Banc CSP | — | toutes les pages à 390 px | tout passe | tout passe (scénarios `index-menu`, `index-raccordement` renforcés) |
| `ProgrammeArrosage_s3` | — | compilation | SUCCESS | SUCCESS, Flash **81,0 %** (81,4 % avant D023), RAM 26,9 % |

**Non essayé :** aucune vanne actionnée pendant la session (propriétaire à
distance) ; P3.33 sur une vraie carte vide et sur « carte revenue, pages
perdues » ; rattachement de bout en bout (lot 4) ; guide suivi sur un module ;
second module non mis à jour (5.19.0).

## 5. Fichiers modifiés (depuis `c4bbca3`)

| Zone | Fichiers |
|---|---|
| Firmware | `src/StorageManager.cpp`, `src/StorageManager.h`, `src/main.cpp`, `src/WebManager.cpp`, `src/WebManager.h`, `src/RuntimeProfiler.cpp`, `src/RuntimeProfiler.h` ; supprimés : `src/IoExpanderConfig.*`, `src/IoExpanderManager.*` |
| Web (SD) | `data/app.js`, `data/index.html`, `data/index.js` |
| Outils | `tools/csp_banc_pages.py`, `tools/log_messages.tsv`, `tools/soak/ledger.json` |
| Docs | `docs/codex/02_DECISIONS.md` (D023), `docs/TODO.md`, `docs/guides/MONTER_UN_MODULE_NEUF.md`, `docs/START_HERE.md`, `docs/REPRISE_INGENIEUR.md`, `docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md`, ce checkpoint |
| Version | `VERSION` = 5.20.0 |

Volontairement non modifiés : `littlefs/`, schéma NVS `ALOK`, câblage
`RelayTopology`/`RelayTopologyStore`, relais, PHP.

## 6. Invariants préservés

- I1/I2 : `littlefs/` inchangé (pas de buildfs requis).
- F8/NVS : aucun format persistant modifié ; `aq_io` n'est plus lu, ni effacé.
- F5/F6 : IDs conservés ; routes `/api/io*` retirées par décision documentée (D023).
- Relais (I6, I7) : chemin des vannes non modifié ; V4 confirmé actif au journal.
- Sécurité : pages sans script inline (banc CSP) ; surface réduite (3 routes en moins) ; publication et release sur accord du propriétaire.

## 7. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (contenu de `24fa2d1`) |
| Banc `ProgrammeArrosage_s3_banc` | `.141` 5.20.0 build 1718 `24fa2d1` |
| buildfs | non requis (`littlefs/` inchangé) |
| Web | 5.20.0 en ligne et sur `.141` |
| LCD | inchangé |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture du 11 oct.) |

## 8. Risques et limites

- Modules en 5.20.0 avec des pages 5.17.14 : l'ancienne rubrique « Capteurs et actionneurs » affiche « Lecture impossible » (sans autre effet) ; mettre aussi les pages à jour.
- Détection de présence d'électrovanne retirée avec `IoExpander` : à refaire par une entrée déclarée (`ROLE_INPUT_PRESENCE`) quand le montage existera.
- `_cardMounted` n'est jamais remis à faux après une vraie panne SD (antérieur, relevé en route, non traité).
- P3.34 (carte Lenovo 64 Go) toujours sans remède logiciel ; guide : cartes conseillées.
- Le contrôle de sécurité de l'environnement de l'agent a refusé plusieurs actions légitimes (flash, fusion, `git rm`) ; elles ont été faites après accord explicite du propriétaire.

## 9. Procédure exacte de reprise

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -8
git status --short
git -c core.whitespace=cr-at-eol diff --check
python tools/deploy_cloud.py verifier
python tools/check_published_web.py https://aqualook.alwaysdata.net/aqualook-web-manifest.json
```

Modules (ports à reconfirmer, `pio device list`) :

```powershell
curl http://192.168.1.141/api/diagnostics   # build 1718 24fa2d1, banc 5.20.0
curl http://192.168.1.109/api/diagnostics   # second module, production 5.19.0
```

Suite proposée (détail dans `docs/TODO.md`) :
1. P2.18 lot 4 : essai du rattachement sur le second module (geste sur l'écran + code dans l'espace en ligne), en suivant le guide ; mettre le second module en 5.20.0 (programme et pages) au passage.
2. P2.21 : nom des cartes interfaces (schéma persistant du câblage, migration).
3. P3.31, P3.32 ; P4.1 (compilation sur GitHub à chaque push, passage d'`ubuntu-latest` à Ubuntu 26 dès le 19 oct.) ; puis P2.19, P3.29, P2.8, P2.2.

Firmware :

```powershell
git -c core.whitespace=cr-at-eol diff --check
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3_banc -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent c4bbca3..main
git show 12e3224            # P3.33 : carte saine sans pages
git show 524e601 --stat     # D023 lot B : retrait d'IoExpander
git show d5ef801            # D023 lot C : lignes affichees
git show v5.20.0
```
