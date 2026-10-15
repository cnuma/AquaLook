# Checkpoint AquaLook — 15 octobre 2026 (soir) — paquets Web 5.17.5 / 5.17.6, P3.19, P3.20, SEC-029/030

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-15_dc1957d.md`.

> **En un coup d'œil :** paquet Web 5.17.5 (`sante.js`, P3.18) puis 5.17.6
> (navigation, P3.20) publiés, signés et installés sur `.141`. La CSP est
> désormais envoyée aussi sur les pages de la SD (P3.19). Scripts et Journal
> ont la barre de navigation commune (P3.20, reste `diagnostic.html` et
> `urls.html`). Deux risques de devinette de mot de passe relevés et
> consignés (P1.10 module, P1.11 console ; SEC-029, SEC-030). `.141` flashé
> depuis `main` en 5.17.6.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` ; branches `chore/web-5175`, `docs/p110-force-brute`, `fix/p319-csp-pages-sd`, `chore/web-5176`, `docs/checkpoint-2026-10-15-soir` fusionnées `--no-ff` puis supprimées |
| Commit fonctionnel | **`418e8fb`** (firmware de `.141`) ; après lui, documentation seule |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-15_418e8fb.md`) |
| Version (`VERSION`) | `5.17.6` ; pas de tag, pas de release |
| Release GitHub publique (latest) | `v5.17.2`, signée (inchangée) |
| Firmware sur `.141` | `5.17.6`, build **1590**, `418e8fb`, branche `main`, env `ProgrammeArrosage_s3_banc` (`devRoutes=true`) |
| Pages Web sur `.141` | paquet `5.17.6` installé par la mise à jour normale (27/27) |
| Paquet Web en ligne | `5.17.6`, signé (clé `37c2c8068db00bd9`) |
| `/editeur/` (AlwaysData) | 7 fichiers identiques à `data/` |
| Service AlwaysData (PHP) | `deploy_cloud.py verifier` : ALIGNE ; `web_security_probe.py` 32 PASS |
| Nom local de `.141` | `jardin-03` → `http://jardin-03.local`, port **COM4** (confirmé dans cette session, à reconfirmer) |
| Dernier numéro de réponse du chat source | `AQL-R009` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul. **Reste à faire : `docs/TODO.md`**
(fait foi). Pas de nouvelle décision `02_DECISIONS.md` ; nouveaux risques
SEC-029 et SEC-030, SEC-015 complété.

## 3. Ce qui a été fait

### 3.1 Paquet Web 5.17.5 (`0f4a3c2`)

`VERSION` 5.17.5 ; `publish_web_assets.py 5.17.5` (27 fichiers, signé) ;
envoi FTPS par l'agent sur accord du propriétaire (fichiers → relecture
HTTPS 27/27 → `.sig` → manifeste) ; `check_published_web.py` 27/27 +
`signature-ok` ; installé sur `.141` (`POST /api/webassets/update` :
`deploye version=5.17.5 fichiers=27/27`, 27/27 servis conformes). Le
classifieur du mode automatique de Claude Code a d'abord refusé la
publication ; le propriétaire l'a autorisée.

### 3.2 Question du propriétaire : mise à jour « sans contrôle » ?

