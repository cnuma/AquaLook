# Checkpoint AquaLook — 7 octobre 2026 (soir) — éditeur graphique des scripts : TODO terminée

Document de reprise autonome. Avec `AGENTS.md` et `docs/REPRISE_INGENIEUR.md`,
il suffit pour reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-07_fb275e2.md` (matin) ; le détail du chantier cloud D014
reste dans `CHECKPOINT_2026-10-06_e4a7456.md`.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public) |
| Branche de travail | `feature/cloud-scripts` (poussée sur `origin`) |
| Commit fonctionnel | `7d273fd` (dernier commit de code Web : `1df97ea` ; dernier commit firmware : `99f21eb`) |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-07_7d273fd.md`) |
| Version fonctionnelle (`VERSION`) | `5.9.7` (inchangée ; `5.10.0-dev` proposée, à décider) |
| Dernier numéro de réponse du chat source | `AQL-R008` |
| Cible de production | `ProgrammeArrosage_s3`, module de test `192.168.1.141`, COM4 (confirmé le 7 oct. 2026) |

Pile de branches (aucune fusionnée) :
`main` → `feat/moteur-de-regles` → `feature/editeur-graphique-scripts` →
`feature/scripts-paralleles` → **`feature/cloud-scripts`**.

## 2. Source de vérité

La branche `feature/cloud-scripts` sur GitHub, au commit de ce document.
Ne pas repartir de `main` ni d'une branche inférieure de la pile.

## 3. Ce qui a été fait et validé (7 oct. 2026)

Les 8 points de la TODO « Éditeur graphique des scripts : améliorations
demandées » (`ROADMAP.md`) sont faits. L'utilisateur a validé les points 1 à
5 et 8 sur `.141` (« nickel pour tout »), puis le point 7 (« nickel »). Le
point 6 (« Modifier le texte ») est déposé sur `.141` mais **pas encore vu
par l'utilisateur**.

