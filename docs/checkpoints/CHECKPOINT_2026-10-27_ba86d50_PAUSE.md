# Checkpoint AquaLook — 27 octobre 2026 — PROJET EN PAUSE (5.20.0)

> **ÉTAT : EN PAUSE** depuis le 27 octobre 2026, à la demande du
> propriétaire. Aucun chantier n'est en cours ; rien n'est à moitié fait dans
> le code. Ce document dit où tout se trouve, ce qui n'a pas été vérifié et
> comment reprendre proprement. Il prend la suite de
> `CHECKPOINT_2026-10-18_24fa2d1.md` (5.20.0), qui reste valable pour le
> détail technique de D023, P3.33 et P3.35.

## 1. Identité au moment de la pause

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, licence **GPL-3.0** depuis D024) |
| Branche | `main` |
| Dernier commit de `main` avant ce document | `ba86d50` (documentation : manuel illustré) |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-27_ba86d50_PAUSE.md`) |
| Commit fonctionnel (dernier code firmware/Web) | **`24fa2d1`** = **v5.20.0** ; depuis, documentation et gouvernance seulement (`src/`, `data/`, `littlefs/`, `VERSION` inchangés) |
| Version (`VERSION`) | `5.20.0` |
| Release publique (latest) | **v5.20.0**, signée (clé `37c2c8068db00bd9`), images de fabrication comprises |
| Pages Web en ligne | **5.20.0**, `check_published_web.py` 30/30 le 27 oct. 2026 |
| Service AlwaysData | ALIGNE sur `24fa2d1` au 18 oct. (PHP inchangé depuis) |
| Dernier numéro de réponse du chat source | `AQL-R018` |

## 2. Modules

| Module | Dernier état connu | Au 27 oct. 2026 |
|---|---|---|
| `.141` (banc, COM4, `Jardin-01`) | 18 oct. : banc 5.20.0 build 1718 `24fa2d1`, pages 5.20.0, santé normale, synchro OK (révision 121) | **injoignable** depuis le PC (192.168.1.169, même réseau) |
| `.109` (second module, COM5) | production 5.19.0 build 1700, pages 5.17.14, PIN et mot de passe Web posés, sans carte relais, non rattaché | **injoignable** |

Facteurs possibles de l'absence de réponse, non vérifiés : modules éteints
ou débranchés, adresse IP changée, Wi-Fi. À la reprise : regarder l'écran,
puis `pio device list` et `/api/diagnostics`.

`.141` a de vraies vannes câblées : à la reprise, ne lancer un arrosage
qu'une zone à la fois, sur une durée courte, sous surveillance (zone 3 ou
4 : le script « Legitime » relance la zone 1).

## 3. Ce qui a été fait depuis le 18 octobre (documentation, sessions cloud)

| Fusion | Contenu |
|---|---|
| `ba027c7` | `CLAUDE.md` et `docs/REPRISE_CLOUD.md` : reprise depuis claude.ai/code (sans le PC) |
| `adf6179` | `docs/codex/` mis en cohérence avec 5.20.0 et D023 |
| `6ad279c` | `AGENTS.md` : « Choix soumis au propriétaire : impacts d'abord » ; règles Git (worktree détaché, pas de réécriture de `main` sans accord) |
| `15b0b26`, `d95d0f1` | manuel utilisateur `docs/guides/manuel/`, chapitres 0 à 6, devenu référence (règle d'`AGENTS.md` : tenu à jour dans le même lot que toute évolution visible) |
| `89461dd` | `README.md` racine aligné sur 5.20.0 |
| `78ea4d2` | licence **GPL-3.0** (D024) ; P4.1 : constat d'une CI partielle |
| `ba86d50` | captures (5.20.0) et illustrations du manuel, jardin d'exemple à Lyon |
| ce document | état de pause ; TODO : P3.35 marqué fait (il l'était depuis `c807e19`) |

## 4. En attente d'une décision du propriétaire

| Sujet | État | Ce qu'il faut |
|---|---|---|
| Branche **`docs/manuel-ch9-13`** (23 oct., poussée, **non fusionnée**) | manuel : sommaire, chapitres 9 (notifications), 11 (mises à jour), 12 (sécurité), 13 (en cas de problème), précision sur la sortie du mode dégradé (ch. 6) ; TODO **P3.38** (messages contradictoires à la sortie du mode dégradé) | relecture puis accord de fusion (documentation seule, sans effet sur les modules) |
| Second module en 5.20.0 | non fait | à faire pendant l'essai du rattachement (P2.18 lot 4), propriétaire devant le module |

## 5. Reste à faire (extrait de `docs/TODO.md`, qui fait foi)

- **Avec le propriétaire devant les modules** : P2.18 lot 4 (rattachement de
  bout en bout sur `.109`, guide `MONTER_UN_MODULE_NEUF.md` suivi en même
  temps) ; P2.4, P2.5 (gestes au doigt, portail captif).
- **Fonctionnel** : P2.21 nom des cartes interfaces (schéma persistant du
  câblage, migration) ; P2.22 page `ota` guidée étape par étape ; P2.19
  Wi-Fi borne faible ; P2.8 notifications par mail ; P2.2 effacement du PIN
  en ligne ; P2.10 (idée, à décider).
- **Défauts** : P3.36 (fenêtre de pluie par zone sans effet, lu dans le
  code), P3.37, P3.31, P3.32, P3.29, et la série P3.5 à P3.15.
- **Avant toute mise en service chez un tiers** : P0.5 (HTTPS local) et
  P1.6 (verrouillage matériel, D022).
- **Dette** : P4.1 (compléter la CI), P4.2, P4.3, P4.6 (sortir le dépôt
  d'OneDrive), P4.7, P4.10, P4.12.

## 6. Ce qui n'a jamais été vérifié sur matériel (à garder en tête)

- P3.33 sur une vraie carte vide, et le cas « carte revenue, pages perdues ».
- D023 : aucune vanne actionnée depuis le retrait d'`IoExpander` (chemin des
  vannes non modifié ; V4 confirmé actif au journal le 18 oct.).
- Le guide du module neuf, jamais suivi de bout en bout.
- La présence d'électrovanne n'existe plus (retirée avec `IoExpander`) : à
  refaire par une entrée déclarée quand le montage existera.

## 7. Invariants et état technique

- Compilation `ProgrammeArrosage_s3` : SUCCESS sur le contenu de `24fa2d1`
  (Flash 81,0 %) ; aucun code modifié depuis, pas de recompilation requise.
- buildfs : non requis (`littlefs/` inchangé depuis le 18 oct.).
- NVS : aucun format modifié depuis 5.19.0 ; `aq_io` laissé sans lecture (D023).
- Relais, planificateur, sécurité de durée maximale : inchangés.
- Dépôt : `main` = `origin/main` au moment de la pause ; un fichier non
  suivi sans rapport (capture du 11 oct.) dans le dossier parent.

## 8. Risques pendant la pause

- **Ubuntu 26** sur les machines GitHub Actions (`ubuntu-latest`, depuis le
  19 oct.) : la prochaine release est le premier essai du workflow
  `ota-release.yml` sur cette image ; surveiller son résultat.
- **Bibliothèques** : patches appliqués au build (AsyncTCP, ESPAsyncWebServer,
  P4.7) ; ne pas mettre à jour une bibliothèque sans revérifier les patches.
- **Sécurité (D017)** : revue trimestrielle du référentiel `docs/security/`
  due au plus tard mi-janvier 2027 ; vérifier les failles publiées
  (ESP-IDF, mbedTLS, PHP, AlwaysData) avant toute remise en service.
- **Clé de signature** : sur le poste du propriétaire, sauvegarde hors ligne
  faite le 14 oct. ; sans elle, plus aucune release ni paquet Web signés.
- **Identifiants** (`.env` racine : FTP AlwaysData, mot de passe Web de
  banc) : locaux, jamais dans Git ; à conserver avec le poste.
- Module laissé allumé sans surveillance : arrosage et sécurités autonomes
  (sans Wi-Fi ni serveur) ; un défaut matériel (alimentation, carte SD)
  ne serait signalé que par ntfy si le Wi-Fi est là.

## 9. Reprendre

### Sur le PC du propriétaire (matériel accessible)

```powershell
cd C:\Users\Emman\OneDrive\Documents\VsCode_travail\arrosage
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -8
git status --short
git -c core.whitespace=cr-at-eol diff --check
python tools/deploy_cloud.py verifier
python tools/check_published_web.py https://aqualook.alwaysdata.net/aqualook-web-manifest.json
pio device list
```

Puis, modules allumés (ports à reconfirmer) :

```powershell
curl http://192.168.1.141/api/diagnostics   # attendu : 5.20.0 build 1718 24fa2d1, banc
curl http://192.168.1.109/api/diagnostics   # attendu : production 5.19.0
```

Si une bibliothèque, PlatformIO ou l'image GitHub a bougé pendant la pause :
`pio run -e ProgrammeArrosage_s3` avant toute autre chose.

### Sans le PC (claude.ai/code)

`CLAUDE.md` puis `docs/REPRISE_CLOUD.md` : documentation seulement, pas de
flash, de publication ni de signature.

### Ordre conseillé

1. Décider du sort de `docs/manuel-ch9-13` (§4).
2. P2.18 lot 4 sur `.109` avec mise à jour en 5.20.0.
3. P3.36 (pluie), puis P2.21 et P2.22.

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent 24fa2d1..main     # tout ce qui a suivi 5.20.0
git log --oneline origin/main..origin/docs/manuel-ch9-13
git show v5.20.0
git branch -r --no-merged origin/main
```
