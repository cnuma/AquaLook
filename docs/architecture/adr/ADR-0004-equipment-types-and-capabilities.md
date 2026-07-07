# ADR-0004 — Types et capacités des équipements

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.2

## Contexte

Un équipement doit être identifiable comme vanne, pompe, éclairage, ventilation ou futur équipement sans enfermer le runtime dans une hiérarchie C++ lourde.

Le type et les capacités ne répondent pas à la même question :

- le **type** indique la famille fonctionnelle et le schéma de paramètres ;
- les **capacités** indiquent les opérations réellement supportées.

Deux équipements de même type peuvent avoir des capacités différentes. Deux types différents peuvent partager certaines capacités.

## Décision

### Type

Chaque équipement référence un `EquipmentTypeId` stable sur 16 bits.

Le type n’est pas un `enum` fermé dans l’objet. Il est résolu dans un `EquipmentTypeDescriptor` versionné et partageable.

Le descripteur contient au minimum :

```text
EquipmentTypeId
nom technique
capacités obligatoires
capacités autorisées
version du schéma de paramètres
taille minimale des paramètres
taille maximale des paramètres
```

### Capacités

Chaque instance porte un masque `CapabilityMask` sur 32 bits.

Les capacités initiales sont :

```text
BINARY_COMMAND
PROPORTIONAL_COMMAND
BIDIRECTIONAL
TIMED_OPERATION
PULSE_COMMAND
POSITION_FEEDBACK
STATE_FEEDBACK
FAULT_FEEDBACK
SAFE_STATE
SHARED_RESOURCE
```

Le masque d’une instance doit :

- contenir toutes les capacités obligatoires du type ;
- ne contenir aucune capacité non autorisée par le type ;
- ne contenir aucun bit inconnu.

## Exemples

### Électrovanne simple

```text
type : irrigation-valve
capacités : BINARY_COMMAND, TIMED_OPERATION, SAFE_STATE
```

### Pompe supervisée

```text
type : water-pump
capacités : BINARY_COMMAND, TIMED_OPERATION, SAFE_STATE,
            STATE_FEEDBACK, FAULT_FEEDBACK, SHARED_RESOURCE
```

### Ouvrant motorisé

```text
type : greenhouse-vent
capacités : BIDIRECTIONAL, TIMED_OPERATION, POSITION_FEEDBACK, SAFE_STATE
```

## Options rejetées

### Un enum fermé contenant toutes les familles

Rejeté comme identité de type durable : toute nouvelle famille imposerait une modification du noyau et limiterait les définitions génériques.

### Déduire le type uniquement depuis les capacités

Rejeté : les capacités ne décrivent pas le schéma de paramètres ni la sémantique métier.

### Héritage C++ par équipement

Rejeté pour le modèle de configuration : coût, fragmentation conceptuelle et sérialisation plus complexe.

## Conséquences

- les nouveaux types peuvent être ajoutés par descripteur ;
- le noyau peut raisonner sur les capacités communes ;
- les paramètres restent validés par le type ;
- l’orchestrateur futur peut rechercher une capacité sans connaître tous les types ;
- les drivers matériels restent séparés des types métier.

## Invariants

1. Le type n’est pas déduit du port matériel.
2. Les capacités ne remplacent pas le type.
3. Le type ne contient aucune affectation matérielle.
4. Toute capacité inconnue invalide la candidate.
5. Un type possède un schéma de paramètres versionné.
