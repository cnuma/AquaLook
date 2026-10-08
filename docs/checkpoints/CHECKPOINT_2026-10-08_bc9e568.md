# Checkpoint AquaLook — 8 octobre 2026 (fin de soirée) — variables dans l'espace en ligne, banc de l'éditeur

Document de reprise autonome. Avec `AGENTS.md` et `docs/REPRISE_INGENIEUR.md`,
il suffit pour reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-08_11322d5.md` (nuit), dont la TODO §8 est traitée ici
pour les points 1 à 3.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public) |
| Branche | `main` (seule branche restante, locale et distante) |
| Commit fonctionnel | `bc9e568` (firmware flashé) ; `main` = `d79609f` avant ce document (registre de soak) |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-08_bc9e568.md`) |
| Version fonctionnelle (`VERSION`) | `5.11.0-dev` (inchangée) |
| Firmware sur `.141` | `5.11.0-dev`, build **1294**, `bc9e568`, branche de build `feature/variables-en-ligne` (contenu identique à `main`) |
| Dernier numéro de réponse du chat source | `AQL-R009` |
| Cible | `ProgrammeArrosage_s3`, module de test `192.168.1.141`, **COM4** (confirmé le 8 oct. 2026, à reconfirmer) |
| Matériel `.141` | ESP32-S3 N4R8 : 4 Mo de flash, 8 Mo de PSRAM |

## 2. Source de vérité

`main` au commit de ce document. Les nouveaux travaux repartent de `main` sur
une branche dédiée. Toutes les anciennes branches sont archivées sous
`archive/<nom>` (récupération : `git checkout -b <nom> archive/<nom>`).

## 3. Ce qui a été fait (8 oct. 2026, soir)

| Commit | Contenu | Validation |
|---|---|---|
| — | Vérification de la publication : paquet Web `5.10.3` en ligne (`check_published_web.py` 18/18) ; `/editeur/` identique à `main` | lecture seule, sans secret |
| `7aa5dce` | `tools/editeur_banc.py` : banc de l'éditeur sans module (Edge sans affichage, `fetch` simulé) | 25/25 ; détecte une régression (sortie 1) |
| — | Ménage Git : 13 branches fusionnées archivées (`archive/<nom>`, tags poussés et vérifiés) puis supprimées en local et sur GitHub | `git branch -a` = `main` seul |
| `84fcddf` | Firmware : `scripts.variables` `[{i, nom, valeur}]` dans le miroir CloudSync (absent en maintenance : `ScriptGlobals::started()`) ; enregistrer les noms renvoie le miroir. Éditeur en ligne : onglet Variables en lecture seule, masqué pour un firmware antérieur | `.141` : miroir 6280 → 6289 octets après renommage de g2, `config=ok` |
| `ae5e9bc` | Banc : scénarios « espace en ligne » et « firmware antérieur » | 12/12 et 5/5 ; la page de `main` d'avant échoue |
| `1e0dbff` | Firmware : rapport `diag` de chaque cycle avec `variables: {crc, valeurs[16]}` **seulement** si le CRC32 des valeurs diffère du dernier envoi confirmé (2xx) ; toujours au 1er cycle après démarrage ; `ScriptGlobals::snapshot()` | `.141` : 1er cycle `valeurs remontees (crc 3fa313c8)`, cycle suivant rien, g2 changée → `crc bba68618` |
| `6df198d` | Serveur : `merge_module_variables()` (auth.php) appelée par index.php sur un `diag` portant `variables` ; ne touche ni révision, ni `updated_at`, ni sauvegardes ; pose `scripts.variablesSuivies` (conservé par `store_module_config` si le miroir porte les variables). Éditeur : « Valeur au <dernier contact> » si suivies, sinon date du miroir | `php -l` ; base simulée 8/8 ; **vue en ligne confirmée par l'utilisateur** |
| `41ef2b1` | Banc : scénario « valeurs suivies » | 13/13 |
| `2a36741` | D015 mise à jour (remontée en lecture seule, CRC, fusion serveur) | — |
| `bc9e568` | Fix : enregistrer l'onglet Variables sans changer de nom ne réécrit plus les noms en NVS ni ne renvoie le miroir (« [GVAR] noms inchanges ») | flashé, démarrage et remontée OK ; **chemin « noms inchangés » non testé** (écriture signée) |
| `7bdf79a`, `d79609f` | Registre de soak (`tools/soak/ledger.json`) | — |

## 4. Fichiers et fonctions modifiés

| Fichier | Éléments |
|---|---|
| `src/ScriptGlobals.h/.cpp` | `started()`, `snapshot()` (copie sous verrou + CRC32), `g_started` |
| `src/CloudSync.cpp` | `buildScriptsPayload` (bloc `variables`), `CloudSync::run` (télémétrie `variables`, `g_varsCrcConfirmed`, `g_varsCrc`) |
| `src/WebManager.cpp` | `handleSaveScriptGlobals` : comparaison des noms, `noteExternalChange()` si changés |
| `cloud/php-mutualized/auth.php` | `merge_module_variables()` ; `store_module_config()` (report de `variablesSuivies`) |
| `cloud/php-mutualized/index.php` | `/v1/report` : appel de `merge_module_variables` sur `diag` |
| `data/scripts-schema.html` | CSS `body.cloud.sansvars #t-var`, lecture seule en ligne ; `#gv-releve` ; `lireGlobales` (miroir en ligne) ; `lireModule` en ligne (variables, date, titres) ; `rendreVariables` (`readonly`) |
| `tools/editeur_banc.py`, `tools/editeur_banc/*.js` | banc : 4 scénarios (local, en ligne, valeurs suivies, firmware antérieur) |
| `docs/codex/02_DECISIONS.md` | D015 |

