# AGENTS.md — AquaLook

## Mission

Ce dépôt contient le firmware et l’interface utilisateur du contrôleur d’arrosage AquaLook sur ESP32.

Toute intervention automatisée ou assistée par Codex doit préserver en priorité :

1. la sécurité d’activation des relais ;
2. la persistance de la configuration ;
3. la compatibilité LittleFS ;
4. la stabilité de l’interface Web et LCD ;
5. les invariants documentés dans `docs/codex/03_INVARIANTS.md`.

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche stable : `main`
- Base inspectée pour ce socle : commit `a2cf490aa446c7006557f8df62e1f995f6767359`
- Checkpoint associé : `AquaLook_2026-06-27_main_checkpoint_complet_a2cf490.zip`
- Date du socle : 28 juin 2026

Ne jamais reconstruire un fichier depuis un ancien extrait, un souvenir ou une version locale non vérifiée si le dépôt contient une version plus récente.

## Lecture obligatoire avant modification

Lire dans cet ordre :

1. `AGENTS.md`
2. `docs/codex/00_CONTEXT.md`
3. `docs/codex/01_ARCHITECTURE.md`
4. `docs/codex/03_INVARIANTS.md`
5. `docs/codex/04_DEVELOPMENT_RULES.md`
6. `docs/codex/05_BUILD_AND_TEST.md`
7. le ou les fichiers réellement concernés

Pour une évolution significative, lire aussi :

- `docs/codex/02_DECISIONS.md`
- `docs/codex/06_ANTI_REGRESSION.md`
- `docs/codex/08_RISKS_AND_DEBT.md`
- `docs/codex/10_TASK_HANDOFF.md`

## Règles impératives


## Déclencheur permanent : checkpoint

Lorsque l’utilisateur écrit exactement « checkpoint », exécuter automatiquement la procédure suivante sans attendre de demande complémentaire :

1. Identifier la branche active, le commit courant et l’état Git.
2. Vérifier que le code validé est compilé, que buildfs est validé si data/ a changé, et que le dépôt ne contient aucune modification non validée.
3. Créer le document :
   docs/checkpoints/CHECKPOINT_YYYY-MM-DD_<sha-court>.md
4. Le document doit être autonome et inclure :
   - dépôt, branche et commit fonctionnel ;
   - source de vérité ;
   - résumé des fonctionnalités validées ;
   - fichiers et fonctions modifiés ;
   - invariants préservés ;
   - état compilation, buildfs, Web, LCD et matériel ;
   - risques et limites ;
   - procédure exacte de reprise ;
   - commandes Git utiles.
5. Ajouter, committer et pousser ce document.
6. Utiliser le nouveau commit documentaire comme commit officiel de reprise.
7. Générer ensuite un checkpoint complet nommé :
   AquaLook_YYYY-MM-DD_<branche>_checkpoint_complet_<sha-court>.zip
8. Inclure dans le ZIP les sources, data, documentation, AGENTS.md, platformio.ini et le document de reprise.
9. Exclure .git, .pio, sauvegardes, logs, secrets et fichiers temporaires.
10. Calculer et fournir le SHA-256.
11. Fournir enfin un bloc minimal de reprise prêt à copier dans un nouveau chat.

Ne jamais annoncer qu’un document de reprise est dans le dépôt avant d’avoir vérifié qu’il a réellement été committé et poussé.

### Périmètre minimal

- Modifier le minimum nécessaire.
- Ne pas réécrire un fichier complet pour une modification locale.
- Ne pas reformater globalement hors besoin explicite.
- Ne pas modifier les noms d’API, IDs HTML, routes ou structures persistées sans décision documentée.
- Ne pas supprimer un comportement existant sous prétexte de simplification.

### Vérification fonctionnelle obligatoire pour tous les développements ESP

Cette règle s’applique à tout développement ESP32, ESP8266 et Arduino.

- Une modification n’est pas validée par sa seule présence dans le dépôt ni par la réussite de la compilation.
- Vérifier que le code agit réellement sur l’élément demandé par l’utilisateur.
- Vérifier toute la chaîne d’exécution : inclusion, instanciation, appel, déclencheur, conditions d’entrée, rafraîchissement et effet observable.
- Toute nouvelle fonction, classe, tâche, route, callback ou gestionnaire doit être relié à un point d’entrée réellement exécuté (`setup()`, `loop()`, tâche FreeRTOS, événement, route HTTP ou callback matériel).
- Vérifier qu’aucun autre rendu, cache, sprite, rafraîchissement ou rechargement de configuration ne masque ou n’annule le résultat.
- Vérifier tous les modes concernés par la demande, et pas seulement un cas particulier.
- Avant livraison, identifier pour chaque demande le fichier, la fonction, le point d’appel et le résultat attendu.
- Si la vérification matérielle n’a pas été effectuée, le signaler explicitement.

