# Checkpoint AquaLook — 13 octobre 2026 (matin) — 5.13.3 : clignotement LCD du créneau (P2.11), `ota.html` en largeur PC (P2.12)

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-12_4f03e1f.md`.

> **En un coup d'œil :** version **5.13.3**. Sur le LCD, la barre du créneau
> programmé clignote pendant l'arrosage qu'il a lancé (P2.11). La page
> `ota.html` du module a sa navigation en haut, exploite la largeur d'un PC
> et n'a plus de script inline (P2.12, P3.11 pour cette page). Firmware
> flashé et vérifié sur `.141`, les deux évolutions validées par le
> propriétaire. Paquet Web 5.13.3 **préparé, pas encore envoyé** (FTP
> manuel du propriétaire).

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` (branche `feature/lcd-creneau-clignote` fusionnée `--no-ff` en `840bda3`, poussée, supprimée) |
| Commit fonctionnel | **`af6c1c6`** (code identique à `840bda3` ; `af6c1c6` ne touche que la TODO) |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-13_af6c1c6.md`) |
| Version fonctionnelle (`VERSION`) | **`5.13.3`** ; pas de tag |
| Firmware sur `.141` | `5.13.3`, build **1453**, `gitSha` `8c76854` (commit de la branche avant fusion, code identique à `main`), lu sur `/api/diagnostics` |
| Pages Web sur `.141` | `5.13.2` + `ota.html`/`ota.js` 5.13.3 déposés sur la SD (dépôt direct) |
| Paquet Web en ligne | `5.13.2` ; `5.13.3` préparé dans `dist/alwaysdata` (non suivi par Git) |
| Service AlwaysData (PHP) | inchangé depuis `c7cdec3` (aucun fichier de `cloud/` modifié) |
| Dernier numéro de réponse du chat source | `AQL-R003` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul, `ProgrammeArrosage_s3` seule cible
firmware. **Reste à faire : `docs/TODO.md`** (fait foi). Sécurité :
`docs/security/CYBERSECURITY_ARCHITECTURE.md` et
`docs/security/SECURITY_RISK_REGISTER.md`.

## 3. Ce qui a été fait (commits sur `main`)

| Commit | Sujet | Validation |
|---|---|---|
| `c9b57ae` | **P2.11** : la barre du créneau programmé clignote sur le LCD pendant son arrosage | `.141`, voir §3.1 ; visuel validé par le propriétaire |
| `78bbcea` | **P2.12** : `ota.html` en largeur PC, navigation en haut, script sorti dans `data/ota.js` | dépôt SD `.141`, captures Edge, banc CSP 14/14 ; validé par le propriétaire |
| `8c76854` | Version **5.13.3** | build 1453 sur `.141` |
| `347f5d4`, `2b3a2fd`, `9458c4d` | Registre de soak (flashs 1447, 1451, 1453) | — |
| `683a830`, `af6c1c6` | TODO : P2.11, P2.12 faits ; P3.11 sans `ota.html` | doc |
| `840bda3` | Fusion dans `main` | — |

### 3.1 P2.11 — chaîne réellement exécutée

- `ScheduleManager::update()` → après `activateZone()` d'un créneau :
  `ActiveSlot.slotIdx` / `slotEpochDay` renseignés ; `activateZoneMs()`
  (départs manuels et scripts) et `deactivateZone()` les remettent à -1 ;
  `resumeZone()` les conserve pour un créneau suspendu puis repris.
  Lecture : `ScheduleManager::getScheduledSlot()`.
- `renderPlanSprite()` (LIST), `renderPlanSpriteCompact()` (GRID2),
  `renderPlanSpriteFull()` (GRID4) : la barre du créneau en cours, colonne
  du jour seulement (`isRunningSlotToday()`, compare au jour NTP), est notée
  par `noteSlotBlink()` en coordonnées écran ; `drawHomeFull()` et chaque
  rendu effacent d'abord les notes des zones qu'ils couvrent.
- `DisplayManager::update()` → `updateSlotBlink()` après les gardes veille,
  mise à jour et encart météo : sur HOME, toutes les 500 ms
  (`SLOT_BLINK_MS`), ne repeint que le rectangle noté (barre / fond de
  cellule). Aucun `fillScreen()` (I4), cadence bornée (F1).
- Essai `.141` (COM4, mode LIST 4 zones) : créneau zone 3 à 23:08 →
  `[Screen] Réveil`, `[LCD] Zone 3 : creneau 5 clignote (2x12 @83,124)`,
  STOP à 23:10. Créneau zone 1 relancé en départ script par le script
  « Legitime » du banc : pas de clignotement, conforme à la demande.
  Créneaux d'essai remis à leur valeur d'origine (vérifié après
  redémarrage). Tas libre stable ~185 Ko, comme avant.

### 3.2 P2.12 — `ota.html`

Barre `nav.top` collante (Retour à AquaLook, Santé du module), page de
1 200 px ; au-delà de 900 px : verdict | canaux, puis programme | pages Web
et détails ; une colonne en dessous (`minmax(0,1fr)`). Script inline copié
mécaniquement dans `data/ota.js` ; `onclick` → `data-go` + délégation de
clic. 19 IDs conservés, routes inchangées. Banc local (Edge sans affichage,
CSP `script-src 'self'`, `fetch`/`confirm` simulés) : témoins négatifs
(script inline, `onclick`) bloqués, six boutons reliés, aucun POST : 14/14.

## 4. Fichiers et fonctions modifiés

| Fichier | Élément |
|---|---|
| `src/ScheduleManager.h/.cpp` | `ActiveSlot.slotIdx`, `slotEpochDay` ; `getScheduledSlot()` ; `update()`, `activateZoneMs()`, `deactivateZone()`, `resumeZone()` |
| `src/DisplayManager.h/.cpp` | `SlotBlink`, `isRunningSlotToday()`, `noteSlotBlink()`, `updateSlotBlink()` ; appels dans `update()`, `drawHomeFull()` et les trois rendus du planning |
| `data/ota.html`, `data/ota.js` (nouveau) | P2.12 |
| `VERSION` | 5.13.3 |
| `docs/TODO.md`, `tools/soak/ledger.json` | suivi |

Volontairement non modifiés : `littlefs/`, `cloud/`, NVS (aucune structure
persistée touchée : `ActiveSlot` vit en RAM), `platformio.ini`,
`ScreenManager` (réveil inchangé).

## 5. Invariants préservés

- I4 : pas de `fillScreen()` hors redraw complet ; I6 : le planificateur
  pilote toujours via callback ; I7 : sécurité de durée maximale intacte.
- F1 : clignotement non bloquant, cadence bornée ; F6 : routes inchangées ;
  IDs de `ota.html` conservés.
- Pages Web : plus de script inline dans `ota.html`.
- Mise à jour Web toujours déclenchée par l'utilisateur (D014).

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (build 1453, upload COM4) |
| buildfs | non requis (`littlefs/` inchangé) |
| Web module | `ota.html`/`ota.js` 5.13.3 sur la SD de `.141` (dépôt direct) |
| LCD | P2.11 validé en mode LIST (3-4 zones) ; GRID2 (5-8 zones) non essayé sur matériel |
| Matériel | `.141`, relais réels, essais d'une zone, 2 min |
| Paquet Web 5.13.3 | préparé (21 fichiers, manifeste 4 954 octets < 8 192) ; **envoi FTP à faire** |
| `git diff --check` (`cr-at-eol`) | propre |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture d'écran du propriétaire) |

## 7. Risques et limites

- L'écran ne se réveille qu'au passage « aucune vanne » → « une vanne »
  (`ScreenManager`, comportement existant) : un créneau qui démarre alors
  qu'une autre zone arrose ne réveille pas l'écran.
- Deux créneaux qui se chevauchent au pixel près dans une cellule : la
  phase éteinte du clignotement peut masquer l'autre barre une demi-seconde.
- Banc : le script « Legitime » relance la zone 1 à chacun de ses départs ;
  tester un comportement « créneau » sur la zone 3 ou 4.
- Six pages du module gardent un script inline (P3.11).
- `cloud/php-mutualized/.env` local = réglages de production (inchangé).

## 8. TODO

Voir **`docs/TODO.md`**. Prochaines étapes :

1. **Propriétaire** : envoyer le paquet 5.13.3 par FTP
   (`dist\alwaysdata\web\v5.13.3` → `/web/v5.13.3/`, puis
   `dist\alwaysdata\aqualook-web-manifest.json` à la racine, en dernier),
   puis vérification par l'agent et mise à jour Web de `.141` depuis
   `ota.html`.
2. P3.11 sur les autres pages du module, en appliquant la règle
   d'ergonomie à chaque page retouchée.
3. **P0.5** HTTPS LAN (étude), **P2.8** mails, **P2.10** IA (décision).
4. P1 : point 2 Wi-Fi vers le 14 oct., migration Ubuntu 26 le 19 oct.

## 9. Procédure exacte de reprise

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -4
git status --short
git -c core.whitespace=cr-at-eol diff --check
python tools/deploy_cloud.py verifier
python tools/check_published_web.py https://aqualook.alwaysdata.net/aqualook-web-manifest.json
```

`verifier` doit conclure « Serveur ALIGNE » ; `check_published_web.py`
annonce 5.13.2 tant que le FTP n'est pas fait, puis « 21/21 » en 5.13.3.
Si `dist/` a disparu : `python tools/publish_web_assets.py 5.13.3
--comparer`. Pour un développement firmware, demander le port COM, puis :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent f528a93..main   # cette session
git show c9b57ae                                 # P2.11
git show 78bbcea                                 # P2.12
git diff f528a93 840bda3 -- src/                 # tout le firmware de la session
```
