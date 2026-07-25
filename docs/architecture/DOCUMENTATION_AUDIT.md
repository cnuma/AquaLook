# AquaLook — Audit et gouvernance documentaire

## Statut

Audit documentaire de référence clôturant le chantier de structuration des piliers.

## 1. Constat

La documentation AquaLook était déjà riche et structurée avant ce chantier, mais répartie entre plusieurs familles :

- `AGENTS.md` pour les règles impératives de travail ;
- `docs/codex/` pour le socle historique, les invariants, décisions, tests, risques et reprises ;
- `docs/checkpoints/` pour les états de reprise exacts ;
- `docs/architecture/` pour les architectures spécialisées ;
- les fichiers de roadmap pour les évolutions futures.

Le problème identifié n'était pas l'absence de documentation, mais l'absence d'une porte d'entrée transverse commune et d'un traitement explicite de la cybersécurité, de l'observabilité et de la qualité comme piliers permanents.

## 2. Décision de classement

Aucun document existant n'est déplacé ou renommé dans ce chantier afin d'éviter les liens cassés et les régressions documentaires.

La hiérarchie de référence devient :

1. `AGENTS.md` : règles impératives applicables à toute intervention ;
2. `docs/architecture/ARCHITECTURE_OVERVIEW.md` : vue directrice et grille des piliers ;
3. `docs/codex/` : socle détaillé historique toujours applicable ;
4. documents spécialisés dans `docs/architecture/`, `docs/security/` et `docs/roadmap/` ;
5. `docs/checkpoints/` : sources de vérité ponctuelles par branche et commit.

## 3. Documents fédérateurs créés

- `docs/architecture/ARCHITECTURE_OVERVIEW.md` ;
- `docs/architecture/OBSERVABILITY.md` ;
- `docs/architecture/QUALITY.md` ;
- `docs/architecture/DOCUMENTATION_AUDIT.md` ;
- `docs/security/CYBERSECURITY_ARCHITECTURE.md` ;
- `docs/security/SECURITY_RISK_REGISTER.md` ;
- `docs/roadmap/CYBERSECURITY_LIFECYCLE.md` ;
- `docs/roadmap/ARCHITECTURE_GOVERNANCE.md`.

## 4. Doublons et chevauchements

Les chevauchements suivants sont intentionnels et non considérés comme des doublons à supprimer :

- `docs/codex/01_ARCHITECTURE.md` décrit l'architecture embarquée observée, tandis que `ARCHITECTURE_OVERVIEW.md` décrit l'écosystème et ses piliers ;
- `docs/codex/08_RISKS_AND_DEBT.md` couvre les risques techniques généraux, tandis que `SECURITY_RISK_REGISTER.md` suit les risques cybersécurité ;
- les roadmaps fonctionnelles décrivent les évolutions produit, tandis que `CYBERSECURITY_LIFECYCLE.md` et `ARCHITECTURE_GOVERNANCE.md` définissent des activités transverses permanentes ;
- les checkpoints restent des photographies datées et ne remplacent jamais l'architecture durable.

## 5. Documents obsolètes

Aucun document n'est déclaré obsolète ou supprimé dans ce chantier. Un document ne pourra être archivé qu'après vérification de son contenu, de ses liens entrants et de son remplacement explicite.

## 6. Règle de maintenance

Toute évolution significative doit :

- identifier les piliers concernés ;
- préciser les invariants et risques ;
- définir les tests et le comportement dégradé ;
- analyser l'impact cybersécurité ;
- mettre à jour les documents durables concernés ;
- créer un checkpoint lorsque l'état est matériellement validé.

## 7. État de clôture

Le chantier de structuration documentaire est considéré comme clos lorsque les documents ci-dessus sont présents sur la même branche, que `AGENTS.md` impose leur utilisation et que la pull request décrit explicitement ce périmètre.
