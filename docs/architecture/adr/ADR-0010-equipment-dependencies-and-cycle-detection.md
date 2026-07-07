# ADR-0010 — Dépendances entre équipements et détection des cycles

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.6

## Contexte

Les futurs automatismes devront coordonner plusieurs équipements :

- une vanne dépend d’une pompe ;
- une pompe démarre après l’ouverture d’une vanne ;
- deux équipements ne doivent pas fonctionner ensemble ;
- un équipement inhibe un autre ;
- certains arrêts doivent respecter un ordre.

Ces relations ne doivent pas référencer les cartes ou les ports physiques.

## Décision

Le modèle introduit `EquipmentDependency` comme relation compacte entre deux `EquipmentId`.

Convention unique :

```text
sourceId = équipement dépendant

targetId = équipement ou ressource dont dépend sourceId
```

La relation contient :

```text
sourceId
targetId
requiredState
delayMs
type
flags
```

Sa taille est fixée à 16 octets.

## Types de relations

```text
REQUIRES_STATE
START_AFTER
STOP_BEFORE
MUTUALLY_EXCLUSIVE
INHIBITS
```

### REQUIRES_STATE

`sourceId` nécessite que `targetId` soit dans `requiredState`.

### START_AFTER

Le démarrage de `sourceId` doit être postérieur à l’état ou à l’événement attendu sur `targetId`.

### STOP_BEFORE

L’arrêt de `sourceId` doit être ordonné relativement à `targetId`. La sémantique temporelle détaillée sera appliquée par l’orchestrateur.

### MUTUALLY_EXCLUSIVE

Les deux équipements ne doivent pas être actifs simultanément.

Cette relation est symétrique du point de vue métier. Une déclaration réciproque n’est pas considérée comme un cycle d’ordre.

### INHIBITS

L’état de `targetId` peut empêcher l’activation ou la poursuite de `sourceId`.

## Flags

```text
HARD
BLOCKING
PROPAGATE_STOP
```

- `HARD` : la dépendance ne peut pas être ignorée par une politique normale ;
- `BLOCKING` : son non-respect bloque l’autorisation ;
- `PROPAGATE_STOP` : l’arrêt ou l’indisponibilité de la cible peut demander l’arrêt de la source.

## Validation structurelle

La validation contrôle :

- identifiants valides ;
- absence d’auto-référence ;
- type reconnu ;
- état requis valide pour `REQUIRES_STATE` ;
- existence de la source ;
- existence de la cible ;
- absence de relation strictement dupliquée.

## Détection des cycles

Les relations suivantes participent au graphe orienté :

```text
REQUIRES_STATE
START_AFTER
STOP_BEFORE
```

Les relations suivantes n’y participent pas :

```text
MUTUALLY_EXCLUSIVE
INHIBITS
```

Une exclusivité ou une inhibition réciproque n’est pas automatiquement un cycle d’ordre. Elle sera résolue par l’arbitre et les politiques d’autorisation.

La détection utilise un tri topologique de type Kahn :

1. calcul du degré entrant ;
2. retrait progressif des nœuds sans prédécesseur ;
3. présence d’un cycle si tous les nœuds ne peuvent pas être retirés.

## Mémoire de travail

Le validateur ne réalise aucune allocation dynamique.

L’appelant fournit :

```text
uint16_t indegree[equipmentCount]
uint8_t processed[equipmentCount]
```

Ce workspace peut provenir de l’arène candidate ou d’un buffer temporaire borné.

Le coût temporaire est donc :

```text
3 octets × nombre d’équipements
```

hors alignement.

## Complexité

L’implémentation initiale privilégie la simplicité et la faible mémoire. Les recherches d’identifiants sont linéaires.

Complexité maximale actuelle :

```text
O(E × V + E²)
```

avec :

```text
V = nombre d’équipements
E = nombre de dépendances
```

Cette complexité reste acceptable pour les configurations embarquées visées. Une table d’index compacte pourra être ajoutée si les mesures le justifient.

## Options rejetées

### Référencer directement les ports

Rejeté : couplage du domaine à la topologie matérielle.

### Autoriser les cycles et les résoudre au runtime

Rejeté pour les dépendances d’ordre : comportement indéterministe et risque de blocage permanent.

### Allocation dynamique pendant la validation

Rejetée : fragmentation et échec moins prévisible.

### Considérer toute relation réciproque comme un cycle

Rejeté : l’exclusivité mutuelle est naturellement symétrique.

## Conséquences

- la configuration candidate peut être refusée avant activation ;
- les automatismes multi-équipements disposent d’un socle générique ;
- la résolution runtime des dépendances reste à implémenter dans l’orchestrateur ;
- les relations restent indépendantes du matériel ;
- un registre ou une vue compacte des dépendances sera nécessaire.

## Invariants

1. Une dépendance relie uniquement des `EquipmentId`.
2. Les deux références doivent exister dans la configuration candidate.
3. Une auto-référence est toujours invalide.
4. Les cycles d’ordre sont refusés avant activation.
5. Les relations symétriques ne sont pas traitées comme des arcs d’ordre.
6. Le validateur n’alloue aucune mémoire.
7. Aucun driver ou port physique n’est consulté.
