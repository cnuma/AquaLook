# Checkpoint AquaLook — 8 octobre 2026 — 5.10.0-dev, éditeur terminé, CloudSync TLS 8 s, fusion vers `main`

Document de reprise autonome. Avec `AGENTS.md` et `docs/REPRISE_INGENIEUR.md`,
il suffit pour reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-07_7d273fd.md`.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public) |
| Branches | `main` = `feature/cloud-scripts` au commit de ce document (fusion PR #29 en avance rapide) |
| Commit fonctionnel | `fade726` |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-08_fade726.md`) |
| Version fonctionnelle (`VERSION`) | **`5.10.0-dev`** (décidée le 8 oct. 2026) |
| Ressources Web préparées | **`5.10.0`** dans `dist/alwaysdata` (non versionné), en ligne : `5.9.49` |
| Dernier numéro de réponse du chat source | `AQL-R012` |
| Cible | `ProgrammeArrosage_s3`, module de test `192.168.1.141`, COM4 (confirmé le 7-8 oct. 2026) |

## 2. Source de vérité

`main` au commit de ce document (identique à `feature/cloud-scripts`). Les
nouveaux travaux repartent de `main` sur une branche dédiée. `AGENTS.md` est
désormais aligné sur la chaîne de build réelle (`ProgrammeArrosage_s3`).

## 3. Ce qui a été fait (7-8 oct. 2026)

| Commit | Contenu | Validation |
|---|---|---|
| `31e89fb` | Retrait de l'ancien éditeur texte `scripts.html` (+ bouton, lien de couverture, `urls.html`) | utilisateur, sur `.141` |
| `05c8b73` | Pastille de gamme réduite à « Pro » (index + éditeur) | utilisateur |
| `f3af6d1` | « Script non enregistré » sous le nom, clignotant ; barre sur une ligne jusqu'à ~1 200 px | utilisateur |
| `291b39f` | « Fermer » bleu partout : `.btn.fermer` / `.btn-full.fermer` dans `style-base.css`, bouton en bas de la modale d'`index.html`, fenêtre de sauvegarde d'`app.html` | utilisateur |
| `fc26b23` | `app.html` : suppression de créneau en attente visible (barrée, cadre ambre) | banc (faux serveur) |
| `065fd0d` | « Si » et « En parallèle » en forme de pince (le losange devient la mâchoire du haut) | banc ; déposé sur `.141`, **pas encore vu par l'utilisateur** |
| `b6d667c` | `app.html` : « effacement en attente », nom barré | banc |
| `283993e` | **Firmware** CloudSync : `TLS_HANDSHAKE_TIMEOUT_MS` = 8 s (TCP 4 s inchangé), `PHASE_HARD_DEADLINE_MS` 25 → 30 s | `.141` sous charge : 2/2 cycles OK (4 501 / 4 391 ms, qui échouaient avant) |
| `20385e6` | `app.html` : la sauvegarde dit ce qui a changé par rapport à la précédente (zones, scripts, notifications, réglages) et liste les scripts | banc |
| `54766cd` | `VERSION` = `5.10.0-dev` | `.141` : `/api/diagnostics.build.version` = `5.10.0-dev`, build 1252 |
| `fade726` | `AGENTS.md` : chaîne `ProgrammeArrosage_s3`, `--dtr 1 --rts 0`, `announce_reboot.py`, section Legacy/V4 remplacée ; `REPRISE_INGENIEUR.md` annoté | — |
| `84a319d`, `e6ec400` | Registre de soak (fenêtres de maintenance des flashes) | — |

## 4. Fichiers et fonctions modifiés

| Fichier | Éléments |
|---|---|
| `src/CloudSync.cpp`, `src/CloudSync.h` | `TLS_HANDSHAKE_TIMEOUT_MS`, `setHandshakeTimeout`, `PHASE_HARD_DEADLINE_MS` |
| `data/scripts-schema.html` | barre du haut (`.nomwrap`, `#dirty`), `majModule` (gamme), `nodeEl` (pinces `si`/`par`/`boucle`), `subOf('si')`, CSS du losange retirée |
| `data/style-base.css` | `.btn.fermer`, `.btn-full.fermer` |
| `data/index.html`, `data/app.js` | bouton « Fermer » de la modale ; pastille « Pro » |
| `data/urls.html`, `data/script-lang.js` | renvois à `scripts.html` retirés ; `data/scripts.html` supprimé |
| `cloud/php-mutualized/app.html` | `button.fermer`, `celluleJour` (`.cr.sup`), `rendreScripts`, `resumeScriptsSauvegarde`, `diffSauvegardes`, `ouvrirSauvegarde` |
| `VERSION`, `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`, `ROADMAP.md`, `tools/soak/ledger.json` | version, gouvernance, état de la TODO |

Volontairement non modifiés : `littlefs/`, routes et IDs existants (ajouts
seulement), modèle et bytecode des scripts, PHP serveur.

## 5. Invariants préservés

