# Checkpoint AquaLook — 11 octobre 2026 (matin) — 5.13.1, phase 6 validée

Document de reprise autonome. Avec `AGENTS.md` et `docs/REPRISE_INGENIEUR.md`,
il suffit pour reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-11_679eb6b.md` (campagne sécurité du site cloud), dont les
§5 et §7 restent valables sauf mention contraire, et met à jour sa TODO §8.

> **En un coup d'œil :** la **phase 6 du lot F** (module sans carte SD,
> portail captif) est **validée sur `.141`**. Elle a révélé **trois défauts**,
> tous corrigés, testés sur la carte et fusionnés : version **`5.13.1`**,
> build **1382** flashé sur `.141`.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public) |
| Branche | `main` (branche `fix/sans-sd-acces-admin-accueil` fusionnée `--no-ff` en `71c7e46`, puis supprimée) |
| HEAD `main` avant ce document | **`c8b2a66`** (registre de soak) |
| Commit firmware fonctionnel | **`71c7e46`** (merge, version `5.13.1`) |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-11_c8b2a66.md`) |
| Version fonctionnelle (`VERSION`) | **`5.13.1`** ; pas de tag ni de release GitHub |
| Firmware sur `.141` | `71c7e46`, build **1382**, `5.13.1`, branche `main` — vérifié par `/api/diagnostics.build` après flash |
| Identifiant matériel `.141` | `aql-44bd8d7acb88` ; module serveur `Jardin-01` |
| Cible firmware | `ProgrammeArrosage_s3`, module `192.168.1.141`, **COM4** (confirmé le 11 oct. 2026, à reconfirmer) |
| Dernier numéro de réponse du chat source | `AQL-R016` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul, `ProgrammeArrosage_s3` seule cible
firmware. Dettes : `docs/codex/08_RISKS_AND_DEBT.md` (D9, D10, D11). Lot F et
phase 6 : `docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md` §9.

## 3. Ce qui a été fait

### 3.1 Phase 6 (essais sur `.141`, firmware 5.13.0 puis branche de correctifs)

| Partie | Vérification | Résultat |
|---|---|---|
| A — SD retirée (hors tension) | démarrage dégradé nommé (« SD indisponible », 5 remontages puis reprise lente 10 min), relais et planning chargés | OK |
| | `/login`, `/ota` (secours 3 ko), `/logs`, `/setup` (secours) | 200 |
| | écriture sans session (`POST /api/cloudSyncNow`) | 401 « session requise » |
| | connexion par `tools/module_session.py`, écriture avec session | 200, synchro lancée |
| | CloudSync | `rapport=ok config=ok cmd=aucune` |
| | après connexion : `/index.html` | **« Not found » → défaut 1** |
| LCD | bouton menu de HOME pendant la panne SD | **mène à Santé → défaut 2** |
| B — portail | `POST /api/captive` avec une session ouverte | **plantage → défaut 3** |
| Après correctifs | menu → PIN → ADMIN ; pictogrammes du bandeau → Santé | OK (propriétaire) |
| | portail lancé depuis le Web puis depuis l'écran | `AP 'Arrosage-Setup' IP=192.168.4.1`, aucun redémarrage |
| | téléphone sur `Arrosage-Setup` : page Wi-Fi, `/api/wifi/scan` libre, `/api/notifications` refusé | OK (propriétaire) |
| | SD remise : interface complète servie par la SD, Wi-Fi reconnecté | OK |
| Non essayé | enregistrer un réseau depuis le portail (configuration Wi-Fi préservée) | — |

### 3.2 Correctifs (un commit par défaut)

