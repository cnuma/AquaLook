# Checkpoint AquaLook — 12 octobre 2026 (nuit) — paquet Web 5.13.2 publié et installé (P2.1), règle d'ergonomie Web

Document de reprise autonome. Avec `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`
et **`docs/TODO.md`** (liste unique du reste à faire), il suffit pour
reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-12_1139d12.md`.

> **En un coup d'œil :** le paquet Web des modules passe de `5.10.3` à
> **`5.13.2`** (nouvel éditeur de scripts compris) : publié sur AlwaysData,
> vérifié 20/20, installé sur `.141` et relu octet pour octet. Nouvelle
> règle d'ergonomie du propriétaire (navigation en haut, largeur PC
> exploitée), consignée dans les règles de développement. Trois entrées
> ajoutées à la TODO (P2.11, P2.12, P3.11). Aucun changement de code :
> firmware, serveur PHP et `data/` inchangés.

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public, open source par choix du propriétaire) |
| Branche | `main` (branches de la session fusionnées `--no-ff`, poussées, supprimées) |
| Commit fonctionnel | **`4f03e1f`** |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-12_4f03e1f.md`) |
| Version fonctionnelle (`VERSION`) | **`5.13.2`**, inchangée ; pas de tag |
| Firmware sur `.141` | `ad75d36`, build 1390, `5.13.2` (non reflashé) |
| Pages Web sur `.141` | **`5.13.2`** (mise à jour Web lancée par le propriétaire depuis `ota.html`) |
| Paquet Web en ligne | **`5.13.2`** (`/aqualook-web-manifest.json`, fichiers sous `/web/v5.13.2/`) |
| Service AlwaysData (PHP) | aligné sur Git : `deploy_cloud.py verifier` → « Serveur ALIGNE sur c7cdec3 » en début de session (aucun fichier serveur modifié depuis) |
| Dernier numéro de réponse du chat source | `AQL-R006` |

## 2. Source de vérité

`main` au commit de ce document. V4 seul, `ProgrammeArrosage_s3` seule cible
firmware. **Reste à faire : `docs/TODO.md`** (fait foi). Sécurité :
`docs/security/CYBERSECURITY_ARCHITECTURE.md` et
`docs/security/SECURITY_RISK_REGISTER.md`.

## 3. Ce qui a été fait (commits sur `main`)

| Commit | Sujet | Validation |
|---|---|---|
| `850ce6b` | TODO **P2.11** : clignotement, sur le LCD, de la barre du créneau programmé pendant l'arrosage qu'il a déclenché | doc |
| `03662ae`, `5f82ba4` (fusion `25bfc96`) | **P2.1 fait** : paquet `5.13.2` | voir ci-dessous |
| `880141b` | Règle d'**ergonomie Web** dans `docs/codex/04_DEVELOPMENT_RULES.md` (§ Web › Ergonomie) | doc |
| `2109f40` (fusion `4f03e1f`) | TODO **P2.12** (disposition PC de `ota.html`) et **P3.11** (scripts inline des pages du module) | doc |

### P2.1 — chaîne réellement exécutée

1. `python tools/publish_web_assets.py 5.13.2 --comparer` : 20 fichiers
   copiés depuis `data/` dans `dist/alwaysdata/web/v5.13.2` (un normalisé en
   LF), relus contre le manifeste ; manifeste **4 741 octets** (limite du
   module `MANIFEST_MAX_BYTES` = 8 192, `src/WebAssetsUpdater.h`). Fichiers
   changés par rapport à `5.10.3` : `app.js`, `cloudsync.html`,
   `index.html`, `logs.html`, `ota.html`, `sante.html`, `script-editeur.js`,
   `scripts-schema.html`, `session.js`, `setup.html`.
2. Envoi FTP par le propriétaire : le dossier `v5.13.2` dans `/web/`, puis
   le manifeste à la racine, en dernier.
3. `python tools/check_published_web.py
   https://aqualook.alwaysdata.net/aqualook-web-manifest.json` : **20/20
   conformes** ; manifeste en ligne identique octet pour octet à celui
   préparé.
