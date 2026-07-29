# AquaLook Engineering Reference — Consolidation à chaque checkpoint

- Version documentaire : 1.0
- Statut : règle de gouvernance
- Dernière consolidation : 2026-07-27

## Objet

Chaque checkpoint consolide le code, l’état de validation et la documentation d’ingénierie. Le checkpoint sert de source principale pour amender le référentiel sans réinventer l’architecture ni recopier les roadmaps.

## Procédure obligatoire

Avant la clôture d’un checkpoint :

1. identifier les composants, interfaces, invariants et risques modifiés ;
2. relire les tomes d’ingénierie concernés ;
3. corriger l’état actuel sur la base du code et des validations du checkpoint ;
4. ajouter les URL, méthodes, topics, fichiers, ports ou interfaces matérielles réellement introduits ;
5. ne pas intégrer comme existantes les évolutions seulement présentes dans une roadmap ;
6. mettre à jour les références vers le checkpoint, les ADR et les risques ;
7. réévaluer le niveau de maturité documentaire D0 à D5 ;
8. vérifier les liens, termes et doublons ;
9. inclure la documentation consolidée dans le commit et dans l’archive de reprise.

## Rapport de consolidation

Le document de checkpoint indique :

- tomes consultés ;
- tomes modifiés ;
- interfaces ajoutées ou retirées ;
- invariants ajoutés, modifiés ou confirmés ;
- maturité documentaire avant/après ;
- lacunes restant ouvertes ;
- dernier numéro de réponse conversationnelle pertinent, lorsqu’il est connu.

## Règles de contenu

- le référentiel décrit l’état réel ;
- les roadmaps décrivent les évolutions ;
- les ADR justifient les décisions ;
- les checkpoints décrivent l’état validé à une date et un commit ;
- aucune connaissance validée ne doit rester uniquement dans une conversation.

## Critère de clôture

Un checkpoint n’est documentairement complet que lorsque les tomes affectés correspondent au commit de reprise ou que les écarts non consolidés sont explicitement recensés dans le checkpoint.

## Intégration dans `AGENTS.md`

Cette procédure doit être référencée depuis la section « Déclencheur permanent : checkpoint » de `AGENTS.md`. La mise à jour est traitée comme une règle d’agent et doit rester compatible avec les étapes existantes de compilation, buildfs, validation matérielle, commit et création du ZIP.

## Historique

### 1.0

Création de la procédure de consolidation documentaire obligatoire.
