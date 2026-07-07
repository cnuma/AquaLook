# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Run d’origine :** Phase 0 — Run 0  
**Dernière mise à jour :** Phase 1 — Run 1.6, 7 juillet 2026

## 1. Rôle

Ce document recense les décisions encore ouvertes. Une entrée est close lorsqu’elle est décidée par une ADR, abandonnée ou remplacée.

## 2. Priorités

- **P0 :** décision nécessaire avant le prochain code ;
- **P1 :** décision nécessaire dans la phase en cours ;
- **P2 :** décision nécessaire avant une phase future ;
- **P3 :** orientation différée.

## 3. Backlog principal

| ID | Sujet | Statut |
|---|---|---|
| ARCH-001 | Identifiants stables | **Décidé** |
| ARCH-002 | Capacité dynamique bornée | **Principe décidé, budgets à mesurer** |
| ARCH-003 | Paramètres spécifiques | **Décidé** |
| ARCH-004 | Types et capacités | **Décidé** |
| ARCH-005 | États runtime | **Décidé et prototypé** |
| ARCH-006 | Intentions et priorités | **Décidé et prototypé** |
| ARCH-007 | Exécutions | **Décidé et prototypé** |
| ARCH-008 | Dépendances | **Décidé et prototypé** |
| ARCH-009 | Cycles interdits | **Décidé et prototypé** |
| ARCH-010 | Inventaire générique des bus | Orientation décidée |
| ARCH-011 | Cartes et ports génériques | Orientation décidée |
| ARCH-012 | Adresses de bus dupliquées | En attente |
| ARCH-013 | Actionneur binaire | En attente |
| ARCH-014 | État sûr par port/actionneur | En attente |
| ARCH-015 | Compatibilité `RelayAssignment` | En attente |
| ARCH-016 | Commandes idempotentes | En attente |
| ARCH-017 | Reprise après reboot | En attente |
| ARCH-018 | Durées maximales | En attente |
| ARCH-019 | Compensation d’échec | Primitive décidée, politique ouverte |
| ARCH-020 | Pompe partagée | En attente |
| ARCH-021 | Précharge et post-fonctionnement | En attente |
| ARCH-022 | Simultanéité | En attente |
| ARCH-023 | Qualité des observations | En attente |
| ARCH-024 | Comptage débitmètres | En attente |
| ARCH-025 | Défauts débit et fuite | En attente |
| ARCH-026 | Format persistant indépendant | En attente |
| ARCH-027 | Candidate et activation atomique | Architecture définie |
| ARCH-028 | Migration V3 vers V4 | En attente |
| ARCH-029 | Import, export et modèles | En attente |
| ARCH-030 | Révision et concurrence | En attente |
| ARCH-031 | Erreurs API V4 | En attente |
| ARCH-032 | Authentification locale | En attente |
| ARCH-038 | CI et tests hôte | Ouvert |
| ARCH-039 | API minimale de l’arène | Prototype isolé réalisé |
| ARCH-040 | Versionnement des modèles de cartes | Ouvert |
| ARCH-041 | Suppression d’une carte liée | Ouvert |
| ARCH-042 | Activation pendant une exécution | Ouvert |
| ARCH-043 | Catalogue de types | Ouvert |
| ARCH-044 | Validateurs spécifiques | Ouvert |
| ARCH-045 | Registre borné des états | Ouvert |
| ARCH-046 | Registre et politique des défauts | Ouvert |
| ARCH-047 | Tolérances de convergence | Ouvert |
| ARCH-048 | Calcul de santé | Ouvert |
| ARCH-049 | File bornée d’intentions | Ouvert |
| ARCH-050 | Arbitre complet | Ouvert |
| ARCH-051 | Groupes de corrélation | Ouvert |
| ARCH-052 | Registre borné des exécutions | Ouvert |
| ARCH-053 | Retry et backoff | Ouvert |
| ARCH-054 | Clôture après compensation | Ouvert |
| ARCH-055 | Résolution runtime des dépendances | Ouvert |
| ARCH-056 | Groupes d’exclusivité | Ouvert |
| ARCH-057 | Optimisation de l’index du graphe | Différé après mesures |

## 4. Décisions acquises

- `Equipment`, état runtime, intention, exécution et dépendance sont séparés ;
- une dépendance relie uniquement deux `EquipmentId` ;
- `sourceId` désigne l’équipement dépendant et `targetId` sa contrainte ;
- les relations d’ordre sont validées comme graphe acyclique ;
- les relations symétriques restent hors du tri topologique ;
- les références orphelines, auto-références et doublons sont refusés ;
- le workspace de validation est fourni par l’appelant ;
- `EquipmentDependency` occupe 16 octets ;
- aucune allocation dynamique ni liaison matérielle ;
- runtime historique inchangé.

## 5. Backlog immédiat de la Phase 1

1. consolidation du catalogue de types et des validateurs spécifiques ;
2. registres bornés des états, intentions, exécutions et défauts ;
3. budget mémoire global de Phase 1 ;
4. arbitre complet et résolution runtime des dépendances ;
5. décision de clôture de Phase 1 avant Phase 2.
