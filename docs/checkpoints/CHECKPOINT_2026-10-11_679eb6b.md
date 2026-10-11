# Checkpoint AquaLook — 11 octobre 2026 — campagne sécurité du site cloud

Document de reprise autonome. Avec `AGENTS.md` et `docs/REPRISE_INGENIEUR.md`,
il suffit pour reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-10_a721b81.md` (dont les §5 invariants et §7 risques
restent valables, sauf mention contraire) et met à jour sa TODO §8.

> **En un coup d'œil :** le **firmware n'a pas changé** depuis `a721b81`
> (version `5.13.0`, build 1368 sur `.141`). Cette session n'a touché qu'au
> **service web cloud** (`cloud/php-mutualized/`), à la documentation et aux
> outils : audit de sécurité, durcissement `.htaccess` (déployé et vérifié en
> prod) et un harnais de test réutilisable. **Rien à reflasher.**

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public) |
| Branche | `main` (branche de campagne `fix/cloud-securite-entetes-logs` fusionnée `--no-ff` puis supprimée) |
| HEAD `main` | **`679eb6b`** (merge de la campagne sécurité) ; + le commit qui ajoute CE document |
| Commit firmware fonctionnel | **`a721b81`** (version `5.13.0`) — arbre `src/`, `include/`, `platformio.ini`, `littlefs/` identique à celui du checkpoint précédent |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-11_679eb6b.md`) |
| Version fonctionnelle (`VERSION`) | **`5.13.0`** ; pas de tag ni de release GitHub |
| Firmware sur `.141` | `a721b81`, build **1368**, version `5.13.0` (vérifié par `/api/diagnostics.build`) ; inchangé cette session |
| Identifiant matériel `.141` | `aql-44bd8d7acb88` ; module serveur `Jardin-01` |
| Mot de passe Web de `.141` | mot de passe de banc, dans `.env` (ignoré par Git) |
| Dernier numéro de réponse du chat source | `AQL-R012` |
| Cible firmware | `ProgrammeArrosage_s3`, module `192.168.1.141`, **COM4** (confirmé le 10 oct. 2026, à reconfirmer) |

## 2. Source de vérité

`main` au commit de ce document. V4 seul, `ProgrammeArrosage_s3` seule cible
firmware. Service cloud : `cloud/php-mutualized/` (PHP + MySQL) sur AlwaysData
(`https://aqualook.alwaysdata.net`). Dettes : `docs/codex/08_RISKS_AND_DEBT.md`
(D9, D10, D11).

## 3. Ce qui a été fait dans cette session (campagne sécurité web)

Aucune modification firmware. Travail entièrement sur le service cloud, la doc
et les outils.

| Commit(s) | Contenu | Validation |
|---|---|---|
| `11bbfd0` | **Durcissement `.htaccess`** : `*.log` ajouté au `<FilesMatch>` (un journal PHP dans la racine web serait servi tel quel) ; bloc `<IfModule mod_headers>` : CSP, `X-Frame-Options: DENY`, `X-Content-Type-Options: nosniff`, `Referrer-Policy: no-referrer`, HSTS. `.gitignore` élargi à `*.log`. | Déployé en prod par l'utilisateur (éditeur de fichiers du panneau AlwaysData) et **vérifié actif** |
| `c22a13b` | **Rapport d'audit** `docs/security/AUDIT_WEB_2026-10-10.md` : audit statique des ~3 600 lignes de `cloud/php-mutualized/`. | — |
| `43e2d23` | **Outil** `tools/web_security_probe.py` : harnais de vérification active, non destructif, faible débit (en-têtes, non-exposition fichiers sensibles, contrôle d'accès 401, fuite d'erreur, CORS, plafond anti-force-brute opt-in). | Syntaxe `py_compile` OK ; exécuté en prod (voir §6) |
| `679eb6b` | Merge `--no-ff` de la campagne dans `main`, poussé sur GitHub. | — |

### Résultat de l'audit
Backend très sain : requêtes préparées partout (aucune injection SQL),
autorisation par propriété de module avec 404 anti-énumération, sessions
`HttpOnly; Secure; SameSite=Strict`, connexion anti-timing + anti-force-brute,
mails protégés contre l'injection d'en-tête (CR/LF), `Host` jamais utilisé
(liens via `APP_BASE_URL`), rendu client systématiquement échappé (`esc()`),
couleur de zone validée `#RRGGBB`, jeton de fragment parsé strict. **Aucune
vulnérabilité exploitable.** Les 3 constats (voir rapport) étaient de la
défense en profondeur / un cas latent, tous corrigés par `11bbfd0`.

## 4. Fichiers modifiés cette session

| Fichier | Nature |
|---|---|
| `cloud/php-mutualized/.htaccess` | blocage `*.log` + en-têtes de sécurité (`mod_headers`) |
| `cloud/php-mutualized/.gitignore` | ajout `*.log` |
| `docs/security/AUDIT_WEB_2026-10-10.md` | rapport d'audit (nouveau) |
| `tools/web_security_probe.py` | harnais de test de sécurité (nouveau) |

Volontairement non modifiés : tout l'arbre firmware (`src/`, `include/`,
`platformio.ini`, `littlefs/`, `data/`), le code PHP applicatif (jugé sain),
format NVS, routes, IDs, relais, planificateur.

## 5. Invariants préservés

- Firmware, relais, durée maximale, planificateur, moteur V4 : **non touchés**.
- Service cloud : aucune route, aucun format persisté, aucune logique métier
  modifiée — uniquement de la configuration serveur (`.htaccess`) et des
  outils/doc hors chemin d'exécution.
- Aucun secret dans Git (`.env` ignoré ; `.env.example` ne porte que des
  `CHANGEME`).