### LittleFS

- `data/` contient uniquement les ressources embarquées réellement nécessaires.
- Ne jamais déposer dans `data/` : sauvegarde, patch, script, fichier `.bak`, copie de travail ou documentation.
- Après toute modification de `data/`, exécuter obligatoirement `pio run -e ProgrammeArrosage -t buildfs`.
- Un changement Web doit avoir un bilan de taille maîtrisé. La partition est proche de sa limite.

### Persistance

- `ConfigManager` est l’unique propriétaire du montage LittleFS.
- La configuration persistante active est stockée en NVS.
- LittleFS est en lecture seule pour les ressources Web et le splash, hors migration historique contrôlée.
- Toute évolution du format NVS doit être versionnée, validée et accompagnée d’une stratégie de migration ou de repli.

### Relais et sécurité

- Le planificateur ne pilote jamais directement le matériel.
- `ScheduleManager` demande l’état via callback.
- Le callback matériel est câblé dans `main.cpp`.
- Toute modification de logique directe/inverse ou de contrôleur doit être traitée comme critique.
- Ne jamais supprimer la sécurité de durée maximale.
- Les tests matériels commencent par une seule zone, durée courte, sous surveillance.

### Web

- Conserver les routes existantes sauf décision explicite.
- Conserver les IDs HTML utilisés par `app.js`.
- Après un POST qui modifie l’affichage, positionner le mécanisme de rafraîchissement prévu (`EventBus::displayDirty`).
- Pour les opérations suivies d’un redémarrage, répondre au client avant le reboot.
- Le verrouillage administrateur actuel est un verrouillage visuel côté navigateur, pas une authentification forte.

### Affichage

- Ne pas appeler `fillScreen()` en dehors du chemin de redraw complet prévu.
- Respecter le bus touch séparé.
- Préserver le fonctionnement hot-reload des paramètres d’affichage.
- Ne pas ajouter de polices GFXFF par includes séparés si elles sont déjà fournies par TFT_eSPI.

## Procédure obligatoire avant livraison

1. Identifier la branche et le commit de base.
2. Lister les fichiers et fonctions concernés.
3. Énoncer les invariants à préserver.
4. Faire la modification minimale.
5. Vérifier que les changements sont réellement appelés et qu’ils agissent sur les éléments demandés.
6. Exécuter `git diff --check` puis `pio run -e ProgrammeArrosage`.
7. Si `data/` est modifié, exécuter `pio run -e ProgrammeArrosage -t buildfs`.
8. Pour une modification matérielle ciblée, utiliser `pio run -e calibration` ou `pio run -e test_relais`.
9. Examiner le diff final et rechercher duplication HTML/CSS/JS, IDs dupliqués, blocs ajoutés plusieurs fois, changement hors périmètre et hausse anormale de taille.
10. Documenter les fichiers modifiés, fichiers volontairement non modifiés, statut de compilation, statut LittleFS, tests matériels restant à faire, risques et incertitudes.

## Livrables

Quand plus de deux fichiers sont modifiés :

- fournir directement tous les fichiers complets concernés ou un package complet ;
- ne pas attendre que l’utilisateur le redemande ;
- indiquer l’emplacement exact de chaque fichier dans le dépôt.

## Git

- `main` est stable.
- Une évolution est développée sur une branche dédiée `feature/...`, `fix/...` ou `docs/...`.
- Les commits doivent être petits, explicites et testables.
- Ne pas pousser un code non compilé.
- Ne pas fusionner vers `main` avant validation firmware et LittleFS.
- Les checkpoints doivent inclure l’origine dans leur nom.

## Style de travail Codex

- Expliquer les hypothèses.
- Signaler les incertitudes.
- Ne pas masquer un échec de compilation ou de test.
- Ne pas inventer un résultat matériel.
- Pour une tâche, produire un plan court, appliquer, tester, puis faire un bilan factuel.
