# Documentation Developer AquaLook

Ce dossier explique **comment modifier et étendre le projet** sans casser ses invariants.

## Lot D1

| Référence | Guide |
|---|---|
| `DEV-001_Architecture_du_projet.md` | lire le dépôt et comprendre les couches |
| `DEV-002_Creer_un_Manager.md` | ajouter un composant applicatif |
| `DEV-003_Ajouter_une_route_REST.md` | créer et tester un contrat HTTP |
| `DEV-004_Creer_un_checkpoint.md` | produire un état de reprise vérifiable |
| `DEV-005_Bonnes_pratiques_de_developpement.md` | règles de code, test, Git et documentation |

## Lot D2

| Référence | Guide |
|---|---|
| `DEV-006_Ajouter_un_equipement.md` | étendre le modèle et les plans V4 |
| `DEV-007_Ajouter_une_vue_LCD.md` | créer une vue et gérer les redraws |
| `DEV-008_Ajouter_une_ressource_Web.md` | choisir SD, LittleFS ou fallback firmware |
| `DEV-009_Ajouter_un_service_reseau.md` | intégrer un service non bloquant et sécurisé |
| `DEV-010_Ajouter_un_backend_materiel.md` | traduire une action logique en sortie physique |

## Lot D3

| Référence | Guide |
|---|---|
| `DEV-011_Ajouter_un_test_ou_un_banc_materiel.md` | choisir et documenter une preuve logicielle ou matérielle |
| `DEV-012_Ajouter_une_donnee_persistante_et_sa_migration.md` | faire évoluer NVS sans casser les configurations existantes |
| `DEV-013_Ajouter_un_diagnostic_JSON.md` | exposer un état observable, borné et sans secret |
| `DEV-014_Ajouter_un_defaut_FaultManager.md` | ajouter un défaut avec activation et résolution explicites |
| `DEV-015_Modifier_la_boucle_Runtime_sans_la_bloquer.md` | intégrer un traitement court, borné et observable |
| `DEV-016_Ajouter_un_contrat_de_cybersecurite_CI.md` | protéger une propriété de sécurité dans la CI |

## Règle de maintenance

Un guide est mis à jour lorsque la méthode de développement change. Les détails d’implémentation restent dans `docs/firmware/`, et l’architecture de référence dans `docs/engineering/`.
