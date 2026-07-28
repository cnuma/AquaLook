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

### Versionnement et espace À propos

- Tout projet exécutable ou publiable doit posséder un numéro de version proposé et maintenu.
- La version fonctionnelle doit être lisible et, de préférence, suivre `MAJEUR.MINEUR.CORRECTIF` avec suffixe éventuel `-dev`, `-beta` ou `-rc`.
- La version fonctionnelle ne doit pas être calculée uniquement depuis le nombre de commits ou une appréciation subjective de l’avancement.
- Lorsque Git est disponible, compléter la version par un numéro de build, un SHA court, la branche ou l’origine et la date de compilation.
- Ces informations doivent provenir d’une source unique du code et ne jamais être dupliquées manuellement dans plusieurs fichiers.
- Chaque projet doit toujours prévoir un espace visible de type `À propos`, `Système`, `Version`, splash, commande CLI ou endpoint de diagnostic.
- Cet espace affiche au minimum le nom du produit, la version fonctionnelle et l’identifiant exact du build.
- Proposer une évolution du numéro de version à chaque palier fonctionnel, release, correction publiée ou évolution significative.
- Réutiliser cette identité dans les checkpoints, archives, releases et procédures de reprise.

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

#### Identité et progression de démarrage obligatoires

Cette règle s’applique à tout développement AquaLook qui crée, modifie ou remplace un chemin de démarrage, un environnement PlatformIO, un profil matériel, un splash, une sortie série de boot ou une initialisation de service.

- La sortie série de démarrage doit identifier explicitement le produit, la version fonctionnelle, la cible ou le profil matériel actif, l’environnement PlatformIO, le numéro de build, le SHA Git court et, lorsque disponible, la branche Git.
- Ces informations doivent provenir de la source unique d’identité de build définie par le projet ; il est interdit de conserver ou d’ajouter un numéro de version, une cible ou un nom d’environnement écrit en dur dans `main.cpp`, un splash, un log ou une page Web.
- Le splash doit afficher au minimum le nom du produit, la version fonctionnelle, la cible active (`LEGACY`, `V4` ou futur profil explicite), l’identifiant de build ou le SHA court, l’étape courante et une progression lisible.
- Chaque étape significative du boot doit produire une information cohérente sur le splash et dans la sortie série, avec un état distinguant au minimum : en cours, réussi, dégradé et échec.
- Un mode dégradé ou un fallback matériel doit être nommé explicitement ; il ne doit jamais être présenté comme un démarrage nominal.
- La fin du boot doit produire un bilan synthétique comprenant au minimum la version, la cible active, la durée de démarrage, l’état réseau ou l’adresse IP lorsqu’elle est disponible, le nombre de zones et la mémoire libre utile.
- Les messages de boot doivent permettre de diagnostiquer rapidement où le démarrage s’est arrêté, sans exiger l’activation de logs de développement supplémentaires.
- Toute nouvelle étape d’initialisation ajoutée au runtime doit être intégrée à cette progression de boot ou être explicitement documentée comme volontairement silencieuse.
- Aucun délai long ou non borné ne doit être ajouté uniquement pour l’esthétique. Une temporisation courte de lisibilité du splash est autorisée lorsqu’elle est explicitement demandée ou configurée, documentée dans le code, bornée par une constante et appliquée uniquement pendant `setup()`, sans retarder une action de sécurité ni le contrôle des relais.
- Avant livraison, vérifier les sorties des environnements `ProgrammeArrosage` et `ProgrammeArrosage_v4` afin de confirmer que l’identité affichée correspond réellement au binaire compilé.

### Validation obligatoire des fichiers livrés

- Tout patch, diff, script, archive, fichier de configuration ou fichier de transformation destiné à être appliqué par l’utilisateur doit être testé avant livraison avec l’outil exact prévu.
- Pour un patch Git, exécuter obligatoirement `git apply --check <fichier.patch>` sur une copie fidèle de la base ciblée avant de le fournir.
- Vérifier ensuite que l’application réelle du patch réussit, que `git diff --check` reste propre et que le résultat ne contient ni duplication ni modification hors périmètre.
- Pour une archive, vérifier son ouverture, son contenu attendu, l’absence de fichiers parasites et, si pertinent, son extraction dans un répertoire temporaire.
- Pour un script, exécuter au minimum un test syntaxique ou un mode non destructif lorsque l’environnement le permet.
- Ne jamais demander à l’utilisateur de valider en premier un fichier que l’agent pouvait vérifier lui-même.
- Si la validation complète est impossible dans l’environnement disponible, l’indiquer avant livraison et fournir une méthode de contrôle locale minimale.

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
10. Valider avec l’outil cible tout patch, script, archive ou fichier de transformation destiné à l’utilisateur.
11. Documenter les fichiers modifiés, fichiers volontairement non modifiés, statut de compilation, statut LittleFS, tests matériels restant à faire, risques et incertitudes.

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

- Expliquer clairement les hypothèses et limites.
- Préserver les décisions déjà validées.
- Ne jamais masquer une régression sous une simplification.
- Utiliser des commits petits, explicites et réversibles.
- Documenter toute dette technique introduite volontairement.
