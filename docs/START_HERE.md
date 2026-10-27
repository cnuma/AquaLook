# AquaLook — START HERE

> **PROJET EN PAUSE depuis le 27 octobre 2026.** Point d'entrée : `docs/checkpoints/CHECKPOINT_2026-10-27_ba86d50_PAUSE.md`.

Ce fichier est le point d'entrée officiel de la documentation AquaLook.

## Ordre de lecture recommandé

Mis à jour le 14 octobre 2026.

1. `AGENTS.md` — règles impératives, chaîne de compilation et de test ;
2. `docs/REPRISE_INGENIEUR.md` — point d'entrée de reprise et fraîcheur de chaque famille de documents ;
3. le checkpoint le plus récent de `docs/checkpoints/` — état du jour ;
4. `docs/TODO.md` — reste à faire, liste unique par priorité ;
5. `docs/codex/` (00 à 05, puis 02_DECISIONS) — contexte, architecture, invariants, règles, compilation ;
6. `docs/engineering/01_PROJECT_STATUS.md` — situation consolidée ;
7. les documents spécialisés (`docs/engineering/00_MANUAL_INDEX.md` et suivants). Plusieurs chapitres du manuel sont antérieurs au portage ESP32-S3 : en cas d'écart, les sources 1 à 4 priment.

## Guides pratiques

- `docs/guides/manuel/` — **manuel utilisateur** (sommaire dans `README.md`) : référence de ce que voit et fait l'utilisateur (écran, pages Web, planning, pluie, arrosage manuel…). Tenu à jour avec chaque évolution visible (`AGENTS.md`, « Manuel utilisateur »).
- `docs/guides/MONTER_UN_MODULE_NEUF.md` — monter un module neuf, de la carte nue au rattachement (liste de contrôle comprise).

## Source de vérité

Le dépôt Git `cnuma/AquaLook` constitue la mémoire officielle du projet. Une décision importante n'est considérée comme acquise que lorsqu'elle est intégrée dans le dépôt sous forme de documentation, d'ADR, d'invariant, de test, de checkpoint ou de code versionné.

Les conversations, notes locales et souvenirs ne sont jamais une source de vérité suffisante lorsqu'une version plus récente existe dans Git.

## Principes

- ne pas dupliquer inutilement les mêmes informations ;
- faire évoluer les documents existants plutôt que les réinventer ;
- conserver l'historique et la justification des décisions ;
- relier toute évolution aux piliers architecture, matériel, logiciel, connectivité, cloud, cybersécurité, observabilité, qualité, documentation et évolution ;
- mettre à jour la documentation en même temps que le code.

## Références existantes

Le manuel d'ingénierie fédère sans remplacer :

- `docs/codex/` ;
- `docs/architecture/` ;
- `docs/security/` ;
- `docs/roadmap/` ;
- `docs/checkpoints/`.