4. Mise à jour Web de `.141` par le propriétaire (bouton « Installer les
   pages Web 5.13.2 » de `ota.html`) : « tout est à jour ».
5. Relecture HTTP des 20 fichiers servis par `http://192.168.1.141/` :
   **20/20 identiques** au paquet.
6. « Changer le secret » depuis l'éditeur du module : confirmé fonctionnel
   par le propriétaire.

### Règle d'ergonomie (propriétaire, imposée progressivement)

Navigation **en haut de la page**, toujours accessible ; une page utilisée
depuis un PC exploite la largeur pour limiter le défilement vertical ; une
seule chose à la fois (menu d'options). S'applique à chaque page créée ou
retouchée. Premier chantier : `ota.html` (P2.12).

## 4. Fichiers et fonctions modifiés

| Fichier | Élément |
|---|---|
| `docs/TODO.md` | P2.1 (fait), P2.11, P2.12, P3.11 |
| `docs/codex/04_DEVELOPMENT_RULES.md` | § Web › Ergonomie |
| `docs/checkpoints/CHECKPOINT_2026-10-12_4f03e1f.md` | ce document |

Volontairement non modifiés : firmware (`src/`, `include/`, `lib/`),
`littlefs/`, `data/`, `cloud/`, `platformio.ini`, `VERSION`. `dist/` est
ignoré par Git (zone de préparation).

## 5. Invariants préservés

- Aucun changement firmware, relais, planificateur, NVS, LittleFS.
- Mise à jour Web déclenchée par l'utilisateur (invariant D014), jamais par
  le serveur ni par l'agent.
- Ordre de publication respecté : fichiers puis manifeste.
- Aucun `.env` envoyé par FTP ; aucun secret dans le dépôt ni dans le chat.

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | non requise (aucun fichier firmware touché) ; dernier build 1390 SUCCESS |
| buildfs | non requis (`littlefs/` inchangé) |
| Web module | `5.13.2` sur `.141`, vérifié par relecture HTTP |
| LCD / matériel | inchangés, aucun relais sollicité ; port du banc : COM4 (confirmé par le propriétaire pour cette session, inutilisé) |
| Service en ligne | paquet Web `5.13.2` ; PHP aligné sur Git |
| `git diff --check` (`cr-at-eol`) | propre |
| Dépôt | `main` = `origin/main` ; un fichier non suivi sans rapport (`Capture d'écran 2026-10-11 161236.png`, au propriétaire) |

## 7. Risques et limites

- Sept pages du module contiennent encore un `<script>` inline ou des
  `on*=` (P3.11) ; les pages du service en ligne sont, elles, sous CSP
  stricte.
- `tools/apercu_web/apercu.py` hors service depuis P0.4b (inchangé).
- `cloud/php-mutualized/.env` local = réglages de production : banc PHP
  uniquement dans une copie isolée (procédure au §9 du checkpoint
  `1139d12`).
- Les anciens dossiers `/web/v5.9.x`, `/web/v5.10.x` restent sur le serveur
  (inoffensifs, rien ne les annonce).

## 8. TODO

Voir **`docs/TODO.md`**. Prochaines étapes proposées :

1. **P2.11** clignotement LCD (firmware : compilation, flash `.141`, essai
   sur un créneau court, une zone, sous surveillance).
2. **P2.12** disposition PC de `ota.html` (+ sortie du script inline, P3.11).
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
« Publication coherente : 20/20 ». Publication du service :
`python tools/deploy_cloud.py publier` (lancé par le propriétaire). Paquet
Web : `python tools/publish_web_assets.py <version>` puis FTP manuel
(fichiers, manifeste en dernier). Pour un développement firmware, demander
le port COM, puis :

```powershell
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

## 10. Commandes Git utiles

```powershell
git log --oneline --first-parent c7cdec3..main   # cette session
git show 03662ae                                 # P2.1 dans la TODO
git show 880141b                                 # règle d'ergonomie
git show 2109f40                                 # P2.12, P3.11
```
