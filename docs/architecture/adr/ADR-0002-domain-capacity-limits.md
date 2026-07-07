# ADR-0002 — Capacité dynamique bornée de la configuration V4

- **Statut :** Acceptée — remplace la première version de l’ADR
- **Date initiale :** 7 juillet 2026
- **Révision :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.1 corrigé

## Contexte

La première version de cette ADR proposait des plafonds fonctionnels fixes tels que :

```text
MAX_EQUIPMENTS_V4
MAX_SENSORS_V4
MAX_HARDWARE_BOARDS
MAX_PORTS_PER_BOARD
MAX_PORT_BINDINGS
```

Cette approche rendait la capacité du produit trop dépendante de tableaux préalloués et de valeurs arbitraires.

Or la configuration réelle doit être déduite des éléments effectivement déclarés : modèles de cartes, cartes installées, ports exposés, équipements, capteurs, automatismes, dépendances et bindings.

Exemple :

```text
1 carte de 4 sorties TOR
+ 1 carte de 2 sorties TOR
+ 1 carte de 4 entrées
=
3 cartes
6 sorties
4 entrées
10 ports physiques
```

La configuration runtime ne doit réserver que les objets nécessaires à cette installation.

## Décision

AquaLook V4 utilise une **construction dynamique dans une mémoire bornée**, appelée ici option C.

```text
Configuration déclarée
    -> ConfigurationBuilder
    -> calcul des besoins
    -> allocation séquentielle dans une arène candidate bornée
    -> validation complète
    -> activation atomique
```

La capacité fonctionnelle n’est plus définie par une liste de constantes `MAX_*` correspondant aux différentes catégories métier.

Elle est déterminée par :

1. les objets réellement présents dans la configuration ;
2. leur taille sérialisée et runtime ;
3. le budget mémoire disponible ;
4. la compatibilité des références et capacités ;
5. des gardes absolues de sécurité contre les configurations manifestement invalides.

## Arènes mémoire

Le modèle cible distingue au minimum :

```text
ActiveConfigurationArena
CandidateConfigurationArena
RuntimeExecutionArena
```

### ActiveConfigurationArena

Contient la configuration actuellement validée et utilisée par le système.

Elle n’est jamais modifiée partiellement.

### CandidateConfigurationArena

Reçoit une nouvelle configuration lors d’un import, d’une modification ou d’une migration.

Elle est construite indépendamment de la configuration active.

### RuntimeExecutionArena

Contient les objets temporaires de fonctionnement : intentions en attente, exécutions actives, résultats courts et états d’orchestration.

Elle est séparée de la configuration durable.

## Allocation

L’allocation dans une arène est :

- séquentielle ;
- alignée ;
- bornée ;
- sans libération individuelle ;
- réinitialisée globalement lors de l’abandon ou du remplacement de la configuration.

Cette stratégie évite la fragmentation du heap tout en adaptant la consommation au contenu réel.

Le constructeur doit pouvoir répondre avant activation :

```text
mémoire requise
mémoire disponible
nombre de cartes
nombre total de ports
nombre d’entrées et de sorties par nature
nombre d’équipements
nombre de capteurs
nombre d’automatismes
nombre de dépendances
nombre de bindings
```

## Budgets, pas capacités fonctionnelles

Les constantes structurantes deviennent des budgets techniques, par exemple :

```text
CONFIGURATION_ARENA_BYTES
CANDIDATE_CONFIGURATION_ARENA_BYTES
RUNTIME_EXECUTION_ARENA_BYTES
```

Leur valeur exacte sera fixée après mesure sur la cible.

Les deux arènes de configuration peuvent être :

- simultanément présentes en RAM si la marge le permet ;
- ou construites dans une mémoire temporaire différente, puis activées selon une stratégie à préciser en Phase 7.

## Gardes absolues

Quelques limites très larges restent autorisées pour protéger les parseurs et algorithmes :

```text
ABSOLUTE_MAX_OBJECTS
ABSOLUTE_MAX_RELATIONS
ABSOLUTE_MAX_PORTS_PER_DECLARATION
MAX_CONFIGURATION_DEPTH
MAX_CONFIGURATION_INPUT_BYTES
```

Ces limites :

- ne décrivent pas une installation normale ;
- ne conduisent pas à préallouer les objets correspondants ;
- servent uniquement à rejeter rapidement une entrée corrompue ou hostile ;
- doivent être nettement supérieures aux besoins réalistes tout en restant calculables.

## Modèles de cartes

Le nombre de ports d’une carte vient de son `BoardModelDescriptor`, pas d’un `MAX_PORTS_PER_BOARD` métier.

Une définition peut exposer :

```text
1, 2, 4, 8, 16 ou davantage de ports
```

à condition que :

- le descripteur soit valide ;
- le driver sache les gérer ;
- la configuration candidate respecte son budget ;
- les temps de validation et d’exécution restent compatibles avec le système.

## Validation avant activation

Une configuration candidate n’est activée que si :

1. sa construction est complète ;
2. toutes les références sont résolues ;
3. les ports existent ;
4. les directions et capacités sont compatibles ;
5. les identifiants sont uniques ;
6. les dépendances sont valides et sans cycle interdit ;
7. aucun binding exclusif n’est dupliqué ;
8. le budget mémoire est respecté ;
9. les limites absolues ne sont pas dépassées ;
10. la configuration active peut être remplacée atomiquement.

En cas d’échec, la configuration active reste inchangée.

## Évolution de la configuration

Les opérations supportées conceptuellement sont :

```text
ajouter une carte
retirer une carte
remplacer un modèle
mettre à niveau une définition
ajouter ou retirer des équipements
ajouter ou retirer des capteurs
modifier les bindings
modifier les automatismes
```

Toute opération produit une nouvelle configuration candidate complète. Il n’existe pas de modification partielle directe de l’arène active.

## Options rejetées

### Tableaux fixes par famille

Rejeté comme fondation V4 : gaspillage de RAM, plafonds arbitraires et difficulté à faire évoluer les catégories.

### Allocation générale par `new`, `malloc` ou `std::vector`

Rejetée pour la configuration active : fragmentation et échecs moins prévisibles.

### Absence totale de borne

Rejetée : un microcontrôleur doit pouvoir refuser une configuration avant épuisement mémoire.

## Conséquences

- les anciens plafonds `MAX_*_V4` ne sont plus des décisions d’architecture ;
- la configuration réelle définit automatiquement ses propres compteurs ;
- l’inventaire est proportionnel aux cartes et ports déclarés ;
- la mémoire est calculée avant activation ;
- le système peut évoluer par ajout, suppression ou remplacement de définitions génériques ;
- la persistance devra permettre une configuration candidate, une validation puis une activation atomique ;
- des diagnostics de budget deviennent obligatoires.

## Invariants

1. La configuration active n’est jamais modifiée partiellement.
2. Une configuration invalide ne remplace jamais la configuration active.
3. La consommation mémoire est calculable avant activation.
4. Aucun objet n’est créé uniquement parce qu’un plafond théorique le permet.
5. Les ports disponibles proviennent des modèles de cartes réellement déclarés.
6. Les limites absolues de sécurité ne sont pas des capacités commerciales ou fonctionnelles.
7. L’arène ne réalise aucune libération individuelle.
8. Le runtime d’exécution est séparé de la configuration durable.
