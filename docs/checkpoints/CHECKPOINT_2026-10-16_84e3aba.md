# Checkpoint AquaLook — 16 octobre 2026 (nuit) — P3.23, firmware 5.17.9 build 1627

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-16_3b9575c.md`.

> **En un coup d'œil :** la barre du créneau programmé clignote de nouveau
> sur le LCD quand un script relance la zone (P3.23, Z1 et Z2 confirmées à
> l'œil). Correctif limité à `ScheduleManager`. `.141` reflashé sur `main`.
> Prochain chat : P4.11 puis P1.6, ensuite version **5.17.10** (décision du
> propriétaire).

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` ; branche `fix/p323-clignotement-script` fusionnée `--no-ff` puis supprimée |
| Commit fonctionnel | **`84e3aba`** (firmware de `.141`, serveur AlwaysData ALIGNE) ; après lui, documentation seule |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-16_84e3aba.md`) |
| Version (`VERSION`) | `5.17.9` inchangée ; **5.17.10** prévue après P4.11 et P1.6 (décision du propriétaire) ; pas de tag |
| Release GitHub publique (latest) | `v5.17.2`, signée (inchangée) |
| Firmware sur `.141` | `5.17.9`, build **1627**, `84e3aba`, branche `main`, env `ProgrammeArrosage_s3_banc` (`devRoutes=true`), santé normale |
| Pages Web sur `.141` | paquet `5.17.9` (inchangé) |
| Paquet Web en ligne | `5.17.9`, signé, `check_published_web.py` 27/27 |
| Service AlwaysData (PHP) | `deploy_cloud.py verifier` : ALIGNE sur `84e3aba` (rien à publier : `cloud/`, `data/`, `littlefs/` inchangés depuis `3b9575c`) |
| Nom local de `.141` | `jardin-03` → `http://jardin-03.local`, port **COM4** (confirmé dans cette session, à reconfirmer) |
| Dernier numéro de réponse du chat source | `AQL-R005` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul. **Reste à faire : `docs/TODO.md`**
(P3.23 barré). Pas de nouvelle décision `02_DECISIONS.md` ; registre des
risques inchangé (aucune nouvelle surface).

## 3. Ce qui a été fait — P3.23 (`d67380c`)

**Cause.** Le script « Legitime » (déclencheur « début de zone 1 ») relance
aussitôt la zone (`demarrer zone 1 pendant 600`). `ScheduleManager::
startZoneForSeconds()` → `activateZoneMs()` remettait `slotIdx` à -1, et
`getScheduledSlot()` refusait tout arrosage marqué manuel : le LCD ne
trouvait plus le créneau, la barre ne clignotait pas. Ordre vérifié : le
`ScriptRunner` détecte le départ de zone par sondage, donc après que
`update()` a noté le créneau.

**Correctif** (`src/ScheduleManager.cpp`, commentaires dans `ScheduleManager.h`
et `DisplayManager.h`) :

- `startZoneForSeconds()` : une zone **déjà en cours pour un créneau** que
  le script relance garde `slotIdx`/`slotEpochDay` ; ligne série
  `[Schedule] Zone N — script : creneau K conserve`. Une zone arrêtée puis
  relancée par le script repart sans créneau (règle P2.11 conservée).
- `getScheduledSlot()` : `slotIdx` seul fait foi (plus de test `isManual`).
- `resumeZone()` : recopie le créneau sans condition (-1 pour un départ sans
  créneau), pour qu'un créneau prolongé puis suspendu/repris reste le même.

Chaîne vérifiée : `ScheduleManager::update()` (créneau) → `ScriptRunner`
(déclencheur) → `ScriptHostRuntime` `ZONE_START` → `startZoneForSeconds()`
→ `displayDirty` → rendu du planning → `isRunningSlotToday()` →
`noteSlotBlink()` → `updateSlotBlink()` depuis `DisplayManager::update()`.

## 4. Essais

