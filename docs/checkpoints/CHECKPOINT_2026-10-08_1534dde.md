# Checkpoint AquaLook — 8 octobre 2026 (soir) — scripts Et/Ou, variables globales, journal lisible

Document de reprise autonome. Avec `AGENTS.md` et `docs/REPRISE_INGENIEUR.md`,
il suffit pour reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-08_f172172.md` (matin) ; la TODO de
`CHECKPOINT_2026-10-08_fade726.md` §8 reste la référence pour ce qui n'est pas
traité ici.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public) |
| Branche | `main` (toutes les branches de la session y sont fusionnées en avance rapide) |
| Commit fonctionnel | `1534dde` |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-08_1534dde.md`) |
| Version fonctionnelle (`VERSION`) | `5.11.0-dev` (passée de `5.10.0-dev` ce jour) |
| Dernier numéro de réponse du chat source | `AQL-R016` |
| Cible | `ProgrammeArrosage_s3`, module de test `192.168.1.141`, **COM4** (confirmé le 8 oct. 2026) |
| Matériel `.141` | ESP32-S3 **N4R8** : 4 Mo de flash (Boya 0x68/0x4016), 8 Mo de PSRAM — vérifié par `esptool flash_id` le 8 oct. 2026 |

## 2. Source de vérité

`main` au commit de ce document. Les nouveaux travaux repartent de `main` sur
une branche dédiée.

## 3. Ce qui a été fait (8 oct. 2026, journée)

| Commit | Contenu | Validation |
|---|---|---|
| `677ae3b` | Pied de page de l'index : plus de « synchronisées le undefined » (3 formats du marqueur `assets-version.json`) | `.141` (rendu réel) |
| `8a36338` | `VERSION` → `5.11.0-dev` | identité lue sur `.141` |
| `5bbc47a` | CloudSync : `cloudSync.resultSinceBoot` ; `cloudsync.html` et badge d'accueil disent « en attente du 1er cycle » après un redémarrage au lieu d'un faux « Échec » | `.141` : `false` au boot, `true` au 1er cycle (réussi, rév. 71) ; page réelle vue après reboot |
| `3c65dca` | Éditeur : pinces d'une seule couleur (sélection/arrêt d'un tenant) ; « En parallèle » sans pince | banc visuel Edge, puis utilisateur |
| `33fc1e1` | « En parallèle » **en Et / en Ou** : opcode `JOINANY` (87), générations de bloc, `JOIN` avant le `HALT` final | autotests VM ; **testé par l'utilisateur sur `.141` (« tout marche »)** |
| `71c120e` | **Variables globales g1..g16** — firmware : `ScriptGlobals` (NVS `aqlvars`), `GLOAD`/`GSTORE` (5/6), routes `GET/POST /api/script-globals` | autotest ; persistance constatée sur `.141` au fil des flashes |
| `2c325f2` | Variables — langage et éditeur : bloc « Variable », condition « une variable », décision **D015** | banc logique ; **utilisateur sur `.141`** |
| `8f7ce05` | **Journal lisible** : cache RAM (PSRAM) des phrases ; « Noter au journal » écrit la phrase ; `NOTIFY_VAR` (88) = phrase + « nom = valeur » | autotest ; **utilisateur sur `.141`** |
| `b931d79` | Zone du bas en deux (texte à gauche ; onglets Essai / Phrases / Variables à droite) ; bouton Enregistrer orange quand le script est modifié | banc visuel ; utilisateur |
| `1534dde` | « Notifier sur GSM » (renommage) avec variable facultative : `ALERT_VAR` (89) ; zone du bas repliable (▾/▴) | build ; autotest 21/21 sur `.141` ; banc visuel |
| `3b8d8bf`, `ba53548`, `9629e83`, `4493258` | registre de soak (fenêtres de maintenance des flashes) | — |

Publication : paquet Web **`5.10.1`** publié sur AlwaysData et installé sur
`.141` (`check_published_web.py` 18/18).

## 4. Fichiers et fonctions modifiés

