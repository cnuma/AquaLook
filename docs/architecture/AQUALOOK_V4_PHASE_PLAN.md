# AquaLook V4 — Plan d’exécution par phases

**Statut :** plan directeur d’implémentation  
**Run d’origine :** Phase 0 — Run 0  
**Date :** 7 juillet 2026

## 1. Règles générales

Chaque phase est découpée en runs courts.

Chaque run doit préciser :

- objectif unique ;
- base Git exacte ;
- décisions requises ;
- fichiers concernés ;
- invariants ;
- hors périmètre ;
- tests ;
- checkpoint ;
- condition de sortie.

Une phase ne commence pas tant que ses décisions P0 ne sont pas closes.

Le changement de phase impose normalement un nouveau chat selon `docs/process/AQUALOOK_CHAT_SEGMENTATION_PROTOCOL.md`.

## 2. Phase 0 — Fondation documentaire

### Objectif

Transformer l’architecture cible en programme de travail gouverné.

### Runs

#### Run 0.0 — Architecture cible

- définir la cible V4 ;
- définir les invariants ;
- définir la roadmap générale.

**Statut : terminé.**

#### Run 0.1 — Gouvernance des chats

- définir les seuils de segmentation ;
- imposer checkpoint, Git et message de reprise.

**Statut : terminé.**

#### Run 0.2 — Préparation d’exécution

- cartographier l’existant vers la cible ;
- créer le backlog d’architecture ;
- détailler la Phase 1 ;
- créer le checkpoint de base.

**Statut : objet du présent Run 0.**

### Condition de sortie

- aucune modification runtime ;
- documents poussés ;
- prochaine action unique définie.

## 3. Phase 1 — Domaine V4 isolé

### Objectif

Introduire le vocabulaire et les types du domaine sans modifier le comportement d’AquaLook.

### Run 1.0 — Inspection source ciblée

**Nature : documentaire.**

- inspecter les structures actuelles de zones, programmes et états runtime ;
- identifier les tailles et limites existantes ;
- identifier les dépendances réelles de `ConfigManager`, `ScheduleManager` et `RelaisManager` ;
- relever les allocations dynamiques et contraintes mémoire ;
- produire une matrice fichiers/fonctions.

**Livrable :** mise à jour de `AQUALOOK_V4_CURRENT_TO_TARGET_MAPPING.md` avec références source précises.

### Run 1.1 — Identités et capacités maximales

**Décisions :** ARCH-001 et ARCH-002.

- choisir la représentation des identifiants stables ;
- définir les plafonds initiaux ;
- établir un budget RAM/flash ;
- décider comment convertir temporairement index et identifiants.

**Livrables :**

- ADR identifiants ;
- ADR limites embarquées ;
- aucun changement runtime.

### Run 1.2 — Modèle Equipment minimal

**Décisions :** ARCH-003 et ARCH-004.

- définir type, capacités, nom, activation, mode, état sûr ;
- définir la stratégie des paramètres spécifiques ;
- ne créer aucun manager d’exécution ;
- ne relier aucun relais.

**Tests :** compilation et tests de construction/validation si disponibles.

### Run 1.3 — États, défauts et résultats

**Décision :** ARCH-005.

- état demandé ;
- état autorisé ;
- état appliqué ;
- état observé ;
- santé ;
- défaut structuré ;
- résultat d’opération.

### Run 1.4 — Intentions

**Décision :** ARCH-006.

- origine ;
- priorité ;
- cible ;
- action ;
- validité ;
- corrélation.

Aucune intention ne doit encore déclencher un relais.

### Run 1.5 — Exécutions

**Décision :** ARCH-007.

- instance distincte de l’automatisme ;
- machine d’états minimale ;
- motif d’arrêt ;
- progression ;
- lien avec intention.

### Run 1.6 — Dépendances

**Décisions :** ARCH-008 et ARCH-009.

