# ADR-0006 — Séparation configuration Equipment et état runtime

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.3

## Contexte

`Equipment` décrit une configuration immuable : identité, type, capacités, mode, état sûr, nom et paramètres.

Les états de fonctionnement changent fréquemment et ne doivent ni modifier la configuration active ni provoquer sa reconstruction : demande, autorisation, commande appliquée, observation, santé et défauts.

Le code actuel d’AquaLook confond encore partiellement commande logique et réussite matérielle. La V4 doit rendre ces étapes distinctes.

## Décision

L’état mutable est porté par `EquipmentRuntimeState`, séparé de `Equipment`.

Il contient quatre valeurs indépendantes :

```text
requested
  état demandé par une intention ou une commande

authorized
  état accepté après arbitrage, politiques et dépendances

applied
  état que l’adaptateur d’actionneur déclare avoir appliqué

observed
  état mesuré ou confirmé par une observation indépendante
```

Chaque valeur contient :

```text
kind
value
validity
```

Chaque étape possède son horodatage monotone en millisecondes.

## Représentation générique de valeur

`EquipmentStateValue` contient :

```text
int32 value
StateValueKind kind
StateValidity validity
```

Types initiaux :

```text
UNKNOWN
BINARY
PERCENT
POSITION
ENUMERATED
RAW_SIGNED
```

Validités initiales :

```text
UNKNOWN
VALID
STALE
INVALID
NOT_SUPPORTED
```

Cette représentation compacte évite une union différente pour chaque équipement tout en conservant une sémantique explicite.

## Convergence

Un équipement est convergé lorsque :

- l’état appliqué est valide ;
- et, si une observation est supportée, l’état observé valide correspond à l’état appliqué ;
- ou, si l’observation est explicitement non supportée, l’état autorisé correspond à l’état appliqué.

`requested == applied` ne suffit pas à prouver une réussite.

## Révision runtime

Chaque modification de l’état runtime incrémente une révision 16 bits.

Cette révision sert à détecter les changements pour les vues, diagnostics ou futures APIs. Elle n’est pas une identité persistante et peut reboucler.

## Santé

La santé synthétique est distincte des états de commande :

```text
UNKNOWN
HEALTHY
DEGRADED
FAULTED
UNAVAILABLE
```

La logique de calcul automatique de cette santé sera introduite ultérieurement.

## Options rejetées

### Ajouter les états dans `Equipment`

Rejeté : la configuration active deviendrait mutable et plus coûteuse à recopier ou versionner.

### Un seul booléen `active`

Rejeté : impossible de distinguer demande, autorisation, application et observation.

### Valeurs sous forme de chaînes

Rejeté : coût mémoire et validation faible.

## Conséquences

- `Equipment` reste à 28 octets ;
- les états ne sont alloués que pour les équipements suivis par le runtime ;
- l’interface pourra expliquer où une commande s’est arrêtée ;
- un échec matériel ne peut plus être présenté comme une activation réussie ;
- les équipements sans feedback déclarent explicitement `NOT_SUPPORTED`.

## Invariants

1. `EquipmentRuntimeState` référence `Equipment` par `EquipmentId`.
2. Aucun état mutable n’est stocké dans la configuration active.
3. `applied` signifie confirmation de l’adaptateur, pas observation physique.
4. `observed` provient d’une source d’observation ou vaut explicitement `NOT_SUPPORTED`.
5. Les durées utilisent un temps monotone, pas l’heure civile.