| Fichier | Éléments |
|---|---|
| `src/domain/ScriptVm.h/.cpp` | opcodes `GLOAD` 5, `GSTORE` 6, `JOINANY` 87, `NOTIFY_VAR` 88, `ALERT_VAR` 89 ; `ScriptHostOps::joinAny/globalGet/globalSet/notifyVar/alertVar` ; `FORK` accepté rend la main ; `_inlineDone` ; validateur |
| `src/ScriptGlobals.h/.cpp` (nouveaux) | valeurs + noms en NVS (`aqlvars`, clés `v`/`n`, blocs versionnés + CRC), écriture regroupée ≤ 1/min (`update`), `flush` |
| `src/ScriptHostRuntime.h/.cpp` | `joinAny`, générations (`branchGen`, `genDone`), `globalGet/Set`, `notify` (phrase), `notifyVar`, `alertVar` |
| `src/ScriptRunner.h/.cpp` | `Job::gen`, fin de branche → `genDone`, branche non démarrée → `genDone` |
| `src/ScriptMessageCatalogue.h/.cpp` | cache RAM (PSRAM) + mutex, rechargé après `store()` ; `phrase()` sans lecture SD |
| `src/NotificationManager.h/.cpp` | `enqueueScriptMessage(code, nom, extra)` |
| `src/BootLoopGuard.cpp` | `ScriptGlobals::flush()` dans `restartDeliberately` |
| `src/main.cpp` | `ScriptGlobals::begin()` avant `scriptRunner.begin`, `ScriptGlobals::update()` dans la boucle |
| `src/WebManager.h/.cpp` | `GET/POST /api/script-globals` (POST signé HMAC) ; `cloudSync.resultSinceBoot` |
| `src/CloudSync.h/.cpp` | `resultSinceBoot()` |
| `src/ScriptVmSelfTest.cpp` | cas 11d–11h (Ou, variables, journal/notification avec variable) — 21 cas |
| `data/script-lang.js` | `parallele [et|ou]`, `g1..g16`, `message/notifier <code> avec gN` |
| `data/script-schema.js` | modèle : `mode` du parallèle, bloc `variable`, terme `globale`, `gvar` |
| `data/scripts-schema.html` | pinces, parallèle, réglages Et/Ou, bloc Variable, onglets, Enregistrer orange, repli |
| `data/cloudsync.html`, `data/app.js` | affichage après redémarrage ; pied de page |
| `tools/script_schema_test.html` | banc logique (≈ 70 cas ajoutés) |
| `tools/log_messages.tsv` | explication de `SCRIPT-NOTIFY` |
| `docs/codex/02_DECISIONS.md` | **D015** — variables globales et parallèle Et/Ou |

Volontairement non modifiés : `littlefs/` (pas de buildfs), table de
partitions, NVS de configuration (`aqualook`/`config`), routes existantes.

## 5. Invariants préservés

- Sécurité relais : aucune instruction de script ne lève la durée maximale ;
  les tests matériels ont été faits par l'utilisateur, une zone, durée courte.
- Persistance : les variables vivent dans un espace NVS séparé, versionné,
  avec CRC ; un bloc illisible donne des valeurs à 0, jamais d'échec de boot.
- Compatibilité : un script sans les nouveautés compile octet pour octet comme
  avant ; un firmware antérieur refuse les opcodes 5/6/87/88/89 à
  l'enregistrement (refus net, jamais d'exécution de travers).
- Identité de build (F12–F14) : `.141` affiche `5.11.0-dev`, build 1278,
  `1534dde`, `main`.

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (RAM 25,9 %, Flash 77,1 %) |
| Flash `.141` | `main` `1534dde`, build 1278, compilé 20:37:17 ; heap ~172 Ko |
| Autotest VM (`/api/debug/script-selftest`) | **21/21** |
| buildfs | non requis (`littlefs/` inchangé depuis `6f12f1e`) |
| SD `.141` | `scripts-schema.html`, `script-schema.js`, `script-lang.js`, `cloudsync.html`, `app.js` de `main` déposés (SHA-256 identiques) ; marqueur « dépôt direct » |
| Variables `.141` | `g1` « Etat de la pompe » = 3 (laissée par les tests utilisateur) |
| AlwaysData | paquet Web `5.10.1` en ligne ; **`5.10.2` préparé, NON publié** |
| CYD | non compilée (abandon en cours) |

## 7. Risques et limites

- **Paquet Web 5.10.2 et `/editeur/` non publiés** : l'éditeur en ligne et les
  autres modules n'ont pas les nouveautés (voir §8).
- **Espace en ligne et variables** : pas de remontée ni de modification des
  variables par CloudSync ; l'éditeur en ligne affiche `gN` sans nom et masque
  l'onglet Variables (D015).
