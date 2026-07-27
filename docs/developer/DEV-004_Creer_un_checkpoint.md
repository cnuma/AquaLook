# AquaLook Developer Guide — Créer un checkpoint

- Référence : DEV-004
- Statut : actif
- Maturité : D4

## Objet

Un checkpoint est un état de reprise vérifiable. Il décrit ce qui fonctionne réellement au commit indiqué, les tests exécutés et les travaux encore ouverts.

## Contenu obligatoire

- date et objectif ;
- branche et commit exacts ;
- matériel et environnement PlatformIO ;
- fichiers ou composants modifiés ;
- décisions et invariants confirmés ;
- commandes de compilation ;
- uploads et tests matériels réellement réalisés ;
- résultats, mesures et logs utiles ;
- écarts, risques et limitations ;
- procédure de reprise ;
- documents Engineering, Firmware et Developer mis à jour.

## Procédure

1. synchroniser la branche et vérifier `git status` ;
2. relire le diff final ;
3. exécuter les builds requis ;
4. exécuter `buildfs` si `littlefs/` change ;
5. exécuter les contrats et bancs concernés ;
6. réaliser les tests matériels possibles ;
7. noter explicitement tout test non réalisé ;
8. mettre à jour la documentation avant clôture ;
9. créer le checkpoint dans `docs/checkpoints/` ;
10. référencer le commit final, sans SHA anticipé faux.

## Interdictions

- déclarer validé un comportement seulement compilé ;
- recopier une roadmap comme état présent ;
- cacher une régression ou une dette ;
- inclure un secret ou une configuration privée ;
- mélanger plusieurs bases firmware sans préciser leur origine.

## Critère de clôture

Le checkpoint est exploitable lorsqu’un ingénieur peut retrouver le commit, reconstruire le firmware, comprendre les validations et reprendre le travail sans dépendre du chat.

## Références

- `docs/engineering/11_CHECKPOINT_CONSOLIDATION.md`
- `docs/engineering/17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md`
- `docs/engineering/31_CHECKPOINT_INDEX.md`
