# Checkpoint AquaLook — 17 octobre 2026 — 5.17.13 : OTA v5.17.12 validée, notifications ntfy mesurées et chiffrées par défaut (P2.17)

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-17_b026340.md`.

> **En un coup d'œil :** **installation OTA réelle de la release publique
> v5.17.12 validée** sur `.141` ; **les notifications ntfy ne font plus ramer
> le module** (mesuré en clair et en HTTPS, aucun effet sur la boucle ni sur
> la latence Web) ; la page des réglages imposait `http://` alors que le
> firmware chiffre depuis le 4 sept. → **`https://ntfy.sh` par défaut**
> (5.17.13, page seulement) ; deux nouveaux sujets au TODO : P2.17 (fait) et
> **P2.18 : créer un nouveau module facilement** (SD, flash, premier
> démarrage, service en ligne, guide).

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` ; branches `docs/todo-p217-p218` et `fix/p217-ntfy-https` fusionnées `--no-ff` (poussées) |
| Commit fonctionnel | **`1b8cd57`** (dernier code : `f195266`, `app.js`/`index.html`) ; après lui, documentation seule |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-17_1b8cd57.md`) |
| Version (`VERSION`) | **`5.17.13`** (pas de tag : seule la page change, le firmware 5.17.13 est identique en comportement à 5.17.12) |
| Release GitHub publique (latest) | `v5.17.12`, signée (clé `37c2c8068db00bd9`), **installée par OTA avec succès sur `.141`** |
| Firmware sur `.141` | `5.17.13`, build **1665**, `f195266`, env `ProgrammeArrosage_s3_banc` (`devRoutes=true`), santé normale |
| Pages Web sur `.141` | paquet 5.17.12 + `app.js` et `index.html` 5.17.13 déposés directement sur la SD (relus identiques) |
| Paquet Web en ligne | **5.17.12** (sans le correctif ntfy), signé, 30/30, manifeste 7 007 o / 8 192 — **5.17.13 non publié** |
| Service AlwaysData | ALIGNE sur `1b8cd57` (rien à publier : aucun fichier du site modifié) ; `/editeur/` 8/8 identiques à `data/` |
| Notifications de `.141` | actives, serveur **`https://ntfy.sh`** (port 443), sujet inchangé |
| Nom local de `.141` | `jardin-03` → `http://jardin-03.local`, port **COM4** (confirmé dans cette session) |
| Dernier numéro de réponse du chat source | `AQL-R007` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul. **Reste à faire : `docs/TODO.md`**.

## 3. Ce qui a été fait

