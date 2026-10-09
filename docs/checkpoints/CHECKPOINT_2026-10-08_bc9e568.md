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
6. **Prochaine session (demande de l'utilisateur, 9 oct. 2026)** : gestion
   des modules et des comptes depuis l'interface Web AlwaysData — voir §11.
7. **Sécurité locale du module (demande de l'utilisateur, 9 oct. 2026)** :
   code PIN sur le LCD et accès sécurisé à l'interface Web du module — voir
   §12.
8. **Erreur SD à examiner (demande de l'utilisateur, 9 oct. 2026)** :
   `/api/faults` sur `.141` (`bc9e568`) rend `unacknowledged: true`,
   `active: false`, `lastErrorMessage` = « Stockage: SD indisponible
   raison=health_check_failed chemin=/www/index.html ». Journal : épisode
   isolé vers 17:46:30, `SD recuperee essai=1 lentes=0 indisponible=2s`,
   `ressources Web SD validees dans /www`, « Incident SD: recupere (episode
   isole, sans notification) » ; la ligne d'origine était déjà sortie du
   tampon HTTP. À examiner : ce qui déclenche le contrôle de santé sur
   `/www/index.html`, l'activité concurrente à cet instant (requêtes Web,
   éditeur, CloudSync), et si une erreur récupérée en 2 s doit rester « non
   acquittée ». Capture série recommandée pour le prochain épisode.
9. **Éditeur de scripts sur tablette (demande de l'utilisateur, 9 oct.
   2026)** : dans `data/scripts-schema.html`, rendre repliables les deux
   volets latéraux — palette de blocs (`aside.palette`, à gauche) et
   réglages du bloc (`aside.props-panel`, à droite) — avec le même triangle
   que la zone du bas (`#b-replier`, classe `.replie`, préférence mémorisée
   dans le navigateur). Sur un écran type iPad, les deux volets prennent
   trop de largeur et la zone du bas devient difficile à utiliser. Toute
   modification se recopie dans `/editeur/` sur AlwaysData.

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

## 11. Prochaine session — gestion des modules et des comptes (espace en ligne)

Demande de l'utilisateur, ajoutée le 9 oct. 2026. Évolution significative :
commencer par une décision documentée (D016) avant tout code.

### État actuel (vérifié dans `cloud/php-mutualized/`)

- Comptes créés **par l'administrateur seulement** (`/admin/user`) ; tables
  `app_user`, `app_session`, `login_attempt` (`schema-v2-comptes.sql`).
- Rattachement module → compte par l'administrateur
  (`/admin/module/owner`, colonne `module.owner_user_id`).
- Jeton du module émis par `/admin/module-token`, **recopié à la main**
  dans le module (CloudSync, NVS).
- Ni inscription, ni réinitialisation de mot de passe, ni envoi de mail.

### Demandé

1. **Enrôlement et suppression des modules par l'utilisateur** depuis
   l'interface Web.
2. **Enregistrement unique des modules** dans la base AlwaysData (identifiant
   stable, contrainte d'unicité, transfert d'un compte à l'autre maîtrisé).
3. **Identifiant et poignée de main depuis le LCD** : l'utilisateur lit sur
   l'écran de quoi rattacher *ce* module à *son* compte, sans se tromper de
   module.
4. **Mot de passe oublié et réinitialisation** depuis l'interface Web.
5. **Analyser la documentation AlwaysData** pour l'envoi de mails
   (réinitialisation, premier enrôlement) : `mail()` PHP ou SMTP du compte,
   adresse d'expédition, SPF/DKIM du domaine, limites d'envoi.

### Pistes de conception à évaluer

- **Enrôlement « code court » (modèle *device authorization*, RFC 8628)** :
  le module, jamais enrôlé ou réinitialisé, appelle `POST /v1/enroll/start`
  avec son identifiant matériel (MAC eFuse / ID puce) ; le serveur rend un
  code utilisateur court (8 caractères, sans ambiguïté O/0, I/1) valable
  ~10 min et usage unique ; le LCD l'affiche avec l'identifiant du module ;
  l'utilisateur le saisit dans son espace ; le module interroge
  `POST /v1/enroll/poll` et reçoit **son jeton** — plus de copie manuelle.
  Limiter les tentatives (comme `login_attempt`), ne stocker que des
  empreintes (SHA-256) des codes et jetons.
- **Suppression / désenrôlement** : révoquer le jeton côté serveur, choix
  explicite entre conserver ou purger l'historique et les sauvegardes ;
  pendant symétrique sur le LCD (« oublier le compte en ligne »).
- **Mot de passe** : table de jetons de réinitialisation (empreinte, expiration
  30-60 min, usage unique) ; réponse identique que le compte existe ou non
  (pas d'énumération des adresses) ; fermer toutes les sessions après
  réinitialisation ; limite de demandes par adresse et par IP.
- **Inscription** : libre avec vérification de l'adresse mail, ou sur
  invitation — à décider.

### Autres services proposés (à arbitrer par l'utilisateur)

- Alertes par mail : module hors ligne depuis X heures, défaut signalé
  (`FaultManager`), arrosage annulé ou en échec ; réglables par module.
- Partage d'un module avec un autre compte (lecture seule ou gestion).
- Sessions actives visibles, avec déconnexion à distance ; historique des
  connexions.
- Double authentification (TOTP) optionnelle.
- Journal d'audit : qui a envoyé quelle commande, quand, avec quel résultat.
- Renommer un module, lui donner un lieu et un fuseau horaire.
- Rotation du jeton d'un module depuis l'espace.
- Annonce d'une nouvelle version firmware/Web disponible (**information
  seulement** : la mise à jour reste déclenchée par l'utilisateur sur le
  module, invariant).
- Export de ses données et suppression de compte (RGPD).

### Points d'attention

- Le dépôt est public : aucun identifiant SMTP ni secret dans Git (`.env` du
  serveur seulement).
- L'interface locale du module n'a toujours pas d'authentification
  (`REPRISE_INGENIEUR.md` §1.5) : l'enrôlement ne doit pas permettre à un
  poste du LAN de rattacher le module à un autre compte sans action visible
  sur le LCD.
- Le firmware actuel ne connaît que le jeton manuel : garder ce chemin
  pendant la transition (modules déjà enrôlés, mode maintenance).

## 12. Sécurité locale du module : PIN sur le LCD, accès Web sécurisé

Demande de l'utilisateur, ajoutée le 9 oct. 2026. Liée à §11 (l'enrôlement
doit s'appuyer sur une action protégée sur le LCD) et au trou de sécurité
assumé de `REPRISE_INGENIEUR.md` §1.5. Commencer par une décision documentée
(D017, ou D016 commune avec §11).

### État actuel

- **LCD** : écran `ADMIN` accessible sans code ; démarrage et arrêt manuel des
  zones depuis l'écran sans protection.
- **Web local** : verrou **visuel** seulement dans `data/index.html`
  (`sessionStorage` `aqualook-admin-unlocked`, mot de passe `1598753` écrit
  en clair dans la page) — n'est pas une authentification.
- Routes `/api/*` ouvertes à tout poste du LAN (`/api/resetConfig`,
  `/api/zone`, Wi-Fi…) ; seules quelques écritures récentes (scripts,
  câblage, variables, redémarrage) sont signées HMAC par `ApiAuth`
  (secret partagé posé depuis le navigateur, effaçable depuis ADMIN >
  Système).

### Demandé

1. **Code PIN sur le LCD** pour agir sur la configuration et pour la gestion
   directe des zones (démarrage/arrêt manuel).
2. **Accès sécurisé à l'interface Web de gestion du module.**

### Pistes de conception à évaluer

- **PIN LCD** : 4 à 6 chiffres sur un pavé tactile ; stocké en NVS sous forme
  d'empreinte salée (jamais en clair) ; temporisation croissante après
  échecs ; déverrouillage valable N minutes ou jusqu'à la mise en veille ;
  périmètre réglable (ADMIN seul, ou ADMIN + actions manuelles sur les
  zones) ; consultation (état, planning, météo) toujours libre.
  **Sécurité relais** : un arrêt d'urgence d'une zone en cours doit rester
  possible sans PIN.
- **Récupération d'un PIN oublié** : procédure physique (bouton au
  démarrage, ou geste sur le LCD pendant le splash) ou depuis l'espace en
  ligne pour un module enrôlé — jamais par une route Web locale ouverte.
- **Web local** : remplacer le verrou visuel par une vraie session —
  mot de passe (ou le même PIN) vérifié **par le module**, jeton de session
  en cookie `HttpOnly`, expiration, limitation des essais ; toutes les
  routes d'écriture exigent la session (lecture d'état éventuellement
  libre) ; retirer le mot de passe écrit en clair de `index.html`.
  Articuler avec `ApiAuth` (HMAC) : garder la signature pour les écritures
  sensibles ou la remplacer par la session — à décider.
- **HTTP en clair sur le LAN** : un mot de passe circulerait en clair ;
  évaluer un défi-réponse (nonce + HMAC, déjà le principe d'`ApiAuth`)
  plutôt qu'un envoi du mot de passe, HTTPS local étant lourd sur l'ESP32.
- **Premier démarrage / portail captif** : définir le PIN et le mot de passe
  à la configuration initiale ; ne pas bloquer un module déjà installé lors
  de la mise à jour (PIN absent = comportement actuel jusqu'à ce que
  l'utilisateur en pose un, avec un rappel visible).

### Points d'attention

- Persistance : nouvelle clé NVS versionnée, hors du bloc `ALOK`
  (`AGENTS.md` §Persistance) ; réinitialisation de la configuration ≠ oubli
  du PIN, à trancher.
- IDs et routes existants à conserver (F5, F6) ; `/api/diagnostics` et
  `/api/logs.txt` restent utiles au diagnostic : décider s'ils restent
  lisibles sans session.
- L'outillage de banc (`tools/`, dépôt direct sur la SD `/api/debug/*`,
  `announce_reboot.py`) devra s'authentifier ou disposer d'un accès de
  développement explicite, désactivé en production.
