# 07 — Git, branches et checkpoints

## Branches

### main

Branche stable, compilable, avec buildfs valide. Aucun développement exploratoire direct.

### Branches de travail

Conventions :

- feature/sujet
- fix/sujet
- docs/sujet
- chore/sujet

Exemples : feature/refonte-parametrage, feature/debimetres-i2c, fix/refresh-lcd, docs/socle-codex.

## Cycle recommandé

```powershell
git checkout main
git pull origin main
git checkout -b docs/socle-codex
```

Après modification :

```powershell
git diff --check
git status
git add AGENTS.md README.md docs/codex
git commit -m "Ajoute le socle documentaire Codex"
git push -u origin docs/socle-codex
```

## Commits

Un commit correspond à une intention, compile si du code est modifié, ne contient aucun fichier temporaire et utilise un message impératif précis.

## Interdictions

Ne pas versionner .pio, sauvegardes, fichiers bak, scripts jetables, builds, credentials ou archives intermédiaires.

## Checkpoints

Nom obligatoire :

```text
AquaLook_YYYY-MM-DD_origine_checkpoint_sha.zip
```

Checkpoint complet :

```text
AquaLook_YYYY-MM-DD_origine_checkpoint_complet_sha.zip
```

## Contenu checkpoint

Inclure src, include, lib, data, test, platformio.ini, .gitignore, README.md, AGENTS.md et docs.

Exclure .git, .pio, backups, scripts temporaires, logs et secrets.

## Validation checkpoint

Archive lisible, arborescence correcte, SHA-256 calculé, commit dans le nom, source de vérité documentée, compilation et buildfs validés.