- relations `REQUIRES_BEFORE`, `REQUIRES_WHILE`, `EXCLUDES` ;
- validation des références ;
- détection de cycles ;
- aucune orchestration réelle.

### Run 1.7 — Adaptateur de compatibilité en mémoire

- construire une vue V4 à partir des zones actuelles ;
- créer des équipements électrovannes virtuels en mémoire ;
- ne pas écrire en NVS ;
- ne pas changer le chemin de commande.

### Run 1.8 — Revue de phase

- compilation ;
- tests ;
- mesure mémoire ;
- revue des invariants ;
- suppression des types inutiles ;
- checkpoint et tag `v4-domain-model-v1` après validation.

### Condition de sortie Phase 1

- domaine compilable et isolé ;
- aucun changement fonctionnel ;
- aucune migration NVS ;
- modèles suffisamment stables pour définir un actionneur.

## 4. Phase 2 — Inventaire matériel

Runs prévus :

1. modèle bus ;
2. modèle carte et capacités ;
3. registre configuré/détecté ;
4. validation des adresses et canaux ;
5. adaptation de `RelayTopology` ;
6. validation carte unique et multi-cartes.

## 5. Phase 3 — Actionneur binaire

Runs prévus :

1. contrat `ActuatorPort` ;
2. affectation matérielle ;
3. adaptateur relais ;
4. états demandé/appliqué ;
5. erreurs idempotentes ;
6. compatibilité des appels existants.

## 6. Phase 4 — Orchestrateur minimal

Runs prévus :

1. service de commande commun ;
2. arbitrage minimal ;
3. machine d’états non bloquante ;
4. zone pilote ;
5. arrêt et compensation ;
6. migration progressive du chemin historique.

## 7. Phase 5 — Pompe et ressources partagées

Runs prévus :

1. modèle pompe ;
2. dépendance zone/pompe ;
3. précharge ;
4. comptage de demandes ;
5. post-fonctionnement ;
6. anti-cycles courts ;
7. scénarios d’échec.

## 8. Phase 6 — Capteurs hydrauliques

Runs prévus :

1. observation et qualité ;
2. pression ;
3. débit ;
4. niveau ;
5. défaut absence de débit ;
6. fuite et débit inattendu ;
7. agrégation et historique court.

## 9. Phase 7 — Persistance V4

Runs prévus :

1. schéma persistant ;
2. configuration candidate ;
3. double copie ;
4. migration V3 ;
5. import/export ;
6. tests de corruption et coupure.

Aucun travail de cette phase ne doit être anticipé dans les phases 1 à 6 hors interfaces abstraites nécessaires.

## 10. Phase 8 — API V4

- ressources versionnées ;
- commandes explicites ;
- erreurs structurées ;
- révision de configuration ;
- adaptateur de compatibilité.

## 11. Phase 9 — Interface Web V4

- vues zones et équipements ;
- inventaire matériel ;
- affectations ;
- défauts ;
- configuration candidate ;
- visualisation demandé/appliqué/observé.

## 12. Phases 10 à 12

### Phase 10 — Automatismes climatiques

Serre, ventilation, éclairage, brumisation et règles à seuil.

### Phase 11 — Nœuds d’extension

Relais et comptage distants, supervision et état sûr.

### Phase 12 — Durcissement

Endurance, pannes, corruption, reboot, CI, documentation de maintenance.

## 13. Critères de passage entre phases

Le passage est autorisé lorsque :

- les critères de sortie sont satisfaits ;
- les tests sont exécutés ou explicitement reportés ;
- Git est propre et synchronisé ;
- le checkpoint est autonome ;
- le backlog est mis à jour ;
- les ADR sont acceptées ;
- le message de reprise du nouveau chat est produit.

## 14. Prochaine action unique

Démarrer **Phase 1 — Run 1.0 : inspection source ciblée**.

Ce run doit inspecter le dépôt réel et enrichir la cartographie avec les chemins de fichiers, structures, fonctions, appels et contraintes mémoire effectivement présents avant toute création de code V4.