Non : toute écriture (`POST`) exige une session (`needsSession`), écriture
inter-sites refusée, cookie `SameSite=Strict`, refus pendant un arrosage, et
seules des pages signées par la clé du propriétaire s'installent (D018,
anti-rejeu). L'erreur de l'agent était de méthode (identité de la carte non
relue avant l'écriture). Le propriétaire a refusé l'essai sans session.

### 3.3 Essais répétés de mot de passe : P1.10, P1.11 (`512d070`)

Revue de code : module, compteur **par adresse IP** (5 essais libres, puis
30 s doublées jusqu'à 15 min), défi à usage unique (un nouveau jeton ne
remet rien à zéro) ; faille : table de 8 adresses, la plus ancienne effacée
même bloquée, aucun plafond global (SEC-029). Espace utilisateur : 10 échecs
par adresse mail **et** 30 par IP sur 15 min, en base (sain). Console
d'administration : aucun compteur, bcrypt seul (SEC-030). Fiche §2.2 du
registre.

### 3.4 P3.19 — CSP sur les pages de la SD (`2910e4c`)

`src/WebCsp.h` : politique unique, partagée avec les pages intégrées
(P3.13). `SdStaticHandler` la pose sur les `.html` et `.svg` (pas les
`.js`/`.css`). `.141` (banc build 1582) : en-tête sur les 11 pages de la SD
(dont `/setup`, `/logs`) ; rendu Edge par un relais local qui garde la CSP
du module et ajoute un `report-uri` : 0 violation, témoin négatif (script
inline) bloqué et signalé. **Leçon** : `--enable-logging` d'Edge ne remonte
pas les violations (témoin à 0 alors que le script était bloqué) ; seul un
`report-uri` les compte. Production `ProgrammeArrosage_s3` SUCCESS
(Flash 80,2 %, RAM 27,1 %).

### 3.5 P3.20 — navigation Scripts et Journal (`f840c17`, `5e1cce2`)

Relevé par le propriétaire. `scripts-schema.html` : bouton « ‹ Retour à
AquaLook » dans la barre du haut, masqué en ligne (`body.cloud`) ; banc
éditeur 77/77. `logs.html` : thème commun `style-base.css`, `nav.top` et
`a.back` de Santé (IDs et `logs.js` inchangés). Validé à l'œil par le
propriétaire. Paquet **5.17.6** publié comme 5.17.5 et installé sur `.141` ;
`scripts-schema.html` recopié dans `/editeur/`.

### 3.6 TODO : P1.1, P1.7, P1.8 clos (`800fbf3`)

Par un **autre chat** travaillant dans le même dossier à 21:03 (pendant le
flash de cette session) ; changement documentaire seul, sans conflit,
intégré. Rappel : un seul chat actif à la fois.

## 4. Essais non faits

- P3.19 : pages sous session ouverte dans le rendu automatique (vues par le
  propriétaire à l'œil) ; portail captif réel (mode AP).
- 5.17.5 : orange « Attention » de `sante.js` non revu à l'œil après
  installation par la voie normale (validé par dépôt direct en P3.18).
- Binaire de production non flashé (compilé seulement).

## 5. Fichiers modifiés

| Zone | Fichiers |
|---|---|
| Firmware | `src/WebCsp.h` (nouveau), `src/SdStaticHandler.cpp`, `src/WebManager.cpp` |
| Web (SD) | `data/scripts-schema.html`, `data/logs.html` |
| Version | `VERSION` (5.17.6) |
| Docs | `docs/TODO.md`, `docs/security/SECURITY_RISK_REGISTER.md`, ce checkpoint |
| Outils | `tools/soak/ledger.json` (fenêtres de maintenance) |

Volontairement non modifiés : `littlefs/`, NVS, routes, IDs HTML existants
(un ID ajouté : `b-retour`), relais, service PHP.

## 6. Invariants préservés

- I6/I7 : aucune commande de vanne ni durée maximale touchée.
- F5/F6 : routes et IDs existants inchangés.
- F14 : version de source unique (`VERSION`), CSP de source unique.
- D014 : aucune mise à jour déclenchée à distance ; les installations sur
  `.141` passent par la route utilisateur, sous session.
- Pages : aucun script inline, aucune ressource externe.

## 7. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (P3.19) ; binaire de banc de `418e8fb` SUCCESS et flashé |
| buildfs | non requis (`littlefs/` inchangé) |
| Web | en ligne et sur `.141` : 5.17.6 ; `/editeur/` à jour |
| LCD / matériel | santé `normal` après flash et mise à jour ; pas de changement d'affichage LCD |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture d'écran du 11 oct.) |

## 8. Risques et limites

- SEC-029 / SEC-030 ouverts (P1.10, P1.11) : mot de passe long et aléatoire
  en attendant.
- P3.20 inachevé : `diagnostic.html`, `urls.html`.
- Le firmware de la session n'est pas en release (exige la signature du
  propriétaire, `release_firmware.py`).
- Deux chats dans le même dossier : vérifier `git log` avant toute fusion.
- Mode automatique de Claude Code : une publication peut être refusée par
  le classifieur ; le propriétaire l'autorise.

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
python tools/csp_banc_pages.py
```

Attendu : « Serveur ALIGNE », 27/27 + `signature-ok` (5.17.6), banc CSP
« tout passe ». `.141` : `/api/diagnostics` → `build.version=5.17.6`,
`number=1590`, `env=ProgrammeArrosage_s3_banc` ; `/api/health` →
`overall=normal`.

Firmware (port COM à reconfirmer) :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3_banc -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent 3b7f47b..main
git show 2910e4c            # P3.19
git show f840c17 5e1cce2    # P3.20
git show dd2d860            # P1.10/P1.11, SEC-029/030
curl.exe -sI http://192.168.1.141/index.html   # en-tete Content-Security-Policy
```