Volontairement non modifiés : `littlefs/` (pas de buildfs), routes, IDs
existants (ajout de `gv-releve` seulement), format NVS (`aqlvars` inchangé),
`VERSION`.

## 5. Invariants préservés

- Le module reste seul juge ; l'espace en ligne ne modifie pas les variables
  (lecture seule, hors périmètre D015).
- Une valeur qui bouge ne fait monter ni `configRevision` ni le nombre de
  sauvegardes serveur ; usure NVS inchangée (écriture au plus 1/min).
- Mode maintenance : aucune variable envoyée (évite d'écraser les noms du
  miroir par des valeurs non relues).
- Rétrocompatibilité : firmware antérieur → onglet masqué en ligne ; PHP
  antérieur → les valeurs `diag` vont seulement dans l'historique, l'éditeur
  date les valeurs du miroir (pas de fraîcheur annoncée à tort).
- Identité de build `.141` vérifiée : `5.11.0-dev`, 1294, `bc9e568`.

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (RAM 25,9 %, Flash 77,2 %) |
| buildfs | non requis (`littlefs/` inchangé) |
| `.141` | flashé `bc9e568` (COM4) ; `[GVAR] variables relues : valeurs ok, noms ok` ; autotest scripts OK ; CloudSync `rapport=ok config=ok` |
| SD `.141` | `scripts-schema.html` de `main` déposé (SHA-256 identique) |
| AlwaysData | `auth.php`, `index.php`, `/editeur/scripts-schema.html` publiés par l'utilisateur ; vue en ligne confirmée (g1, g2 en lecture seule) |
| Paquet Web | `5.10.3` en ligne ; `scripts-schema.html` a changé depuis (mode en ligne seulement) : paquet `5.10.4` **non préparé** |
| Banc éditeur | `python tools/editeur_banc.py` : 25 + 13 + 13 + 5, TOUT OK |
| LCD | non concerné |

## 7. Risques et limites

- **Chemin « noms inchangés » (`bc9e568`) non testé sur la carte** : fixer une
  seule valeur depuis l'éditeur local doit journaliser `[GVAR] noms inchanges`
  et **ne pas** produire de ligne `miroir … octets` ; la valeur doit partir au
  cycle suivant par `[GVAR] valeurs remontees`.
- Un compteur qui change à chaque cycle ajoute ~150 octets à chaque rapport
  `diag` (historique `module_message`) ; aucune connexion de plus.
- `g_varsCrcConfirmed` est en RAM : chaque démarrage renvoie les valeurs une
  fois (voulu).
- `variablesSuivies` est reporté d'un miroir à l'autre : un retour à un
  firmware qui met les variables dans le miroir mais pas dans `diag`
  (`84fcddf` seul) laisserait l'éditeur dater à tort du dernier contact. Cas
  de laboratoire.
- La fusion serveur n'a été testée que sur base simulée et par la vue en
  ligne ; pas de test d'accès concurrent réel.
- Restent valables : risques `11322d5` §7 et `1534dde` §7 (défaut
  `'abcdefgh'.indexOf` de `script-lang.js`, etc.).

## 8. TODO — reste à faire

1. Tester le chemin « noms inchangés » (§7, premier point).
2. Préparer et publier le paquet Web `5.10.4` si souhaité
   (`python tools/publish_web_assets.py 5.10.4 --comparer`, fichiers puis
   manifeste, puis `check_published_web.py`).
3. Documentation codex périmée : `docs/codex/00_CONTEXT.md`,
   `05_BUILD_AND_TEST.md` (encore Legacy/V4 CYD) et
   `docs/REPRISE_INGENIEUR.md` §1.1 (dit que tout vit sur
   `feat/moteur-de-regles`, branche désormais archivée).
4. Corriger le test des variables locales `a..h` dans `script-lang.js`
   (`'abcdefgh'.indexOf(mot)` accepte `ab`, `gh`…).
5. Reste de la TODO `fade726` §8 (jeton admin de production, CloudSync 8 s
   sur plusieurs heures, CI, migration Ubuntu 26 le 19 oct.).

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

Pour l'éditeur : `python tools/editeur_banc.py` (`-v` pour tout voir). Pour
une page Web seule, dépôt direct sur la SD de `.141` (autorisé d'office) :
`POST /api/debug/deploy-begin`, `POST /api/debug/deploy-file?name=<fichier>`
(corps binaire), `POST /api/debug/deploy-commit`, puis comparer le SHA-256 de
`GET /<fichier>`.

Contrôles rapides sans secret : `GET /api/diagnostics` (champ `build`),
`GET /api/debug/script-selftest`, `GET /api/script-globals`,
`GET /api/logs.txt` (lignes `[GVAR]`, `CloudSync: cycle`).

Piège : un `git diff --check` simple signale à tort les CR des fichiers CRLF ;
utiliser `git -c core.whitespace=cr-at-eol diff --check`.

## 10. Commandes Git utiles

```powershell
git log --oneline 4512abb..HEAD      # la soirée du 8 octobre
git show 1e0dbff                     # CRC des valeurs dans la télémétrie
git show 6df198d                     # fusion serveur + date du dernier contact
git show bc9e568                     # noms inchangés : pas de renvoi du miroir
git tag -l 'archive/*'               # branches archivées
```
