# AquaLook Engineering Reference — Index des checkpoints

- Version documentaire : 1.0
- Statut : index initial
- Dernière consolidation : 2026-07-27
- Source : `docs/checkpoints/` et historique Git
- Maturité : D3

## Objet

Cet index permet d’identifier rapidement les états de reprise qui ont servi à consolider le référentiel. Le contenu exact du dossier `docs/checkpoints/` reste la source de vérité.

## Checkpoints de référence

| Date | Référence | Portée principale | État |
|---|---|---|---|
| 2026-06-27 | checkpoint complet `main` associé au socle Codex | base firmware, règles de reprise et compilation | historique |
| 2026-07-13 | `CHECKPOINT_2026-07-13_MAIN_STEP6_CLOSED.md` | clôture étape 6, matériel, Web, SD, NTP, EventLog et Runtime | référence majeure |
| 2026-07-25/26 | checkpoints et commits documentaires architecture/cybersécurité | gouvernance, sécurité et manuel d’ingénierie | intégrés |
| 2026-07-27 | lots 1 à 4 Engineering Reference v1.0 | consolidation documentaire | courant |

## Informations obligatoires d’un checkpoint

- dépôt, branche et commit ;
- source de vérité ;
- fonctionnalités validées ;
- fichiers et fonctions modifiés ;
- invariants préservés ;
- compilation, buildfs et tests matériels ;
- risques, limites et points ouverts ;
- procédure exacte de reprise ;
- documents du référentiel impactés.

## Règle de consolidation

À chaque nouveau checkpoint :

1. comparer le code avec le dernier état documenté ;
2. identifier les tomes impactés ;
3. mettre à jour les interfaces, invariants, tests et maturités ;
4. ajouter le checkpoint à cet index ;
5. vérifier les liens du référentiel global.

## Règles de confiance

- le code du commit ciblé prime en cas de divergence ;
- un checkpoint décrit un état validé, pas une intention ;
- une roadmap ne remplace pas une preuve de test ;
- les conversations ne constituent pas une source technique suffisante.

## Références

- `11_CHECKPOINT_CONSOLIDATION.md` ;
- `03_DOCUMENTATION_GOVERNANCE.md` ;
- `AGENTS.md` ;
- `docs/checkpoints/`.