| Commit | Point | Contenu |
|---|---|---|
| `a810871` | 2, 3 | Fenêtre « Phrases » centrée ; « Fermer » bleu sur la rangée du bouton principal (classe `.btn.fermer`). Cause : la classe locale `.modal` héritait du `.modal` de `style-base.css` (max-width 440 px) ; renommée `.voile`. |
| `866dd70` | 4 | La liste déroulante suit le nom saisi (local et en ligne, nom de la commande en attente compris). |
| `a752a9f` | — | Registre de soak : fenêtre de maintenance du flash. |
| `99f21eb` | 1, 5 | **Firmware** : `/api/scripts` et le miroir CloudSync exposent `notifications` et `productLine` ; `POST /api/notifications/config` appelle `noteExternalChange()` (sinon le miroir n'était jamais renvoyé). **Éditeur** : avertissement sur les blocs « Notifier » quand les notifications sont coupées, pastille de gamme, champ nom flexible. |
| `a7bb5e7` | 8 | « et » / « ou » entre chaque condition, « et » avant « ou » (modèle `cond = { terms, ops }`) ; sélecteurs, ligne « Lu comme », glisser-déposer et ↑. |
| `1335f45` | 7 | Boucles « Répéter » et « Tant que » en forme de pince (Scratch). |
| `97a4271` | — | Croix rouge pour retirer une condition (demande en cours de route). |
| `1df97ea` | 6 | « Modifier le texte » : texte compilé puis relu en schéma ; un texte non dessinable est gardé tel quel (mode texte, Départ et Actif réglables, Enregistrer envoie ce texte), en local et en ligne. |
| `122da22`, `88611e2`, `7d273fd` | — | `ROADMAP.md` : état de la TODO. |

## 4. Fichiers et fonctions modifiés

| Fichier | Fonctions / éléments |
|---|---|
| `src/WebManager.cpp` | route `GET /api/scripts` : champs `notifications`, `productLine` |
| `src/WebManager.h` | `POST /api/notifications/config` : capture `this`, `_config->noteExternalChange()` |
| `src/CloudSync.cpp` | `buildScriptsPayload()` : champs `notifications`, `productLine` |
| `data/script-schema.js` | `make()` (`cond.ops`), `condGroups`, `condText`, `condEval`, relecture `orGroups`/`andGroups`/`unaryGroups`/`condition` |
| `data/scripts-schema.html` | `.voile`, `.btn.fermer`, `libelleSlot`/`rendreSlots`/`majNomSlot`/`vueSlot`/`suffixeSlot`, `majModule`, `warnings`, `condFields`/`moveTerm`/`wireProps`, `.pince*` dans `nodeEl`, fenêtre `#texte` (`ouvrirTexte`, `majEtatTexte`, `appliquerTexte`, `selectionnerLigne`), `texteAEnvoyer`, `modeTexte`, garde de `essai()` |
| `tools/script_schema_test.html` | modèle `ops`, cas et/ou mélangés, table de vérité (80 cas), refus « ou dans et » et « non groupe » |
| `ROADMAP.md`, `tools/soak/ledger.json` | état de la TODO, fenêtre de maintenance |

Volontairement non modifiés : `scripts.html` (toujours en place), `index.html`,
`urls.html`, `app.html`, `style-base.css`, `littlefs/`.

## 5. Invariants préservés

- Rien ne se lance à distance ; seul `config.apply` est accepté du cloud ; le
  module revalide tout bytecode (D014).
- Le compilateur et la VM sont inchangés : l'éditeur ne produit que du texte
  du langage existant. Un script à opérateur unique garde exactement le même
  texte et le même bytecode.
- La relecture texte → schéma refuse ce qu'elle ne sait pas dessiner
  exactement (« ou » entre parenthèses dans un « et », « non » devant un
  groupe, variables, calculs) ; un texte non dessinable est gardé, jamais
  traduit « à peu près ».
- Durée maximale de sécurité des relais intacte ; aucun relais commandé.
- IDs HTML et routes existants conservés (ajouts seulement).

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | **SUCCESS** (RAM 25,8 %, flash 76,6 %) sur les sources de `99f21eb` ; aucun fichier `src/`, `include/` ou `platformio.ini` modifié depuis |
| buildfs | non requis (`littlefs/` inchangé depuis `e0437e6`) |
| `.141` (COM4) | flashé le 7 oct. 2026 ~18 h 08 ; l'identité affiche `866dd70` (build lancé avant le commit `99f21eb`, sources identiques à celui-ci) ; SD : `scripts-schema.html` (`e094ed21…`) et `script-schema.js` (`9175b45e…`) déposés, empreintes vérifiées |
| Vérifié sur `.141` | `/api/scripts` → `notifications=false` (réellement coupées) et `productLine="Aqualook Pro"` ; réenregistrement des notifications → miroir reconstruit 10 s après ; 3/3 synchros OK au repos (révision 56) |
| Bancs | `tools/script_schema_test.html` TOUT OK ; banc de page (Edge sans fenêtre, faux module et faux espace en ligne) 68 contrôles OK — banc jetable, non versionné |
| AlwaysData `/editeur/` | à charge de l'utilisateur (FTP) : dernière version = `data/scripts-schema.html` de `1df97ea` + `data/script-schema.js` de `a7bb5e7` ; dépôt de la dernière version non confirmé |
| LCD | non concerné |
| Arbre Git | propre au moment de ce document |

## 7. Risques et limites

- **CloudSync au bord du délai TLS** : les connexions réussies prennent
  3 970-4 070 ms, les échecs abandonnent vers 4 450-4 900 ms. Sous activité
  Web (interrogation de `/api/logs.txt` toutes les 4-5 s pendant les tests),
  4 cycles sur 5 ont échoué ; au repos, 3/3 réussis. Fragilité connue
  (`REPRISE_INGENIEUR.md` §8.2) dont la marge mérite un chantier.
- **Barre du haut de l'éditeur** : à 1 400 px, elle passe sur deux lignes dès
  que « non enregistré » s'affiche (déjà le cas avant la pastille de gamme) ;
  le champ nom ne fait plus que 130 px.
- **Mode texte** : l'historique d'annulation repart de zéro à l'entrée ou à la
  sortie du mode texte (choix assumé, commentaire dans `appliquerTexte`).
- **Non prouvé sur carte** (inchangé) : commande > 4 Ko, refus d'un script en
  cours, effacement distant, phrases distantes ; le banc
  `tools/cloud_test_scripts_apply.py` demande le jeton admin **de production**.
- Pas de signature de bout en bout (D014).
- Toute modification de `data/scripts-schema.html` ou `data/script-schema.js`
  se recopie à deux endroits : la SD du module et `/editeur/` sur AlwaysData.
- `dist/alwaysdata` (5.9.49) est périmé.
- `git diff --check` signale de faux espaces en fin de ligne sur les sources
  CRLF.

## 8. TODO — reste à faire

**Éditeur graphique et interface Web**

1. **Décider le retrait de `scripts.html`** : le point 6 le rend possible.
   Retirer aussi le bouton « Éditeur texte » et le lien « Ouvrir dans
   l'éditeur texte » de `scripts-schema.html`, et les liens d'`index.html`
   et d'`urls.html` (libère la barre du haut de l'éditeur).
