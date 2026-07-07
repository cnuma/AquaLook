# AquaLook V4 — Backlog d’architecture

**Statut :** backlog vivant  
**Run d’origine :** Phase 0 — Run 0  
**Date :** 7 juillet 2026

## 1. Rôle

Ce document recense les décisions encore ouvertes. Il évite de surcharger l’architecture cible avec des détails prématurés.

Une entrée est retirée du backlog uniquement lorsqu’elle est :

- décidée par une ADR ;
- explicitement abandonnée ;
- ou remplacée par une décision plus récente.

## 2. Priorités

- **P0 :** décision nécessaire avant le prochain code ;
- **P1 :** décision nécessaire dans la phase en cours ;
- **P2 :** décision nécessaire avant une phase future identifiée ;
- **P3 :** orientation à conserver sans urgence.

## 3. Backlog

| ID | Priorité | Sujet | Phase cible | Livrable attendu | Statut |
|---|---:|---|---:|---|---|
| ARCH-001 | P0 | Format des identifiants stables | 1 | ADR | Ouvert |
| ARCH-002 | P0 | Limites `MAX_*` du domaine | 1 | ADR + budget mémoire | Ouvert |
| ARCH-003 | P0 | Représentation des paramètres spécifiques par type d’équipement | 1 | ADR | Ouvert |
| ARCH-004 | P0 | Distinction type d’équipement / capacités | 1 | ADR courte | Ouvert |
| ARCH-005 | P1 | États demandé, autorisé, appliqué, observé | 1 | modèle d’état | Ouvert |
| ARCH-006 | P1 | Modèle d’intention et priorités | 1 | ADR + modèle | Ouvert |
| ARCH-007 | P1 | Modèle d’exécution | 1 | modèle + machine d’états | Ouvert |
| ARCH-008 | P1 | Représentation des dépendances | 1 | ADR + règles de validation | Ouvert |
| ARCH-009 | P1 | Détection des cycles et dépendances interdites | 1 | stratégie de validation | Ouvert |
| ARCH-010 | P2 | Inventaire générique des bus | 2 | ADR | En attente |
| ARCH-011 | P2 | Identité et capacités des cartes matérielles | 2 | modèle matériel | En attente |
| ARCH-012 | P2 | Politique adresse I²C dupliquée | 2 | règle de validation | En attente |
| ARCH-013 | P2 | Actionneur binaire et contrat d’erreur | 3 | ADR | En attente |
| ARCH-014 | P2 | État sûr global ou par canal | 3 | ADR | En attente |
| ARCH-015 | P2 | Compatibilité de `RelayAssignment role + targetIndex` | 3 | décision de transition | En attente |
| ARCH-016 | P2 | Politique de commande idempotente | 3 | contrat actionneur | En attente |
| ARCH-017 | P2 | Politique de reprise après reboot | 4 | ADR | En attente |
| ARCH-018 | P2 | Durée maximale d’une exécution et d’un équipement | 4 | politique | En attente |
| ARCH-019 | P2 | Compensation après échec de séquence | 4 | machine d’états | En attente |
| ARCH-020 | P2 | Partage et comptage des demandes de pompe | 5 | ADR | En attente |
| ARCH-021 | P2 | Précharge et post-fonctionnement pompe | 5 | politique temporelle | En attente |
| ARCH-022 | P2 | Nombre de zones simultanées | 5 | ADR | En attente |
| ARCH-023 | P2 | Qualité et fraîcheur des observations | 6 | modèle capteur | En attente |
| ARCH-024 | P2 | Architecture de comptage des débitmètres | 6/11 | ADR | En attente |
| ARCH-025 | P2 | Politique absence de débit / fuite | 6 | politique de sécurité | En attente |
| ARCH-026 | P2 | Format persistant V4 | 7 | ADR | En attente |
| ARCH-027 | P2 | Double copie et génération de configuration | 7 | ADR | En attente |
| ARCH-028 | P2 | Migration V3 vers V4 | 7 | spécification de migration | En attente |
| ARCH-029 | P2 | Format d’import/export | 7 | spécification | En attente |
| ARCH-030 | P2 | Révision et concurrence de configuration | 8 | contrat API | En attente |
| ARCH-031 | P2 | Contrat des erreurs API V4 | 8 | spécification API | En attente |
| ARCH-032 | P2 | Authentification locale | 8 | ADR sécurité | En attente |
| ARCH-033 | P3 | Équipements impulsionnels | 10 | extension du modèle | Différé |
| ARCH-034 | P3 | Équipements bidirectionnels | 10 | extension du modèle | Différé |
| ARCH-035 | P3 | Protocole de nœud distant | 11 | ADR + protocole | Différé |
| ARCH-036 | P3 | Mise à jour d’un nœud distant | 11 | stratégie | Différé |
| ARCH-037 | P3 | Historique long et rotation SD | 12 | ADR stockage | Différé |
| ARCH-038 | P3 | CI et tests hôte du domaine | 1/12 | plan de tests | Ouvert |

## 4. Décisions déjà acquises

Les sujets suivants ne sont plus ouverts :

- l’équipement est l’objet métier piloté ;
- le relais reste un moyen matériel ;
- une zone ne connaît ni carte ni adresse I²C ;
- un automatisme produit une intention ;
- `RelaisManager` reste une couche matérielle ;
- le format NVS n’est pas modifié avant stabilisation du domaine ;
- le profil de compatibilité reste `Zone N -> carte 0 -> voie N` ;
- la segmentation des chats, le checkpoint et la synchronisation Git font partie de la gouvernance.

## 5. Backlog immédiat de la Phase 1

L’ordre de résolution recommandé est :

1. ARCH-001 — identifiants ;
2. ARCH-002 — limites et budget mémoire ;
3. ARCH-003 — paramètres spécifiques ;
4. ARCH-004 — types et capacités ;
5. ARCH-005 — états ;
6. ARCH-006 — intentions ;
7. ARCH-007 — exécutions ;
8. ARCH-008 et ARCH-009 — dépendances.

Aucun autre sujet ne doit retarder le démarrage du modèle de domaine.

## 6. Règle d’utilisation

Avant chaque run :

- sélectionner un nombre limité d’entrées ;
- préciser lesquelles bloquent le code ;
- créer les ADR nécessaires ;
- mettre à jour leur statut dans ce document ;
- reporter les sujets hors phase au lieu de les résoudre par anticipation.