- **Éditeur en ligne vs firmware** : un script utilisant Et/Ou ou les
  variables, enregistré par config.apply vers un module ancien, sera refusé
  par ce module (comportement voulu, mais à expliquer à l'utilisateur).
- **Ou** occupe 3 des 4 places de scripts ; faute de place, les branches
  s'enchaînent (journal).
- **Variables** : une coupure de courant peut perdre la dernière minute.
- **Défaut existant repéré, non corrigé** : `script-lang.js` teste les
  variables locales par `'abcdefgh'.indexOf(mot)` ; un mot comme `ab` ou `gh`
  passerait pour une variable.
- `git diff --check` signale à tort les CR des fichiers CRLF
  (`NotificationManager.cpp`) : utiliser `git -c core.whitespace=cr-at-eol diff --check`.
- Branches de travail fusionnées mais non supprimées (§8).
- Restent valables les risques des checkpoints `fade726` §7 et `f172172` §7.

## 8. TODO — reste à faire

1. **Publier le paquet Web `5.10.2`** (déjà généré dans `dist/`, non versionné) :
   d'abord `dist\alwaysdata\web\v5.10.2\` → `/web/v5.10.2/`, **puis** seulement
   `dist\alwaysdata\aqualook-web-manifest.json` → racine ; ensuite
   `python tools/check_published_web.py https://aqualook.alwaysdata.net/aqualook-web-manifest.json`.
   Si `dist/` a disparu : `python tools/publish_web_assets.py 5.10.2 --comparer`.
2. **Mettre à jour `/editeur/` sur AlwaysData** : `data/scripts-schema.html`,
   `data/script-schema.js`, `data/script-lang.js` (et `data/style-base.css`
   inchangé).
3. Variables dans l'espace en ligne (CloudSync : remontée des noms/valeurs ;
   éventuellement modification) — décision à prendre.
4. Ménage Git : supprimer les branches fusionnées (`fix/web-pied-de-page-synchro`,
   `fix/cloudsync-apres-redemarrage`, `feature/scripts-parallele-sans-attendre`,
   `feature/lcd-a-propos`, …) après archivage `archive/<nom>` si souhaité.
5. Mettre à jour `docs/codex/00_CONTEXT.md` et `05_BUILD_AND_TEST.md` (encore
   rédigés pour Legacy/V4 CYD) et `docs/REPRISE_INGENIEUR.md` §1.1 (dit encore
   que tout vit sur `feat/moteur-de-regles`).
6. Corriger le test des variables locales `a..h` dans `script-lang.js` (§7).
7. Reste de la TODO `fade726` §8 (preuve avec le jeton admin de production,
   CloudSync 8 s sur plusieurs heures, CI et migration Ubuntu 26 le 19 oct.).

## 9. Procédure exacte de reprise

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -3
git status --short
git checkout -b <feature/...>
```

Demander le port COM courant, puis :

```powershell
git -c core.whitespace=cr-at-eol diff --check
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

Contrôles rapides sans secret :
`GET /api/diagnostics` (champ `build`), `GET /api/debug/script-selftest`
(attendu 21/21), `GET /api/script-globals`.

Banc logique de l'éditeur :

```powershell
& "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe" --headless --disable-gpu --allow-file-access-from-files --dump-dom "file:///<chemin-du-depot>/tools/script_schema_test.html"
```

Dépôt d'une page sur la SD de `.141` sans flash : `POST /api/debug/deploy-begin`,
`POST /api/debug/deploy-file?name=<fichier>` (corps binaire),
`POST /api/debug/deploy-commit`, puis relire et comparer le SHA-256. Le
classifieur de l'agent exige l'accord explicite de l'utilisateur pour chaque
dépôt.

Le secret HMAC du module (enregistrement et lancement de scripts) n'existe que
dans le navigateur de l'utilisateur ; l'utilisateur a refusé de le stocker sur
disque : les tests de scripts réels se font dans l'éditeur.

## 10. Commandes Git utiles

```powershell
git log --oneline 6f12f1e..HEAD    # la journée du 8 octobre
git show 33fc1e1                   # parallèle Et / Ou (JOINANY)
git show 71c120e                   # variables globales, socle firmware
git show 8f7ce05                   # journal lisible (cache des phrases, NOTIFY_VAR)
git show 1534dde                   # Notifier sur GSM (ALERT_VAR), zone repliable
```
