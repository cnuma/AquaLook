# Checkpoint AquaLook — 10 octobre 2026 (soir) — D016 lot F : session Web locale et durcissement

Document de reprise autonome. Avec `AGENTS.md` et `docs/REPRISE_INGENIEUR.md`,
il suffit pour reprendre dans un nouveau chat. Il prend la suite de
`CHECKPOINT_2026-10-10_19810d9.md` (lot B), dont la TODO §8 est reprise et
mise à jour ici (§8).

## 1. Identité

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook` (GitHub, public) |
| Branche | `main` ; `feature/session-web-locale` fusionnée (fast-forward) et poussée |
| Commit fonctionnel | `f0e7a8a` |
| Commit officiel de reprise | le commit qui ajoute CE document (`git log -1 -- docs/checkpoints/CHECKPOINT_2026-10-10_f0e7a8a.md`) |
| Branche en attente | `feature/lot-f-b1-b5` (`158eec8`) : B1 + B5 compilés, **non flashés, non testés** |
| Version fonctionnelle (`VERSION`) | `5.11.0-dev` (inchangée ; proposition : `5.12.0` une fois B1/B5 validés) |
| Firmware sur `.141` | code de `ea6a0df` (= `main` hors documentation), build **1344**, `gitSha` affiché `25458e5` (compilé avant le commit `ea6a0df`, limite connue) |
| Identifiant matériel `.141` | `aql-44bd8d7acb88` ; module serveur `Jardin-01` |
| Mot de passe Web de `.141` | **mot de passe de TEST** posé par l'agent, dans `.env` (`AQUALOOK_WEB_PASSWORD=`, ignoré par Git) |
| Dernier numéro de réponse du chat source | `AQL-R015` |
| Cible | `ProgrammeArrosage_s3`, module de test `192.168.1.141`, **COM4** (confirmé le 10 oct. 2026, à reconfirmer) |

## 2. Source de vérité

`main` au commit de ce document. Décision : **D016** (`docs/codex/02_DECISIONS.md`,
complément lot F) ; conception et classement des routes :
`docs/architecture/ENROLEMENT_ET_SECURITE_LOCALE.md` §8, **§8.1 (W1)**, §9
(état du lot F). V4 seul, `ProgrammeArrosage_s3` seule cible.

## 3. Ce qui a été fait

| Commit | Contenu | Validation |
|---|---|---|
| `decf3d5` | Classement W1 de toutes les routes (§8.1) ; décisions du propriétaire : arrêt d'une zone sous session, Wi-Fi libre en portail captif | documentation |
| `3e0c22d` | `WebSession` (défi-réponse HMAC, cookie `aqls` HttpOnly SameSite=Strict, 4 sessions, 30 min), filtre `SessionGate`, `/api/session/*`, gardes installées avant toute route (main.cpp enregistrait SD/défauts/maintenance avant `begin()`, ce qui contournait aussi la garde d'URL) | `.141` : 401/200 attendus |
| `3e18ef4` | Durcissement : essais par IP (oubli 1 h), `hasSecret()` en échec fermé, anti « DNS rebinding » (421), anti CSRF (403, même sans mot de passe), `X-Frame-Options`/`nosniff`, page `/login` intégrée au firmware | `.141` ; HMAC JS = Python (4 vecteurs) |
| `06dde42` | `data/session.js`, `index.html` sans `1598753`, session.js dans 6 pages, éditeur sans secret en `localStorage`, `tools/module_session.py`, outils de soak branchés | dépôt SD (SHA identiques), phases 1-2 : 34/34 |
| `97df868` | Saisie du mot de passe masquée (`AquaSession.promptSecret`) | `.141` |
| `25458e5` | `GET /` → `/login` sans session | `.141` : 302 → `/login` / `/index.html` |
| `ea6a0df` | `/login` sans déconnexion ; bouton « Options avancées du module » (repli de la configuration, pas un verrou) ; menu Paramètres sans rubriques vides | `.141` + Edge ; navigateur validé par le propriétaire (« good ») |
| `f0e7a8a` | Documentation alignée (00, 02, 08, ENROLEMENT §8.1/§9), registre de soak | — |
| `158eec8` (branche `feature/lot-f-b1-b5`) | **B1** : `/api/auth-secret` forme chiffrée (`nonce`, `enc`, `mac`), forme en clair refusée dès qu'un secret existe ; **B5** : premier secret accepté seulement dans une fenêtre de 10 min ouverte sur l'écran (ADMIN > Système « Autoriser un mot de passe Web », ouverte aussi par « Oublier ») | compilé S3 (RAM 26,3 %, flash 78,7 %), **non flashé** |

## 4. Fichiers et fonctions modifiés

| Fichier | Éléments |
|---|---|
| `src/WebSession.h/.cpp` (nouveaux) | `newChallenge`, `login` (par IP), `openTrusted`, `isValid`, `remainingSec`, `close`, `closeAll`, `activeCount`, `failureEntry` |
| `src/ApiAuth.h/.cpp` | `verifyMessage`, cache `g_hasSecret`, `hasSecret` (API NVS, échec fermé) |
| `src/WebManager.h/.cpp` | `installGuards`, `SessionGate`, `hostAllowed`, `crossSiteWrite`, `sendWithSessionCookie`, `cookieOf`, `LOGIN_HTML`, routes `/login`, `/api/session/challenge|state|login|logout`, `/`, `handleSetApiSecret`, en-têtes de `sendEmbeddedPage` |
| `src/SdStaticHandler.cpp` | en-têtes `X-Frame-Options`, `nosniff` |
| `src/DisplayManager.cpp` | `WebSession::closeAll()` à l'oubli ; libellé « Oublier le mot de passe Web » |
| `data/session.js` (nouveau) | `refresh`, `login`, `logout`, `setPassword`, `changePassword`, `ask`, `promptSecret`, enveloppe `fetch` |
| `data/index.html` | `adminApply`, `adminShow`, `adminSetPassword`, `adminToggle`, `avanceOuvert` ; bouton `admin-btn` (ID conservé) |
| `data/app.js` | `buildCfgMenu`, `cfgOrphanSections` (rubriques masquées exclues) |
| `data/scripts-schema.html` | `demanderSecret`, `secret` (sessionStorage), `signer`, `definirSecret` |
| `data/sante|logs|cloudsync|ota|setup.html` | inclusion `hmac.js` + `session.js` |
| `tools/module_session.py` (nouveau) | `read_password`, `ModuleSession`, `open_session`, `install_global_session`, `deploy`, CLI `state|login|post|deploy` |
| `tools/soak/apply_soak_planning.py`, `restore_planning.py`, `cram_cycles.py` | `install_global_session` |

Volontairement non modifiés : `littlefs/`, `platformio.ini`, format NVS `ALOK`
(le secret reste `aq_apikey` dans le namespace `aqualook`), `VERSION`,
routes et IDs existants (nouvelles routes ajoutées seulement), serveur
AlwaysData.

## 5. Invariants préservés

- Relais, durée maximale, planificateur : non touchés ; l'arrêt d'une zone
  reste libre sur l'écran (D012).
- Une requête refusée par le filtre ne voit jamais son corps traité
  (gestionnaire choisi à la fin des en-têtes).
- Aucun secret dans Git ; le mot de passe ne circule jamais à la connexion.
- Sans mot de passe posé : comportement d'avant + bandeau « Accès non
  protégé » (les gardes CSRF / DNS restent actives).
- Pages SD anciennes : `/login` intégrée ; firmware ancien : `session.js`
  laisse tout ouvert (404 sur `/api/session/state`).

## 6. État

| Élément | État |
|---|---|
| Compilation `ProgrammeArrosage_s3` (`main`) | SUCCESS — RAM 26,3 % (86 044 o), Flash 78,6 % (1 545 945 o) |
| buildfs | non requis (`littlefs/` inchangé) |
| `.141` | flashé (COM4) avec `ea6a0df` ; pages SD déposées (SHA identiques) |
| Web | validé : automatisé 34/34 + navigateur (propriétaire) |
| LCD | libellé « Oublier le mot de passe Web » ; oubli réel exécuté par le propriétaire à 11:54:09 |
| AlwaysData | inchangé. **À recopier dans `/editeur/`** : `scripts-schema.html` modifié et `session.js` (sinon 404 bénin) |

## 7. Risques et limites

- **Tant que B1 n'est pas flashé, « changer le secret » dans l'éditeur
  échoue** (la page envoie déjà la forme chiffrée, le firmware de `main`
  l'ignore) : contournement « Oublier » sur l'écran puis bandeau.
- Sans B5, la première pose reste possible depuis n'importe quel poste du
  LAN tant qu'aucun mot de passe n'existe (récupération par l'écran).
- HTTP en clair : cookie captable 30 min au plus (risque assumé D016).
- Échange de connexion capté = attaque hors ligne sur le mot de passe :
  12 caractères minimum, phrase de passe conseillée.
- Garde de nom d'hôte : un accès par un nom DNS public personnalisé est
  refusé (421) ; passer par l'IP, `.local`, `.lan`, `.home`…
- Pas de déconnexion dans l'interface (voulu) : 30 min ou redémarrage.
- Les outils de robustesse (`tools/robustesse/fuzz*.py`) n'ouvrent pas de
  session : leurs écritures sont refusées (401).
- Le secret reste en clair dans la NVS (accès physique ; sauvegardes
  `nvs_backup.py` à ne jamais committer).
- Restent valables : §7 des checkpoints `19810d9`, `7bc3990`, `370a6eb`.

## 8. TODO — reste à faire

1. **Flasher `feature/lot-f-b1-b5`** sur `.141`, puis : phase 5 (changement
   chiffré depuis l'éditeur, forme en clair refusée 400, mauvais actuel 403)
   et phase 4 (oublier sur l'écran → pose refusée sans geste → « Autoriser »
   → pose acceptée). Fusionner dans `main` si validé.
2. **Remplacer le mot de passe de test** de `.141` par celui du
   propriétaire (oublier sur l'écran, autoriser, poser depuis le bandeau) ;
   mettre à jour `.env`.
3. Phase 6 : SD retirée (`/login`, `/ota` intégrées), portail captif
   (`/setup` libre, reste sous session), synchro cloud inchangée.
4. Recopier `scripts-schema.html` et `session.js` dans `/editeur/`
   (AlwaysData, par le propriétaire) ; paquet Web `5.10.4` à préparer.
5. Version : proposer `5.12.0` après B1/B5.
6. Lot G (effacement du PIN depuis l'espace en ligne).
7. Reprises du checkpoint `19810d9` §8 : 2 (invitation et changement de mot
   de passe en production), 3 (accusé de commande), 5 (geste PIN au
   démarrage), 6 (ménage legacy : alias `_legacy`/`_v4`/`_s3_v4` dans
   `platformio.ini`, `06_ANTI_REGRESSION` encore sur `ProgrammeArrosage`,
   `10_TASK_HANDOFF` encore sur la branche OTA de juillet, en-tête de
   `REPRISE_INGENIEUR` du 5 oct.), 7 à 10.

## 9. Procédure exacte de reprise

```powershell
git fetch origin
git checkout main
git pull --ff-only
git log --oneline -3
git status --short
git checkout feature/lot-f-b1-b5
git -c core.whitespace=cr-at-eol diff --check
python tools/soak/announce_reboot.py
pio run -e ProgrammeArrosage_s3 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200 --dtr 1 --rts 0
```

Port COM à redemander. Flash de `.141` et dépôt SD autorisés d'office.
Écrire sur le module exige désormais une session :
`python tools/module_session.py login|state|post <route> '<json>'|deploy <fichiers>`
(mot de passe lu dans `.env`, `AQUALOOK_WEB_PASSWORD=`). Contrôles sans
session : `GET /api/diagnostics` (`build`), `/api/logs.txt` (lignes
`[WEB-SESSION]`, `[WEB-GARDE]`), `/api/session/state`.

Pièges : `git diff --check` avec `-c core.whitespace=cr-at-eol` ; heredoc bash
qui mange les `\` (passer par un fichier) ; `data/**` en LF dans Git ;
les écritures sans session répondent 401, pas une erreur de code.

## 10. Commandes Git utiles

```powershell
git log --oneline 37b5921..f0e7a8a      # lot F complet
git show 3e18ef4                        # durcissement firmware
git show 06dde42                        # pages et outils
git diff main..feature/lot-f-b1-b5      # B1/B5 en attente
```