- Mises à jour toujours déclenchées par l'utilisateur (D014) : inchangé.
- Les §5 (invariants firmware) du checkpoint `a721b81` restent valables.

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` | **Non re-exécutée, non requise** : arbre firmware identique à `a721b81` (dernier build validé = 1368). `git diff --name-only a721b81 HEAD -- src/ include/ platformio.ini littlefs/` est **vide**. |
| buildfs | Non requis (`littlefs/` inchangé). |
| Dépôt | Propre, `main` synchronisé avec `origin/main`. |
| Service web prod — audit | Statique : sain. |
| Service web prod — tests actifs (11 oct., lancés par l'utilisateur via `tools/web_security_probe.py`) | **Tout au vert** : 5 en-têtes de sécurité présents ; `.env`/`.env.example`/`schema*.sql`/`router.php`/`cleanup.php`/`README.md`/`php.log`/`error.log` → 403 ; `.git/`, `.gitignore` → 404 (absents du serveur) ; `/admin/modules`, `/app/me`, `/app/modules`, `/v1/pending-command` → 401 sans jeton ; corps JSON invalide → 400 sans trace ; route inconnue → 404 ; CORS fermé (pas d'`Access-Control-Allow-Origin`) ; plafond anti-force-brute → 10×401 puis **429**. |
| `.htaccess` déployé | Par l'utilisateur (panneau AlwaysData) ; prod == version Git. |
| LCD, Web module, matériel `.141` | Non touchés cette session. |

## 7. Risques et limites

- **XSS — socle seulement.** La CSP déployée garde `'unsafe-inline'` car
  `admin.html`/`app.html` embarquent leurs scripts en inline. Le rendu étant
  déjà bien échappé, aucun XSS n'a été trouvé, mais le vrai durcissement
  (retrait de `'unsafe-inline'`) demande d'externaliser ces scripts pour passer
  à une CSP par nonce/hash. Noté dans le rapport, non fait.
- **Dérive de config.** Le `.htaccess` a été déployé en éditant le serveur, pas
  depuis Git. Les deux sont identiques aujourd'hui (test à l'appui), mais la
  règle saine est de déployer depuis Git pour éviter la divergence.
- **Tests actifs ciblés seulement.** La campagne active s'est limitée à une
  liste fixe de vérifications non destructives (hébergement mutualisé). Des
  tests plus poussés restent à scoper (faible débit, hors heures de pointe).
- **Restent valables** : §7 du checkpoint `a721b81` (dette D9 release S3 sans
  binaire ; mot de passe Web de banc ; HTTP en clair côté module ; secret NVS
  en clair) et §7 de `cd09199`.
- L'environnement agent **bloque** le probing réseau vers le serveur externe
  (« Exfil Scouting ») : les tests actifs se lancent **depuis la machine du
  propriétaire**, pas par l'agent.

## 8. TODO — reste à faire

Sécurité web (nouveau) :
1. (Optionnel) Externaliser les scripts inline de `admin.html`/`app.html` pour
   retirer `'unsafe-inline'` de la CSP.
2. (Process) Déployer le service cloud depuis Git plutôt qu'en éditant le
   serveur.
3. (Optionnel) Tests actifs plus poussés, à scoper, toujours à faible débit.

Report de la TODO firmware du checkpoint `a721b81` (inchangée) :
4. Vers le 14 oct. : relire l'historique Wi-Fi (`module_message`, `payload.wifi`,
   `Jardin-01`) et décider du **point 2** (bascule de nœud en cours de
   connexion). Branche `feature/wifi-diag-bssid` conservée pour cela.
5. Tester la reconnexion après coupure réelle d'un nœud + un redémarrage
   logiciel. (Les BSSID des nœuds vus par `.141` : `00:11:32:A4:35:15`,
   `…:CE:6F:A2`, `…:D3:9F:05`.)
6. Phase 6 : SD retirée (`/login`, `/ota` intégrées), portail captif.
7. Recopier `data/scripts-schema.html` et `data/session.js` dans `/editeur/`
   (AlwaysData) ; paquet Web `5.10.4`.
8. Lot G (effacement du PIN depuis l'espace en ligne).
9. D9 : chaîne de release S3, puis retrait de `_legacy`/`_v4`.
10. Avant mise en service : mot de passe Web définitif.

## 9. Procédure exacte de reprise

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -3
git status --short
git -c core.whitespace=cr-at-eol diff --check
```

Le firmware fonctionnel est `a721b81` (build 1368 sur `.141`) : **rien à
reflasher** pour reprendre. Pour un nouveau développement firmware, demander le
port COM, puis la chaîne habituelle (`announce_reboot.py`, `pio run -e
ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>`, moniteur `--dtr 1`).

Revérifier la sécurité du site (depuis la machine du propriétaire, pas l'agent) :
```powershell
python tools/web_security_probe.py
python tools/web_security_probe.py --include-ratelimit   # + plafond anti-force-brute
```

Rapport d'audit : `docs/security/AUDIT_WEB_2026-10-10.md`.
Flash de `.141` et dépôt SD autorisés d'office ; port COM à reconfirmer par
session. Pièges : `git diff --check` avec `-c core.whitespace=cr-at-eol` ;
chaque connexion refusée compte dans la limite par IP ; `announce_reboot.py`
modifie `tools/soak/ledger.json` ; heredoc qui mange les antislashs.

## 10. Commandes Git utiles

```powershell
git log --oneline a721b81..main                 # campagne securite (cloud/docs/tools)
git show 679eb6b                                # merge de la campagne
git diff a721b81 HEAD -- src/ include/ platformio.ini littlefs/   # VIDE : firmware inchange
```
