# Reprise AquaLook depuis le navigateur (session cloud)

Document de reprise pour travailler **sans le PC du propriétaire** :
Claude Code sur le Web (claude.ai/code) ou l'application mobile, à partir du
dépôt GitHub. Usage prévu : **essentiellement la documentation**.

Il complète, sans les remplacer, `AGENTS.md`, `docs/REPRISE_INGENIEUR.md`, le
checkpoint le plus récent de `docs/checkpoints/` et `docs/TODO.md`.

## 1. Point de départ (18 oct. 2026)

| Élément | Valeur |
|---|---|
| Dépôt | `cnuma/AquaLook`, branche `main` |
| Checkpoint officiel | `docs/checkpoints/CHECKPOINT_2026-10-18_24fa2d1.md` (reprise `12369d9`) |
| Commit fonctionnel | `24fa2d1` = **v5.20.0** (tag `v5.20.0`, release signée) |
| Pages Web en ligne | 5.20.0 (30/30) |
| Banc `.141` | 5.20.0 build 1718 `24fa2d1`, pages 5.20.0 |
| Second module `.109` | production 5.19.0, pages 5.17.14, non rattaché |
| Décision la plus récente | D023 : « Cartes interfaces » seule couche d'entrées/sorties |

Ces valeurs vieillissent : si un checkpoint plus récent existe dans
`docs/checkpoints/`, c'est lui qui fait foi.

## 2. Passe de démarrage (obligatoire, sans rappel du propriétaire)

1. Numéroter les réponses à partir de `AQL-R001`.
2. Lire dans Git, version courante : `AGENTS.md`, puis `docs/codex/00_CONTEXT.md`,
   `01_ARCHITECTURE.md`, `03_INVARIANTS.md`, `04_DEVELOPMENT_RULES.md`,
   `05_BUILD_AND_TEST.md`, `docs/REPRISE_INGENIEUR.md`, le checkpoint le plus
   récent et `docs/TODO.md`.
3. Signaler toute divergence entre `main`, le checkpoint et les règles.

## 3. Ce que la session cloud ne peut pas faire

- Aucun accès au réseau local ni à l'USB : pas de flash, pas de moniteur
  série, pas de `.141` ni `.109`, pas de dépôt sur la carte SD.
- Pas de publication AlwaysData (identifiants FTP dans le `.env` local) ni de
  signature de release (clé sur le poste du propriétaire).
- Toute modification de code est livrée « compilée, **non testée sur
  matériel** » et le dit explicitement. Compilation éventuelle :
  `pio run -e ProgrammeArrosage_s3`, si PlatformIO s'installe dans
  l'environnement ; sinon le signaler.
- Les mesures à 390 px (`tools/csp_banc_pages.py`, `tools/apercu_web/`)
  demandent un navigateur headless : les tenter, sinon les laisser pour le
  retour du propriétaire.

## 4. Git dans le cloud

- Une branche `docs/...` (ou `feature/...`, `fix/...`) par sujet ; commits
  petits et explicites, **un commit par modification validée**.
- Pousser la branche ; **ne jamais fusionner dans `main` sans l'accord
  explicite du propriétaire dans le chat**.
- Vérifier `git log base..origin/main` avant une fusion : un seul chat actif
  à la fois, mais `main` peut avoir bougé.

## 5. Règles du propriétaire (consignées ici pour la session cloud)

- Toujours répondre en français, messages d'étape compris.
- V4 seul, S3 seul : ne jamais citer ni réintroduire le moteur historique ;
  pas de build CYD.
- Le dépôt reste open source : aucun secret dans Git, une page, un journal
  ou le chat ; la sécurité ne repose jamais sur le secret du code.
- Mise à jour jamais déclenchée à distance : l'utilisateur déclenche.
- Énoncer les facteurs possibles, ne pas présenter une cause non prouvée ;
  vérifier ce qui est réellement en place avant de l'écrire dans la doc.
- Paramètres modifiables sans recompilation (registre `settings.php` +
  console admin), jamais en dur.
- Pages Web : responsives à 390 px, aucun script inline ni `on*=`, aucune
  ressource externe ; navigation en haut ; une chose à la fois (menu
  d'options, pas d'empilement de cartes) ; même apparence module / site
  (violet `#6633cc` pour les mises à jour).
- Migration NVS : schéma versionné, garde de longueur élargie, sauvegarde
  `tools/nvs_backup.py` avant tout essai.
- Ne pas lancer de recherche récursive dans `.pio/`.

## 6. Travail de documentation possible

- Mettre en cohérence `docs/codex/` avec D023 (retrait d'`IoExpander`,
  « Cartes interfaces ») et avec les décisions récentes de
  `docs/codex/02_DECISIONS.md`.
- Relire `docs/guides/MONTER_UN_MODULE_NEUF.md` (pas encore suivi de bout en
  bout sur un module) et `docs/START_HERE.md`.
- Tenir `docs/TODO.md` à jour, rédiger les spécifications des chantiers
  avant leur code (P2.21 nom des cartes interfaces, P2.22 page `ota` guidée,
  P2.8 notifications par mail, P4.1 CI).
- Réviser le référentiel de sécurité (`docs/security/`), revue trimestrielle
  prévue par D017.

À faire au retour du propriétaire (matériel) : P2.18 lot 4 sur `.109`, mise à
jour de `.109` en 5.20.0, essais de tout code écrit pendant la session cloud.

## 7. Fin de session cloud

Écrire « checkpoint » déclenche la procédure d'`AGENTS.md`. Dans le cloud,
le ZIP et son SHA-256 peuvent être produits mais restent dans
l'environnement temporaire : se limiter au document de checkpoint committé
et poussé, et le signaler.
