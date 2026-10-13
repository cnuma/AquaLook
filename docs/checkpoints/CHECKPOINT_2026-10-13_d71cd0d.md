# Checkpoint AquaLook — 13 octobre 2026 (soir) — 5.15.2 : pages sans script inline, mises à jour signées, versions visibles partout, nom local

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-13_af6c1c6.md`.

> **En un coup d'œil :** version **5.15.2**, tout est aligné (dépôt, serveur
> AlwaysData, paquet Web en ligne, `/editeur/`, firmware et pages de `.141`).
> Trois chantiers fusionnés dans `main` : **P3.11** (plus aucun script inline
> dans les pages du module), **D018** (mises à jour Web signées ECDSA P-256,
> indépendantes de l'hébergeur), **D019** (versions et mises à jour visibles
> partout avec la même UI, mode d'emploi illustré, module joignable par
> `http://<nom>.local`). Validé par le propriétaire.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` (branches `feature/p311-scripts-inline`, `feature/signature-maj-web`, `feature/site-versions` fusionnées `--no-ff` puis supprimées) |
| Commit fonctionnel | **`d71cd0d`** |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-13_d71cd0d.md`) |
| Version fonctionnelle (`VERSION`) | **`5.15.2`** ; pas de tag |
| Firmware sur `.141` | `5.15.2`, build **1490**, `gitSha` `9902d1f` (code identique à `d71cd0d`, qui n'ajoute que de la documentation et le registre de soak) |
| Pages Web sur `.141` | `5.15.2`, installées par OTA depuis le paquet signé (27/27 conformes) |
| Paquet Web en ligne | `5.15.2`, signé (clé `37c2c8068db00bd9`), `check_published_web.py` 27/27 + signature ok |
| Service AlwaysData (PHP) | `deploy_cloud.py verifier` : ALIGNE sur `d71cd0d` |
| `/editeur/` | `scripts-schema.html`, `script-editeur.js` identiques à Git |
| Nom local de `.141` | `jardin-03` → `http://jardin-03.local` (nom donné par le propriétaire en ligne) |
| Dernier numéro de réponse du chat source | `AQL-R009` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul, `ProgrammeArrosage_s3` seule cible
firmware. **Reste à faire : `docs/TODO.md`** (fait foi). Décisions : D018,
D019 dans `docs/codex/02_DECISIONS.md`. Sécurité :
`docs/security/CYBERSECURITY_ARCHITECTURE.md`,
`docs/security/SECURITY_RISK_REGISTER.md` (SEC-024 à SEC-026).

## 3. Ce qui a été fait

### 3.1 P3.11 — pages du module sans script inline (5.13.4, fusion `a0b3434`)

- `sante`, `diagnostic`, `setup`, `logs`, `cloudsync` : script copié dans un
  `.js` du même nom, gestionnaires en `addEventListener` / `data-go`.
  `sante` et `cloudsync` : navigation en haut, deux colonnes sur PC.
- `index.html` + `app.js` : 79 + 27 attributs `on*=` renommés `data-on*=`
  (même expression), répartiteur sans `eval` dans `data/index.js`
  (grammaire restreinte, remontée et `stopPropagation` rejoués).
- Banc réutilisable **`tools/csp_banc_pages.py`** (CSP `script-src 'self'`,
  GET relayés vers `.141`, écritures jamais transmises, témoin négatif) : 63/63.

### 3.2 D018 — signature des mises à jour Web (5.14.0, fusion `0c53057`)

- `tools/update_signing.py` (`creer`, `signer`, `verifier`, `entete`) ;
  clé publique `tools/cles_publiques/37c2c8068db00bd9.pem` → en-tête généré
  `src/UpdateSignatureKeys.h`.
- **Clé privée : `C:\Users\Emman\.aqualook\signature\cle-privee.pem`**
  (hors dépôt, hors OneDrive, droits au seul compte). **Sauvegarde hors ligne
  à faire par le propriétaire** (SEC-025).
- Module : `src/UpdateSignature.*` (mbedTLS ECDSA P-256), appelé par
  `WebAssetsUpdater::checkForUpdate()` et `deployFromManifest()` avant
  l'analyse ; `<url>.sig` à côté du manifeste ; refus sans repli
  (`signature-absente`, `-illisible`, `-cle-inconnue`, `-invalide`),
  aides dans `data/ota-help.json`, journal `[MAJ-SIGNATURE]`.
- `publish_web_assets.py` signe ; ordre d'envoi : **fichiers → `.sig` →
  manifeste** ; `check_published_web.py` contrôle la signature.
- Validé sur `.141` : signé accepté, falsifié refusé, non signé refusé,
  pages intactes.

### 3.3 D019 — mises à jour visibles partout, UI cohérente, nom local (5.15.x, fusion `d71cd0d`)

- **Règle du propriétaire : UI identique module / site** (mémorisée) :
  pastille « ⬆ MAJ » violette `#6633cc` clignotante, lignes
  « Programme du module · installé X » / « Pages Web · installé Y →
  disponible Z » (classes `upd-*` communes).
- **Serveur** : `/v1/report` répond `webAvailable` (manifeste du site) et
  `label` (nom donné au module) ; `/app/modules` ajoute `web_version`,
  `web_available`, `net_name` (dernier rapport `diag`).
- **Site** : pastille dans le bandeau, carte violette, liste relue chaque
  minute, bouton « Mettre à jour … : comment faire » ; **`maj.html` +
  `maj.js`** : 8 étapes une à la fois, SVG des vrais écrans, étape 3 violette
  avec lien `http://<nom>.local` (lien direct `#etape-N`). Éditeur en ligne :
  bandeau du site (titre, pastille, « ‹ Mes modules »), y compris sur
  téléphone.