| Commit | Contenu |
|---|---|
| `b8a7c86` | TODO : **P2.17** (notifications ntfy à remesurer) et **P2.18** (créer un nouveau module : SD, flash, premier démarrage, impact service en ligne, guide et outils) |
| — (matériel) | **P2.16 suite — OTA v5.17.12 validée** : `.141` flashé par USB en production 5.17.11 (`676bf28`, worktree temporaire), « Vérifier maintenant » → 5.17.12 proposée, mise à jour lancée par le propriétaire depuis `ota.html` : 1 578 320 o en 10,7 s, SHA-256 conforme, `activated-app1-pending-validation`, 5.17.12 build 1657 `0cdd36d` (GitHub) en marche, santé normale |
| — (mesure) | **P2.17** : `/api/diagnostics` relevé toutes les 2 s pendant 6 min, 3 notifications de test, en `http://` puis en `https://` (voir § 4) |
| `0777738` | `VERSION` 5.17.13 |
| `f195266` | **P2.17** — `data/app.js` : le serveur ntfy accepte `https://` et `http://` (`/^https?:\/\/\S{3,}$/`), `https://ntfy.sh` par défaut (affichage, enregistrement, réinitialisation) ; `data/index.html` : placeholder `https://ntfy.sh`, aide réécrite (« https://ntfy.sh conseillé : envoi chiffré ; http:// circule en clair, à réserver à un relais local ») |
| `5ba542e` | TODO : P2.17 fait avec les mesures ; P2.16 : OTA validée |

## 4. Essais

| Profil / cible | Port | Essai | Attendu | Observé |
|---|---|---|---|---|
| `ProgrammeArrosage_s3` 5.17.11 (`676bf28`) | COM4 | flash USB, identité | production | `env=ProgrammeArrosage_s3`, `devRoutes=false`, santé normale |
| `.141` production | — | OTA v5.17.12 depuis `ota.html` (propriétaire) | installée, validée | 5.17.12 build 1657 `0cdd36d`, `app1`, santé normale |
| `.141` 5.17.12, ntfy `http://` | — | 6 min, 3 tests | pas d'effet | envoi 1–2 s, ~5 Ko ; 148 dépassements de boucle / 6 min (écran, sans lien avec les envois) ; HTTP médiane 618 ms, p95 667 ms |
| `.141` 5.17.12, ntfy `https://` | — | 6 min, 3 tests | pas d'effet | envoi 2–3 s, ~45 Ko pendant TLS, tas minimal 87,7 Ko ; 147 dépassements / 6 min ; HTTP médiane 630 ms, p95 685 ms ; 3/3 livrées en 443 |
| `ProgrammeArrosage_s3_banc` 5.17.13 | COM4 | flash USB | banc | build 1665 `f195266`, `devRoutes=true`, santé normale |
| `.141` | — | dépôt SD `app.js`, `index.html` | identiques | relus identiques en HTTP |
| Edge sans affichage → relais avec session → `.141` | — | formulaire ntfy de la page servie par le module | refus `ftp://`, `https://` seul ; `http://` et `https://` enregistrés ; sujet gardé | **9/9** |
| `.141` | — | notification de test après enregistrement par la page | livrée en 443 | `port=443 status=ok`, `http=200` |
| `csp_banc_pages.py --largeur 390` et sans largeur | — | toutes les pages | tout passe | tout passe (index 25/25 et 24/24) ; un premier passage pendant une compilation a donné 2 échecs `ota-repli`/`ota-dispo` (fichiers non touchés), disparus au relancement |
| `ProgrammeArrosage_s3` 5.17.13 | — | compilation production | SUCCESS | SUCCESS (Flash 80,2 %, RAM 27,1 %), non flashé |
| AlwaysData | — | `deploy_cloud.py verifier`, `check_published_web.py`, `release_firmware.py verifier 5.17.12`, `/editeur/` | aligné | ALIGNE `1b8cd57`, 30/30, release conforme, 8/8 |

**Non essayé :** envoi simultané d'une notification TLS (~45 Ko) et d'une
synchro CloudSync (~68 Ko) ; texte de la page sur iPhone (banc 390 px seul) ;
paquet Web 5.17.13 (non publié).

## 5. Fichiers modifiés

| Zone | Fichiers |
|---|---|
| Module Web | `data/app.js`, `data/index.html` |
| Docs | `docs/TODO.md`, ce checkpoint |
| Outils | `tools/soak/ledger.json` (fenêtres de maintenance) |
| Version | `VERSION` = 5.17.13 |

Volontairement non modifiés : firmware (`src/` ; `NotificationManager`
chiffre déjà depuis `f0b050b`), `littlefs/`, NVS, routes, IDs HTML, relais,
PHP, site.

## 6. Invariants préservés

- F5/F6 : routes et IDs inchangés (`cfg-ntfy-server` conservé).
- Pages : aucun script inline, aucun `on*=` (banc CSP tout passe).
- F14 : version de source unique (`VERSION`), relue dans `/api/diagnostics`.
- I1/I2 : `littlefs/` inchangé ; buildfs non requis.
- Mise à jour jamais déclenchée à distance : l'OTA a été lancée par le propriétaire.
- Relais et durée maximale : non touchés.
- Cybersécurité : notifications chiffrées par défaut ; aucun sujet ntfy ni mot de passe écrit dans un journal, un document ou le chat (l'essai n'affiche que la longueur du sujet).

## 7. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (`1b8cd57` équivalent, `f195266`) |
| Banc `ProgrammeArrosage_s3_banc` | `.141` 5.17.13 build 1665 |
| buildfs | non requis |
| Web | en ligne 5.17.12 ; `.141` : 5.17.12 + `app.js`/`index.html` 5.17.13 |
| Service AlwaysData | ALIGNE sur `1b8cd57` |
| LCD / matériel | aucun changement |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture du 11 oct.) |

## 8. Risques et limites

- **Paquet Web 5.17.13 non publié** : les autres modules gardent la page qui refuse `https://` ; à publier (`publish_web_assets.py 5.17.13`, accord du propriétaire) au prochain paquet.
- Les modules déjà réglés en `http://ntfy.sh` le restent : le correctif ne change que la page ; il faut réenregistrer `https://ntfy.sh` sur chaque module.
- Mémoire : une session TLS ntfy prend ~45 Ko ; cumul avec CloudSync (~68 Ko) non provoqué (marge estimée > 60 Ko).
- Pile de la tâche d'envoi : ~4 Ko libres sur 8 en TLS (`stackFree=3984`).
- La boucle dépasse 100 ms environ toutes les 2–3 s, dominé par l'écran (redessin complet ~80 ms) : sans lien avec les notifications, piste si une lenteur est encore ressentie.
- Un `environment-mismatch` garde l'ancienne « version disponible » (`MaintenanceResult.cpp:142`) : `ota.html` sur le banc peut afficher une version périmée.
- Compiler depuis un worktree au chemin long échoue (limite de 260 caractères de Windows, en-têtes ArduinoJson) : passer par `subst` ou un chemin court.
- Git Bash convertit les arguments commençant par `/` : `MSYS_NO_PATHCONV=1` ou PowerShell.

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
python tools/release_firmware.py verifier 5.17.12
```

Attendu : « Serveur ALIGNE », 30/30 (5.17.12), release v5.17.12 conforme et
signée. `.141` : `/api/diagnostics` → `build.version=5.17.13`, `number=1665`,
`gitSha=f195266`, `env=ProgrammeArrosage_s3_banc` ; `/api/health` →
`overall=normal`.

Pages (avant toute livraison Web) :

```powershell
python tools/csp_banc_pages.py --largeur 390
python tools/apercu_web/largeur_site.py
```

Firmware (port COM à reconfirmer) :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3_banc -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

Suite proposée : publier le paquet Web 5.17.13 (accord du propriétaire),
puis **P2.18** (création d'un nouveau module), P2.8 (mails), P2.2.

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent fc3e053..main
git show f195266 --stat     # P2.17 page ntfy
git show f0b050b --stat     # firmware : canal ntfy chiffre (4 sept.)
git show 5ba542e            # TODO P2.17 / P2.16
```