| Commit | Défaut | Correctif |
|---|---|---|
| `40b1ef4` | **3** — `assert failed: tcp_sent tcp.c:2131` dans `AsyncClient::_error` (trace décodée avec l'ELF du build 1368, SHA `434284fa…`). lwIP appelle `tcp_err` après avoir libéré le pcb ; AsyncTCP 3.3.2 le relisait (`_pcb->state`, `tcp_sent`). Déclenché par `WiFi.disconnect` (portail) avec une connexion ouverte. | Patch n°5 `AQUALOOK_ERROR_NO_PCB` dans `tools/patch_asyncwebserver.py` : `_error` oublie le pointeur sans le déréférencer. |
| `d36064a` | **2** — `DisplayManager::handleHeaderTouch` captait tout appui du bandeau dès qu'une panne était active, y compris le bouton menu de HOME : ADMIN inaccessible (portail, PIN, oubli du mot de passe Web). | Coin gauche (`tx < 40`) du bandeau de HOME laissé au bouton menu. |
| `62229ea` | **1** — sans SD, `/login` et le lien « Retour » de `/ota` menaient à `/index.html` = 404. | `onNotFound` (hors portail) sert `HOME_FALLBACK_HTML` pour `GET /index.html` : version, liens `/ota`, `/setup`, `/logs`, diagnostic. En-tête `X-AquaLook-Storage: Firmware-Fallback`. |
| `885b31b` | — | `VERSION` 5.13.0 → **5.13.1** |
| `8be32eb` | — | doc : phase 6 validée dans `ENROLEMENT_ET_SECURITE_LOCALE.md` |
| `71c7e46` | — | merge `--no-ff` dans `main` |
| `c8b2a66` | — | registre de soak (flash 5.13.1) |

## 4. Fichiers et fonctions modifiés

| Fichier | Fonction / élément |
|---|---|
| `tools/patch_asyncwebserver.py` | liste `PATCHES` : patch n°5 (`AsyncClient::_error` d'AsyncTCP) |
| `src/DisplayManager.cpp` | `DisplayManager::handleHeaderTouch` |
| `src/WebManager.cpp` | `HOME_FALLBACK_HTML` (nouveau), gestionnaire `onNotFound` |
| `VERSION` | 5.13.1 |
| `docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md` | §9, lot F : phase 6 |
| `tools/soak/ledger.json` | registre de flash |

Volontairement non modifiés : routes existantes, IDs HTML, format NVS, relais,
planificateur, `littlefs/` (buildfs non requis), `data/` (rien à publier sur
AlwaysData), page `/login` (redirige toujours vers `/index.html`, qui sert
désormais le secours sans SD).

## 5. Invariants préservés

- Relais, durée maximale, planificateur, moteur V4 : non touchés.
- Routes : aucune ajoutée ni renommée ; `/index.html` n'est servi en secours
  que lorsque ni la SD ni LittleFS ne l'ont servi.
- Session Web (lot F) : le filtre `SessionGate` est inchangé ; la page de
  secours est une lecture libre ne contenant que des liens.
- Portail captif : redirection vers `/setup` inchangée (branche prioritaire de
  `onNotFound`).
- Bandeau LCD : les pictogrammes de panne mènent toujours à Santé hors du coin
  du bouton menu ; ADMIN, HEALTH et PIN gèrent toujours leur propre bandeau.
- Identité de build : source unique (`VERSION` + `tools/version_build.py`),
  vérifiée sur la carte.
- §5 des checkpoints `a721b81` et `679eb6b` : valables.

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | **SUCCESS** (build 1382, flash 78,9 %, +1,4 ko pour la page de secours) |
| buildfs | non requis (`littlefs/` inchangé) |
| Dépôt | propre, `main` synchronisé avec `origin/main` |
| `.141` | flashé sur COM4 avec `71c7e46` (5.13.1, build 1382) ; SD prête, Wi-Fi −55 dBm, borne `00:11:32:A4:35:15` |
| Web | page de secours vérifiée (200 sans SD, page SD avec SD) ; `/login`, `/ota`, `/logs`, `/setup` de secours |
| LCD | menu ADMIN pendant panne et Santé par le bandeau validés par le propriétaire |
| Matériel | aucun relais sollicité pendant la campagne |
| AlwaysData | inchangé |

## 7. Risques et limites

- **Défaut 3, essai limité** : le plantage dépendait de la réutilisation d'une
  zone mémoire (aléatoire). Deux lancements de portail sans plantage après le
  correctif (Web avec session ouverte, puis écran) ; la cause corrigée est
  exactement celle de la trace. Le même mécanisme (`tcp_err` sur une connexion
  abandonnée) pourrait jouer lors d'autres coupures Wi-Fi : piste non vérifiée,
  pas une conclusion.
- Le patch n°5 dépend de l'ancre textuelle d'AsyncTCP 3.3.2 : une mise à jour
  de la bibliothèque le ferait sauter (signalé au build, jamais bloquant).
  Revérifier les messages `patch_asyncwebserver` après tout changement de lib.
- Journal série bruyant sans SD : `vfs_api open(): /littlefs/... does not
  exist` à chaque page absente (`serveStatic` LittleFS). Sans conséquence.
- La bannière série de démarrage affiche toujours `Cible OTA : unsupported` et
  `Carte : esp32-2432S028` sur le S3 (constat antérieur, non corrigé).
- Restent valables : §7 de `679eb6b` (CSP `'unsafe-inline'`, déploiement du
  cloud hors Git) et de `a721b81` (D9, mot de passe Web de banc, HTTP en clair,
  secret NVS en clair).

## 8. TODO — reste à faire

1. **Wi-Fi mesh (reporté par le propriétaire)** : test de bascule de nœud.
   Couper le routeur principal Synology fait tomber tout le maillage, ce test
   est donc impossible. Piste retenue : rapprocher une borne secondaire du
   banc, redémarrer le module pour qu'il s'y accroche (vérifier `wifi.bssid`),
   puis la débrancher. À défaut : option de banc pour forcer un BSSID.
   Puis, vers le 14 oct. : relire l'historique `payload.wifi` (`Jardin-01`) et
   décider du **point 2** (branche `feature/wifi-diag-bssid` conservée).
2. Recopier `data/scripts-schema.html` et `data/session.js` dans `/editeur/`
   (AlwaysData) ; paquet Web `5.10.4`.
3. Lot G (effacement du PIN depuis l'espace en ligne).
4. D9 : chaîne de release S3, puis retrait de `_legacy`/`_v4`.
5. Avant mise en service : mot de passe Web définitif.
6. (Optionnel) enregistrer un réseau depuis le portail captif (non essayé).
7. (Optionnel, sécurité) externaliser les scripts inline d'`admin.html` et
   `app.html` (CSP sans `'unsafe-inline'`) ; déployer le cloud depuis Git.
8. (Petit) bannière série du S3 : `Cible OTA`/`Carte` erronées.

## 9. Procédure exacte de reprise

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -3
git status --short
git -c core.whitespace=cr-at-eol diff --check
```

Le firmware sur `.141` est celui de `main` (5.13.1, build 1382) : rien à
reflasher pour reprendre. Pour un nouveau développement firmware, demander le
port COM, puis :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

Flash de `.141` et dépôt SD autorisés d'office ; port COM à reconfirmer par
session. Pièges : `git diff --check` avec `-c core.whitespace=cr-at-eol` ;
`announce_reboot.py` modifie `tools/soak/ledger.json` ; retirer la SD hors
tension (5 V **et** USB) ; un téléphone quitte `Arrosage-Setup` en fermant la
page du portail (« Utiliser sans Internet » / « Rester connecté »).

## 10. Commandes Git utiles

```powershell
git log --oneline e35ab96..main                # cette session
git show 40b1ef4                               # patch AsyncTCP n°5
git show d36064a                               # bandeau LCD
git show 62229ea                               # accueil de secours
git diff e35ab96 71c7e46 -- src/ tools/patch_asyncwebserver.py VERSION
```
