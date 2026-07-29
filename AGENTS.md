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

### Checklist automatique au début de chaque chat AquaLook

Cette checklist est une obligation de l’agent. L’utilisateur ne doit pas avoir à demander sa relecture ni à rappeler les commandes, profils ou conventions déjà consignés dans le dépôt.

Avant toute analyse technique, proposition de modification, commande Git, compilation, téléversement ou test matériel dans un nouveau chat AquaLook, l’agent doit automatiquement :

1. commencer la numérotation conversationnelle à `AQL-R001` ;
2. identifier le dépôt, la branche ou le checkpoint de reprise réellement demandé ;
3. lire la version courante de `AGENTS.md` depuis Git, sans utiliser la mémoire de conversation comme substitut ;
4. exécuter l’ordre de lecture obligatoire défini ci-dessus ;
5. vérifier le dernier checkpoint applicable et ne pas repartir d’un ancien extrait ou d’une branche supposée ;
6. relire `platformio.ini` avant de proposer des commandes de build, d’upload, de buildfs ou de monitoring ;
7. confirmer les noms exacts des environnements PlatformIO et les commandes imposées par le profil actif ;
8. demander le port COM courant avant toute commande qui en dépend, sans reprendre automatiquement celui d’une ancienne session ou de `platformio.ini` ;
9. regrouper toutes les commandes Git, compilation, téléversement et monitoring dans un seul bloc continu, dans l’ordre exact d’exécution ;
10. signaler immédiatement toute divergence entre la branche de travail, `main`, le checkpoint et les règles documentaires applicables ;
11. ne commencer aucune modification tant que cette passe de démarrage n’est pas terminée.

Une reprise fournie par l’utilisateur peut accélérer l’identification du checkpoint, mais ne dispense jamais de relire les fichiers de gouvernance et de configuration dans leur version Git courante.

## Règles impératives

### Numérotation des réponses de l’agent

Afin de rendre visible la profondeur d’un échange, de détecter les conversations devenues trop longues et de faciliter les références croisées :

- toute réponse principale de l’agent dans un échange relatif à AquaLook commence par un identifiant au format `AQL-RNNN`, par exemple `AQL-R001` ;
- la numérotation commence à `AQL-R001` au début de chaque nouveau chat AquaLook et progresse de une unité à chaque nouvelle réponse principale ;
- les messages intermédiaires appartenant à la même réponse utilisent le même numéro suivi d’un suffixe, par exemple `AQL-R004.1`, `AQL-R004.2` ;
- le numéro est affiché au tout début du message, avant le titre ou le contenu ;
- les réponses très courtes, les confirmations et les bilans sont également numérotés ;
- un checkpoint ou un document de reprise peut mentionner le dernier numéro atteint afin d’indiquer la profondeur de la conversation source ;
- lorsque la conversation dépasse environ `AQL-R030`, l’agent doit signaler que le chat devient profond et proposer ou préparer un checkpoint autonome avant que la longueur du contexte ne dégrade la qualité ou les performances ;
- cette numérotation sert uniquement au suivi conversationnel et ne remplace ni les numéros de runs, ni les versions, ni les commits Git, ni les identifiants d’issues ou de pull requests.

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
2. Vérifier que le code validé est compilé, que buildfs est validé si `littlefs/` a changé, et que le dépôt ne contient aucune modification non validée.
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
8. Inclure dans le ZIP les sources, `data/`, `littlefs/`, documentation, AGENTS.md, platformio.ini et le document de reprise.
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

### Qualification Legacy / V4

Pendant la migration du backend d’exécution :

