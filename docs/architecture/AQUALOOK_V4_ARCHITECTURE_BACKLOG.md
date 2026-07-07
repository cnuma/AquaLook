# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Run d’origine :** Phase 0 — Run 0  
**Dernière mise à jour :** Phase 1 — Run 1.2, 7 juillet 2026

## 1. Rôle

Ce document recense les décisions encore ouvertes. Une entrée est close lorsqu’elle est décidée par une ADR, abandonnée ou remplacée.

## 2. Priorités

- **P0 :** décision nécessaire avant le prochain code ;
- **P1 :** décision nécessaire dans la phase en cours ;
- **P2 :** décision nécessaire avant une phase future ;
- **P3 :** orientation différée.

## 3. Backlog

| ID | Priorité | Sujet | Phase | Livrable | Statut |
|---|---:|---|---:|---|---|
| ARCH-001 | P0 | Identifiants stables | 1 | ADR-0001 | **Décidé** |
| ARCH-002 | P0 | Capacité dynamique bornée | 1/7 | ADR-0002 | **Principe décidé, budgets à mesurer** |
| ARCH-003 | P0 | Paramètres spécifiques des équipements | 1 | ADR-0005 | **Décidé** |
| ARCH-004 | P0 | Type d’équipement et capacités | 1 | ADR-0004 | **Décidé** |
| ARCH-005 | P1 | États demandé, autorisé, appliqué, observé | 1 | modèle d’état | Ouvert |
| ARCH-006 | P1 | Intentions et priorités | 1 | ADR + modèle | Ouvert |
| ARCH-007 | P1 | Exécutions | 1 | modèle + machine d’états | Ouvert |
| ARCH-008 | P1 | Dépendances | 1 | ADR + validation | Ouvert |
| ARCH-009 | P1 | Cycles interdits | 1 | stratégie | Ouvert |
| ARCH-010 | P2 | Inventaire générique des bus | 2 | ADR-0003 | Orientation décidée |
| ARCH-011 | P2 | Cartes et ports génériques | 2 | ADR-0003 | Orientation décidée |
| ARCH-012 | P2 | Adresses de bus dupliquées | 2 | règle | En attente |
| ARCH-013 | P2 | Actionneur binaire | 3 | ADR | En attente |
| ARCH-014 | P2 | État sûr par port/actionneur | 3 | ADR | En attente |
| ARCH-015 | P2 | Compatibilité `RelayAssignment` | 3 | transition | En attente |
| ARCH-016 | P2 | Commandes idempotentes | 3 | contrat | En attente |
| ARCH-017 | P2 | Reprise après reboot | 4 | ADR | En attente |
| ARCH-018 | P2 | Durées maximales | 4 | politique | En attente |
| ARCH-019 | P2 | Compensation d’échec | 4 | machine d’états | En attente |
| ARCH-020 | P2 | Pompe partagée | 5 | ADR | En attente |
| ARCH-021 | P2 | Précharge/post-fonctionnement | 5 | politique | En attente |
| ARCH-022 | P2 | Simultanéité | 5 | ADR | En attente |
| ARCH-023 | P2 | Qualité des observations | 6 | modèle | En attente |
| ARCH-024 | P2 | Comptage débitmètres | 6/11 | ADR | En attente |
| ARCH-025 | P2 | Défauts débit/fuite | 6 | politique | En attente |
| ARCH-026 | P2 | Format persistant indépendant | 7 | ADR | En attente |
| ARCH-027 | P2 | Candidate et activation atomique | 7 | ADR + prototype | Architecture définie |
| ARCH-028 | P2 | Migration V3 vers V4 | 7 | spécification | En attente |
| ARCH-029 | P2 | Import/export et modèles | 7 | spécification | En attente |
| ARCH-030 | P2 | Révision et concurrence | 8 | contrat API | En attente |
| ARCH-031 | P2 | Erreurs API V4 | 8 | spécification | En attente |
| ARCH-032 | P2 | Authentification locale | 8 | ADR | En attente |
| ARCH-033 | P3 | Équipements impulsionnels | 10 | extension | Différé |
| ARCH-034 | P3 | Équipements bidirectionnels | 10 | extension | Différé |
| ARCH-035 | P3 | Nœud distant | 11 | protocole | Différé |
| ARCH-036 | P3 | Mise à jour nœud distant | 11 | stratégie | Différé |
| ARCH-037 | P3 | Historique long | 12 | ADR | Différé |
| ARCH-038 | P3 | CI et tests hôte | 1/12 | plan de tests | Ouvert |
| ARCH-039 | P1 | API minimale de l’arène bornée | 1 | `BoundedArena.h` + test | **Prototype isolé réalisé** |
| ARCH-040 | P2 | Versionnement des modèles de cartes | 2/7 | ADR | Ouvert |
| ARCH-041 | P2 | Suppression d’une carte liée | 2/7 | ADR | Ouvert |
| ARCH-042 | P2 | Activation pendant une exécution | 4/7 | ADR | Ouvert |
| ARCH-043 | P1 | Catalogue de types d’équipements | 1/7 | registre + validation | Ouvert |
| ARCH-044 | P1 | Validateurs spécifiques des blocs de paramètres | 1 | interface | Ouvert |

## 4. Décisions acquises

- identifiants forts sur 16 bits ;
- configuration construite dans une arène bornée ;
- aucune capacité fonctionnelle imposée par des tableaux `MAX_*_V4` ;
- inventaire générique carte/port/binding ;
- `Equipment` compact et indépendant du matériel ;
- type référencé par `EquipmentTypeId` et descripteur partagé ;
- capacités portées par un masque de 32 bits ;
- paramètres spécifiques stockés dans un bloc versionné de l’arène ;
- nom stocké par offset et longueur ;
- aucune chaîne dynamique durable ;
- taille actuelle de `Equipment` : 28 octets sur test hôte C++11 ;
- NVS, planning, Web, LCD et runtime historique inchangés.

## 5. Backlog immédiat de la Phase 1

1. ARCH-005 — états demandé, autorisé, appliqué et observé ;
2. ARCH-044 — interface des validateurs spécifiques ;
3. ARCH-006 — intentions ;
4. ARCH-007 — exécutions ;
5. ARCH-008 et ARCH-009 — dépendances et cycles ;
6. ARCH-043 — catalogue de types avant construction complète de configuration.
