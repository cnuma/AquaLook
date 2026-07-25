# AquaLook — Gouvernance documentaire

## 1. Règle de source de vérité

Le dépôt Git est la mémoire officielle d'AquaLook. Une information technique importante doit être enregistrée dans un document versionné, un ADR, un invariant, un test, un checkpoint ou le code lui-même.

Une conversation peut initier une décision, mais elle ne la rend pas durable tant que le dépôt n'a pas été mis à jour.

## 2. Hiérarchie documentaire

En cas de contradiction, appliquer l'ordre suivant :

1. code et configuration réellement présents au commit ciblé ;
2. checkpoint exact de la branche et du commit ;
3. invariants et ADR applicables ;
4. manuel d'ingénierie consolidé ;
5. documents d'architecture spécialisés ;
6. roadmap ;
7. notes historiques et conversations.

Une contradiction détectée doit être corrigée dans le dépôt, pas contournée oralement.

## 3. Cycle de vie d'une décision

1. proposition et analyse ;
2. vérification du dépôt réel ;
3. décision explicite ;
4. enregistrement dans le document cible ou un ADR ;
5. mise à jour des risques et invariants ;
6. implémentation ;
7. tests ;
8. mise à jour du manuel et du checkpoint ;
9. revue et fusion.

## 4. Définition de terminé

Un chantier significatif n'est terminé que si :

- le code ou la documentation est intégré sur une branche dédiée ;
- les tests applicables sont exécutés ;
- les impacts sur les dix piliers sont évalués ;
- les risques et modes dégradés sont documentés ;
- les documents concernés sont mis à jour ;
- un checkpoint autonome existe lorsque le chantier modifie l'état de reprise ;
- le résultat est relu dans le diff final.

## 5. Règles d'évolution

- modifier le document de référence existant plutôt que créer un doublon ;
- conserver l'historique des décisions importantes ;
- signaler les sections non vérifiées ou anciennes ;
- citer branche, commit, matériel et validation lorsque le détail dépend d'un état précis ;
- ne jamais présenter une hypothèse comme un comportement validé ;
- archiver uniquement lorsqu'un remplacement explicite existe ;
- maintenir les liens depuis `docs/START_HERE.md` et `00_MANUAL_INDEX.md`.

## 6. ADR

Toute décision difficile à inverser, transverse ou coûteuse doit faire l'objet d'un Architecture Decision Record. Exemples : protocole MQTT, modèle d'identité des appareils, architecture OTA, choix du microcontrôleur d'extension, format de persistance ou autorité de l'orchestrateur.

Format minimal :

- contexte ;
- décision ;
- options étudiées ;
- justification ;
- conséquences positives et négatives ;
- risques ;
- date, branche et commit ;
- statut : proposé, accepté, remplacé ou abandonné.

## 7. Maturité documentaire

Chaque composant peut être classé D0 à D5. Une augmentation de niveau doit correspondre à du contenu vérifiable, pas seulement à la création d'un fichier.

Les composants critiques ne peuvent être déclarés D4 ou D5 sans interfaces, invariants, séquences, erreurs, modes dégradés, tests et procédure de diagnostic.

## 8. Revue périodique

À chaque jalon majeur et au minimum lors des checkpoints structurants :

- vérifier les liens et références ;
- comparer le manuel au code réel ;
- actualiser la situation du projet ;
- revoir le registre des risques cyber ;
- revoir les dépendances, certificats, secrets et procédures de sauvegarde ;
- marquer les documents obsolètes ou les consolider.

## 9. Responsabilité des agents et ingénieurs

Avant toute proposition, lire les documents applicables et le dépôt réel. Après toute évolution, enregistrer les connaissances nouvelles au bon endroit. Il est interdit de reconstruire une architecture depuis la mémoire lorsqu'une source plus récente existe dans Git.