2. **Passe « Fermer » bleu sur toute l'interface** (règle du 7 oct. 2026) :
   `app.html` et ses fenêtres de détail, `index.html` et ses modales,
   `scripts.html` s'il reste, autres pages. Classe de référence
   `.btn.fermer` (aujourd'hui locale à `scripts-schema.html`, à déplacer
   dans `style-base.css`).
3. Optionnel : « Si » et « En parallèle » en forme de pince, comme les
   boucles.
4. Barre du haut de l'éditeur : ne plus passer à la ligne avec « non
   enregistré » (voir §7).

**Espace en ligne**

5. Dans la grille de `app.html`, un créneau supprimé en attente de
   synchronisation reste invisible.

**Cloud et firmware**

6. Prouver la commande > 4 Ko, le refus d'un script en cours, l'effacement
   et les phrases distants, avec le jeton admin de production.
7. CloudSync : élargir la marge du délai de connexion TLS, ou réduire la
   contention du cœur 1 pendant l'activité Web (§7).

**Publication et gouvernance**

8. Régénérer et publier les ressources Web (`tools/publish_web_assets.py`,
   `dist/alwaysdata` périmé), après décision de la version.
9. Décider la version (`5.10.0-dev` proposée).
10. Fusion de la pile vers `main` après validation (PR pour lancer la CI).
11. Mettre `AGENTS.md` en cohérence avec la chaîne de build réelle
    (`ProgrammeArrosage_s3`, moniteur `--dtr 1 --rts 0`) — proposition dans
    `REPRISE_INGENIEUR.md` §11.

## 9. Procédure exacte de reprise

1. Lire `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`, ce document.
2. Se placer sur la branche :

```powershell
git fetch origin
git checkout feature/cloud-scripts
git pull --ff-only
git log --oneline -3
git status --short
```

3. Demander le port COM courant, puis (chaîne S3) :

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

5. Dépôt d'une page sur la SD de `.141` sans flash (vérifier d'abord
   `bootGuard` dans `/api/adminStatus`) : `POST /api/debug/deploy-begin`,
   puis `POST /api/debug/deploy-file?name=<fichier>` (corps binaire) par
   fichier, puis `POST /api/debug/deploy-commit` ; relire la page et comparer
   le SHA-256.

## 10. Commandes Git utiles

```powershell
git log --oneline e0437e6..HEAD      # la journée du 7 octobre
git show 99f21eb --stat              # seul commit firmware du jour
git diff e0437e6..HEAD -- data/      # éditeur graphique
```
