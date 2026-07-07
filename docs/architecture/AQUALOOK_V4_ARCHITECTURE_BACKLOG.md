# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Run d’origine :** Phase 0 — Run 0  
**Dernière mise à jour :** Phase 1 — Run 1.7, 7 juillet 2026

## 1. Rôle

Ce document recense les décisions ouvertes ou différées. Une entrée est close lorsqu’elle est décidée par une ADR, abandonnée ou remplacée.

## 2. État de la Phase 1

La Phase 1 est **clôturable sur le plan architectural et des modèles isolés**.

Elle n’est pas encore intégrée au firmware historique et reste soumise à :

```text
compilation PlatformIO complète
mesure flash
mesure RAM statique
heap libre au démarrage
heap minimum observé
```

## 3. Backlog principal

| ID | Sujet | Statut |
|---|---|---|
| ARCH-001 | Identifiants stables | **Décidé** |
| ARCH-002 | Capacité dynamique bornée | **Décidé et budgété** |
| ARCH-003 | Paramètres spécifiques | **Principe décidé** |
| ARCH-004 | Types et capacités | **Décidé** |
| ARCH-005 | États runtime | **Décidé et prototypé** |
| ARCH-006 | Intentions et priorités | **Décidé et prototypé** |
| ARCH-007 | Exécutions | **Décidé et prototypé** |
| ARCH-008 | Dépendances | **Décidé et prototypé** |
| ARCH-009 | Cycles interdits | **Décidé et prototypé** |
| ARCH-010 | Inventaire générique des bus | Phase 2 |
| ARCH-011 | Cartes et ports génériques | Phase 2 |
| ARCH-012 | Adresses de bus dupliquées | Phase 2 |
| ARCH-013 | Actionneur binaire | Phase 3 |
| ARCH-014 | État sûr par port/actionneur | Phase 3 |
| ARCH-015 | Compatibilité `RelayAssignment` | Phase 3 |
| ARCH-016 | Commandes idempotentes | Phase 3 |
| ARCH-017 | Reprise après reboot | Phase 4 |
| ARCH-018 | Durées maximales | Phase 4 |
| ARCH-019 | Compensation d’échec | Primitive décidée, politique par type différée |
| ARCH-020 | Pompe partagée | Phase 5 |
| ARCH-021 | Précharge et post-fonctionnement | Phase 5 |
| ARCH-022 | Simultanéité | Phase 5 |
| ARCH-023 | Qualité des observations | Phase 6 |
| ARCH-024 | Comptage débitmètres | Phase 6/11 |
| ARCH-025 | Défauts débit et fuite | Phase 6 |
| ARCH-026 | Format persistant indépendant | Phase 7 |
| ARCH-027 | Candidate et activation atomique | Architecture définie |
| ARCH-028 | Migration V3 vers V4 | Phase 7 |
| ARCH-029 | Import, export et modèles | Phase 7 |
| ARCH-030 | Révision et concurrence | Phase 8 |
| ARCH-031 | Erreurs API V4 | Phase 8 |
| ARCH-032 | Authentification locale | Phase 8 |
| ARCH-038 | CI et tests hôte | Toujours ouvert |
| ARCH-039 | API minimale de l’arène | **Prototype isolé réalisé** |
| ARCH-040 | Versionnement des modèles de cartes | Phase 2/7 |
| ARCH-041 | Suppression d’une carte liée | Phase 2/7 |
| ARCH-042 | Activation pendant une exécution | Phase 4/7 |
| ARCH-043 | Catalogue de types | **Catalogue minimal réalisé** |
| ARCH-044 | Validateurs spécifiques | Différé jusqu’aux schémas de paramètres |
| ARCH-045 | Registre borné des états | **Contrat générique réalisé** |
| ARCH-046 | Registre et politique des défauts | Contrat générique réalisé, politique différée |
| ARCH-047 | Tolérances de convergence | Différé |
| ARCH-048 | Calcul de santé | Différé |
| ARCH-049 | File bornée d’intentions | Contrat générique réalisé, saturation différée |
| ARCH-050 | Arbitre complet | Différé avant intégration runtime |
| ARCH-051 | Groupes de corrélation | Différé |
| ARCH-052 | Registre borné des exécutions | **Contrat générique réalisé** |
| ARCH-053 | Retry et backoff | Différé |
| ARCH-054 | Clôture après compensation | Différé |
| ARCH-055 | Résolution runtime des dépendances | Différé avant intégration runtime |
| ARCH-056 | Groupes d’exclusivité | Différé |
| ARCH-057 | Optimisation de l’index du graphe | Différé après mesures |
| ARCH-058 | Mesure PlatformIO de la Phase 1 | **Bloquant avant intégration, non bloquant pour Phase 2 isolée** |
| ARCH-059 | Mesure heap et fragmentation | **Bloquant avant intégration runtime** |

## 4. Plans de capacité décidés

```text
SMALL      16 équipements   5 168 octets
STANDARD   32 équipements  10 592 octets
EXTENDED   64 équipements  21 184 octets
```

Le profil `STANDARD` est la référence de conception.

## 5. Décisions acquises

- toutes les structures du domaine sont bornées et compactes ;
- aucun conteneur dynamique n’est requis ;
- `BoundedRegistry<T>` fournit le contrat commun ;
- cinq types d’équipements de base sont catalogués ;
- les budgets sont calculés à la compilation ;
- les seuils 8, 16 et 32 Kio sont protégés par assertions ;
- le domaine reste indépendant du matériel et du runtime historique.

## 6. Prochaine étape

Démarrer **AquaLook V4 — Phase 2 — Run 2.1 — Inventaire générique des bus et contrôleurs**.