| Profil flashé | Port | Essai | Attendu | Observé |
|---|---|---|---|---|
| `ProgrammeArrosage_s3_banc` build 1624 (`6f415e6+modif`) | COM4 | créneau d'essai Z1 vendredi, emplacement 5, 19:22, 2 min | script relance 10 min, créneau conservé, barre qui clignote | `START 2min (planifie)` → `Script 1 (Legitime) demarre` → `START 10min (manuel)` → `creneau 5 conserve` → `[LCD] Zone 1 : creneau 5 clignote` ; confirmé à l'œil par le propriétaire |
| idem | COM4 | créneau d'intervalle Z2, emplacement 2 (horaire avancé à 19:24 par le propriétaire) | barre qui clignote | `[LCD] Zone 2 : creneau 2 clignote` ; confirmé à l'œil |
| `ProgrammeArrosage_s3` | — | compilation de production | SUCCESS | SUCCESS (Flash 80,2 %, RAM 27,1 %) ; non flashé |
| `ProgrammeArrosage_s3_banc` build 1627 (`84e3aba`, `main`) | COM4 | reflash final | identité = `main` | `/api/diagnostics` : 5.17.9, 1627, `84e3aba`, `main`, `devRoutes=true` ; `/api/health` normal |

Créneaux d'essai remis à leur état d'origine (Z1 ven. emplacement 5 :
22:00, 2 min, désactivé ; Z2 intervalle emplacement 2 : 06:00, 5 min,
désactivé), Z1 arrêtée à la main, toutes zones à l'arrêt.

**Non essayé :** LCD GRID avec 5 à 8 zones (`.141` n'a que 4 zones) ;
suspension puis reprise d'un créneau prolongé par un script (chemin
`resumeZone()` relu seulement).

## 5. Fichiers modifiés

| Zone | Fichiers |
|---|---|
| Firmware | `src/ScheduleManager.cpp` (`startZoneForSeconds`, `getScheduledSlot`, `resumeZone`), `src/ScheduleManager.h`, `src/DisplayManager.h` (commentaires) |
| Docs | `docs/TODO.md`, ce checkpoint |
| Outils | `tools/soak/ledger.json` (fenêtres de maintenance) |

Volontairement non modifiés : `littlefs/`, NVS, routes, pages Web, relais,
`RelaisManager`, journal « manuel/planifié », `VERSION`.

## 6. Invariants préservés

- I6/I7 : l'activation passe toujours par le callback relais ; durée
  maximale de sécurité intacte.
- I4 : aucun `fillScreen()` ajouté (aucun changement d'affichage).
- F1 : aucun blocage ajouté.
- F5/F6 : routes et IDs inchangés.
- F14 : version de source unique (`VERSION`).

## 7. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS sur le code de `84e3aba` |
| Banc `ProgrammeArrosage_s3_banc` | SUCCESS et flashé sur COM4, `84e3aba` |
| buildfs | non requis (`littlefs/` inchangé) |
| Web | inchangé, 5.17.9 en ligne et sur `.141` |
| Service AlwaysData | ALIGNE sur `84e3aba` |
| LCD / matériel | clignotement Z1 et Z2 confirmé à l'œil ; santé normale |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture du 11 oct.) |

## 8. Risques et limites

- Un arrosage de créneau prolongé par un script reste « manuel » dans le
  journal série (`fin manuel`) tout en clignotant : voulu (le journal dit
  qui a fixé la durée, le LCD qui a déclenché).
- OneDrive peut verrouiller `.git/index` pendant une fusion (P4.6 ouvert).
- Git Bash convertit les arguments commençant par `/` : `MSYS_NO_PATHCONV=1`.
- Une capture série tient COM4 : l'arrêter avant tout téléversement.
- P1.6 (accès physique, SEC-028) : décision du propriétaire attendue.

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

Attendu : « Serveur ALIGNE », 27/27 (5.17.9). `.141` : `/api/diagnostics`
→ `build.version=5.17.9`, `number=1627`, `gitSha=84e3aba`,
`env=ProgrammeArrosage_s3_banc` ; `/api/health` → `overall=normal`.

Suite (décision du propriétaire) : **P4.11** (interface uniforme pilotée
par le CSS : commencer par l'inventaire des `<style>` internes et couleurs
en dur, module et site), puis **P1.6** (décision accès physique), puis
passage en **5.17.10**.

Firmware (port COM à reconfirmer) :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3_banc -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent 6f415e6..main
git show d67380c            # P3.23
git log -- src/ScheduleManager.cpp
```