- **Module** : rapport `diag` + `webAssets`, `netName` ; annonce reçue →
  `NotificationManager::setCloudWebUpdate()` → LCD « MAJ DISPO », voyant
  violet, pastille Web, `ota.html` et rubrique Mises à jour
  (`cloudWebAvailable`) ; synchro immédiate au premier démarrage après une
  mise à jour réussie (`requestSyncAfterUpdate`).
- **Nom local** `src/NetName.*` : nom donné en ligne (translittéré, 32 car.),
  sinon `aqualook-<fin MAC>` ; NVS `aq_net` ; mDNS + nom DHCP ; affiché sur
  ADMIN › WiFi du LCD. Coût mesuré : ~7 Ko de tas (178 Ko libres).

## 4. Fichiers principaux

| Zone | Fichiers |
|---|---|
| Firmware | `src/UpdateSignature.*`, `src/UpdateSignatureKeys.h`, `src/NetName.*`, `src/WebAssetsUpdater.cpp`, `src/CloudSync.*`, `src/NotificationManager.*`, `src/ScreenManager.cpp`, `src/DisplayPlanningDecor.cpp`, `src/DisplayManager.cpp`, `src/WebManager.h`, `src/main.cpp`, `src/ConfigManager.cpp`, `src/WebManager.cpp` (commentaires) |
| Pages module (`data/`) | `index.html`, `index.js`, `app.js`, `sante.*`, `diagnostic.*`, `setup.*`, `logs.*`, `cloudsync.*`, `ota.js`, `ota-help.json`, `scripts-schema.html`, `script-editeur.js` |
| Service en ligne | `cloud/php-mutualized/{auth.php,index.php,app.html,app.js,maj.html,maj.js}` |
| Outils | `tools/csp_banc_pages.py`, `tools/update_signing.py`, `tools/publish_web_assets.py`, `tools/check_published_web.py`, `tools/log_messages.tsv`, `tools/cles_publiques/` |
| Docs | `docs/codex/02_DECISIONS.md` (D018, D019), registre SEC-024..026, référentiel §4.4, `docs/TODO.md` |

Volontairement non modifiés : `littlefs/` (buildfs non requis), schéma NVS
de configuration (nouvel espace `aq_net` indépendant), routes existantes.

## 5. Invariants préservés

- I6/I7 : relais et durée maximale intacts ; aucune mise à jour déclenchée à
  distance (D014) : le serveur **informe**, l'utilisateur installe.
- D016 : toute écriture sous session ; filtre d'hôte inchangé (`.local` déjà admis).
- Pages Web sans script inline (sauf pages intégrées au firmware, P3.13).
- Identité de build : `VERSION` source unique ; `.141` vérifié par `/api/diagnostics`.

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | SUCCESS (build 1490, Flash 80,6 %) |
| buildfs | non requis |
| Web module | 5.15.2 sur `.141` par OTA signé |
| LCD | « MAJ DISPO » sur annonce en ligne, nom `.local` sur ADMIN › WiFi (validés par le propriétaire) |
| Matériel | `.141`, COM4, aucun essai relais nouveau (pas de changement relais) |
| Bancs | CSP pages 63/63, éditeur 77/77, site isolé 17/17 (MariaDB locale, base `aqualook_banc_d`), sonde sécurité site 32 PASS |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (capture d'écran) |

## 7. Risques et limites

- **Sauvegarder la clé privée de signature** (sinon : plus de publication
  possible sans nouveau firmware sur chaque module).
- Signature non encore appliquée au manifeste **firmware** ni anti-rejeu
  d'un ancien manifeste signé (P1.9 lot 2).
- `/api/debug/*` (dépôt direct SD) à fermer avant mise en service (P1.8).
- Synchro immédiate après mise à jour : attend la fin d'un arrosage ; sur le
  banc, le script « Legitime » relance la zone 1 et la retarde (P3.15).
- `/editeur/` contient d'anciennes copies de pages du module (P3.14).
- `style-base.css` importe Google Fonts (P3.12) ; pages firmware inline (P3.13).
- Android : certains navigateurs ne résolvent pas `.local` → l'IP du LCD
  reste le recours (expliqué dans `maj.html`).
- Deux modules portant le même nom en ligne : collision mDNS possible.

## 8. TODO

Voir **`docs/TODO.md`**. Prochaines étapes suggérées : P1.8, P1.9 lot 2,
P3.12, P3.14, puis P0.5 (étude HTTPS LAN), P2.8 (mails), P2.10 (IA).

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
python tools/update_signing.py verifier dist/alwaysdata/aqualook-web-manifest.json
```

Attendu : « Serveur ALIGNE », 27/27 + signature ok. Publier un paquet Web :
`python tools/publish_web_assets.py X.Y.Z` (signe), puis envoyer fichiers →
`.sig` → manifeste. Pour un développement firmware, demander le port COM,
puis :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

Banc site isolé : copie du service + `.env` de banc (MariaDB portable
`C:\Users\Emman\mariadb-local`, lancée avec `--defaults-file=...\data\my.ini`,
port **3306**, base `aqualook_banc_d`) ; `/health` doit montrer
`config.dbPassword=false`.

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent b798cc0..main   # cette session
git show a0b3434 --stat                          # P3.11
git show 0c53057 --stat                          # D018
git show d71cd0d --stat                          # D019
```