- `ProgrammeArrosage_legacy` est la référence historique et la solution de repli ;
- `ProgrammeArrosage_v4` est le profil à qualifier en priorité pour les nouveaux développements ;
- une compilation V4 réussie ne signifie jamais que la fonction est validée en V4 ;
- une fonction ne peut être déclarée « migrée V4 » ou « validée V4 » que si son chemin V4 est réellement instancié, appelé et testé sur la carte avec un effet observable ;
- tant que `V4RelayPhysicalBackend` n’est pas câblé dans `main.cpp` pour la fonction ou la zone concernée, indiquer explicitement que le test V4 n’est pas représentatif, même si la compilation réussit ;
- les essais matériels courants doivent être réalisés avec `ProgrammeArrosage_v4` dès que le chemin concerné est actif ;
- le firmware Legacy ne doit être chargé que pour comparaison, diagnostic de régression, campagne de non-régression ou retour temporaire à la référence stable ;
- toute différence entre Legacy et V4 doit être qualifiée comme régression, correction volontaire ou évolution documentée ;
- ne pas accumuler de nouvelles évolutions importantes tant que les fonctions V4 déjà intégrées n’ont pas été testées.

Avant la première compilation, le premier téléversement ou l’ouverture du moniteur série d’une nouvelle session, demander explicitement à l’utilisateur sur quel port COM la carte est connectée. Ne jamais supposer que le port de `platformio.ini`, d’une autre machine ou d’une ancienne session est encore valable. Tant que le port n’est pas confirmé, utiliser le marqueur `<PORT_COM>` dans les commandes et proposer `pio device list` si nécessaire.

Pour tout nouveau code embarqué destiné à être testé sur matériel, la chaîne minimale est :

```powershell
git diff --check
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port <PORT_COM>
pio device monitor -p <PORT_COM> -b 115200
```

La cible `upload` compile automatiquement V4 avant le téléversement. Ne pas lancer une compilation V4 séparée juste avant cette commande, sauf besoin explicite de diagnostic. Lorsqu’aucun téléversement n’est prévu, utiliser `pio run -e ProgrammeArrosage_v4` pour la validation de compilation.

Consigner le profil réellement flashé, le port série utilisé, le point d’entrée exécuté, le matériel ou la zone testée, le résultat attendu, le résultat observé et les tests non effectués.

### LittleFS

- `data/` contient les ressources complètes destinées notamment à la carte SD.
- `littlefs/` contient uniquement les secours techniques embarqués réellement nécessaires et constitue le `data_dir` PlatformIO.
- Ne jamais déposer dans `littlefs/` : sauvegarde, patch, script, fichier `.bak`, copie de travail ou documentation.
- Après toute modification de `littlefs/`, exécuter obligatoirement `pio run -e ProgrammeArrosage_v4 -t buildfs`.
- Avant checkpoint ou livraison nécessitant un repli complet, valider aussi `pio run -e ProgrammeArrosage_legacy -t buildfs`.
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
6. Avant toute commande dépendant du port série, demander et confirmer le port COM de la machine courante.
7. Exécuter `git diff --check`, puis `pio run -e ProgrammeArrosage_legacy`.
8. Si un essai matériel V4 est prévu, exécuter directement `pio run -e ProgrammeArrosage_v4 -t upload --upload-port <PORT_COM>` ; cette commande assure compilation et téléversement. Sinon, exécuter `pio run -e ProgrammeArrosage_v4`.
9. Si `littlefs/` est modifié, exécuter `pio run -e ProgrammeArrosage_v4 -t buildfs` ; avant checkpoint ou livraison de repli, valider aussi le buildfs Legacy.
10. Pour une modification matérielle ciblée, utiliser `pio run -e calibration`, `pio run -e test_relais` ou `pio run -e test_execution_engine` selon le périmètre.
11. Pour tout nouveau chemin V4 actif, ouvrir le moniteur série sur `<PORT_COM>` et effectuer le test matériel correspondant.
12. Examiner le diff final et rechercher duplication HTML/CSS/JS, IDs dupliqués, blocs ajoutés plusieurs fois, changement hors périmètre et hausse anormale de taille.
13. Valider avec l’outil cible tout patch, script, archive ou fichier de transformation destiné à l’utilisateur.
14. Documenter les fichiers modifiés, fichiers volontairement non modifiés, statut des compilations Legacy et V4, port et profil flashé, statut LittleFS, tests matériels réalisés et restant à faire, risques et incertitudes.

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