- Rien ne se lance à distance ; le module revalide tout (D014).
- Compilateur et VM des scripts inchangés : l'éditeur ne change que le rendu.
- Durée maximale de sécurité des relais intacte ; aucun relais commandé.
- Version issue de la seule source `VERSION` (identité vérifiée sur carte).
- Restaurer une sauvegarde ne renvoie toujours que les créneaux (dit en clair).

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (build 1252, `5.10.0-dev`) |
| buildfs | non requis (`littlefs/` inchangé) |
| `.141` | flashé `5.10.0-dev` build 1252 ; SD : pages Web du jour déposées et vérifiées par SHA-256 |
| CloudSync | 2/2 cycles OK sous charge Web après le passage à 8 s ; durée longue à confirmer |
| Contrats de sécurité (local) | 7 tests OK (2 échecs attendus = trous documentés) |
| CI / PR #29 | 3/3 verts sur `fade726` : build `ProgrammeArrosage` (CYD) 2 min 08, build `ProgrammeArrosage_s3` 2 min 38, `contracts` 6 s |
| `main` | avancé en avance rapide sur le commit de ce document (SHA conservés) ; PR #29 (et #28) marquées fusionnées par GitHub |
| AlwaysData | **rien de ce jour n'est en ligne** : dépôt FTP à faire (§8, point 1) |
| LCD | non concerné |

## 7. Risques et limites

- CloudSync : 8 s validé sur 2 cycles seulement ; surveiller le taux de
  réussite sur plusieurs heures. La contention du cœur 1 pendant l'activité
  Web reste la cause de fond (non traitée).
- Pinces « Si »/« En parallèle » : choix de dessin fait sans l'utilisateur ;
  `git revert 065fd0d` si le losange est préféré.
- `compareVersions()` ignore le suffixe : `5.10.0-dev` = `5.10.0` pour le
  module. Le prochain paquet Web devra être > `5.10.0` (ex. `5.10.1`).
- L'ancien `scripts.html` reste sur la SD des modules (aucun lien n'y mène).
- CI GitHub : la PR #28 n'a jamais tourné (« job was not acquired by
  Runner ») ; `ubuntu-latest` migre vers Ubuntu 26 le 19 oct. 2026 ;
  `materialize-run6-22.yml` est un workflow mort.
- Barre de l'éditeur : passe à la ligne sous ~1 200 px.
- Pas de signature de bout en bout (D014) ; interface module sans
  authentification (trou assumé).
- `git diff --check` signale de faux espaces sur les sources CRLF.

## 8. TODO — reste à faire

**À faire par l'utilisateur (FTP, hors dépôt)**

1. Déposer sur AlwaysData :
   - `cloud/php-mutualized/app.html` → racine ;
   - `data/scripts-schema.html` et `data/style-base.css` → `/editeur/` ;
   - paquet Web `5.10.0` : d'abord `dist/alwaysdata/web/v5.10.0/` →
     `/web/v5.10.0/`, **puis seulement** `dist/alwaysdata/aqualook-web-manifest.json`
     → racine ; ensuite `python tools/check_published_web.py`.
2. Valider à l'œil les pinces « Si » / « En parallèle » sur `.141`, puis en
   ligne : grille (suppression barrée), tableau des scripts (« effacement en
   attente »), fenêtre de sauvegarde (différences, scripts).

**Nouvelles demandes (8 oct. 2026)**

3. **Écran « À propos » sur le LCD** : compte rendu des versions installées
   (firmware : version, build, SHA, branche, date ; ressources Web installées
   et disponibles ; gamme), consultable directement sur l'écran.
4. **Mêmes informations dans la page Santé du Web** (`sante.html`), lisibles
   sans passer en administrateur. Source unique : `/api/diagnostics.build`
   et la version des ressources Web (`assets-version.json`).

**Cloud et firmware**

5. Prouver avec le jeton admin **de production** : commande > 4 Ko, refus
   d'un script en cours, effacement et phrases distants
   (`tools/cloud_test_scripts_apply.py`).
6. CloudSync : confirmer le passage à 8 s sur plusieurs heures ; à terme,
   réduire la contention du cœur 1 pendant l'activité Web.

**Gouvernance et CI**

7. Vérifier que GitHub a bien marqué #28 et #29 comme fusionnées ; supprimer
   les branches intermédiaires de la pile une fois archivées (tags `archive/`).
8. CI : retirer `materialize-run6-22.yml`, vérifier les workflows après la
   migration Ubuntu 26 (19 oct.), comprendre les runners non attribués.
9. Mettre à jour `docs/codex/00_CONTEXT.md` et `05_BUILD_AND_TEST.md`, encore
   rédigés pour la chaîne Legacy/V4 (CYD).

## 9. Procédure exacte de reprise

1. Lire `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`, ce document.
2. Se placer sur `main`, puis créer une branche pour la suite :

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -3
git status --short
```

3. Demander le port COM courant, puis :

```powershell
git diff --check
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

4. Banc logique de l'éditeur :

```powershell
& "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe" --headless --disable-gpu --allow-file-access-from-files --dump-dom "file:///<chemin-du-depot>/tools/script_schema_test.html"
```

5. Dépôt d'une page sur la SD de `.141` sans flash (vérifier `bootGuard` dans
   `/api/adminStatus`) : `POST /api/debug/deploy-begin`, puis
   `POST /api/debug/deploy-file?name=<fichier>` (corps binaire), puis
   `POST /api/debug/deploy-commit` ; relire et comparer le SHA-256.
6. Bancs visuels jetables (non versionnés) utilisés ce jour : copie de la page
   dans un dossier temporaire avec `fetch` remplacé par de fausses réponses,
   rendue par Edge sans fenêtre (`--screenshot` / `--dump-dom`).

## 10. Commandes Git utiles

```powershell
git log --oneline cdb21cf..HEAD        # la session du 7-8 octobre
git show 283993e                       # seul commit firmware (CloudSync TLS)
git revert 065fd0d                     # si les pinces Si/Parallele ne conviennent pas
gh pr view 29                          # PR de fusion vers main
```
