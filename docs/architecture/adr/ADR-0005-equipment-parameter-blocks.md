# ADR-0005 — Paramètres spécifiques des équipements

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.2

## Contexte

Les équipements n’utilisent pas tous les mêmes paramètres :

- une vanne peut avoir une durée maximale et un délai d’ouverture ;
- une pompe peut avoir des temps de précharge, post-fonctionnement et anti-cycle ;
- un ouvrant peut avoir des durées de course ;
- un éclairage peut avoir un niveau ou une rampe.

Une structure universelle contenant tous les champs réserverait inutilement la somme des paramètres de toutes les familles pour chaque équipement.

## Décision

L’objet `Equipment` reste un en-tête compact. Ses paramètres spécifiques sont stockés dans l’arène sous forme d’un bloc opaque versionné référencé par :

```text
offset dans l’arène
taille en octets
version du schéma
```

Le `EquipmentTypeDescriptor` définit :

```text
version attendue
taille minimale
taille maximale
validateur spécifique futur
```

Le bloc est créé pendant la construction de la configuration candidate et devient immuable après activation.

## Représentation

```text
Equipment
├── id
├── typeId
├── capabilities
├── mode
├── safeState
├── flags
├── name -> TextRef
└── parameters -> ParameterBlockRef
```

`ParameterBlockRef` ne contient pas de pointeur persistant. Il contient un offset relatif à l’arène afin de rester valide si l’arène est adressée par sa base active.

## Paramètres absents

Un type peut autoriser un bloc vide lorsque sa taille minimale est zéro.

Un type qui exige des paramètres invalide un équipement dont le bloc est absent ou trop petit.

## Alignement

Le builder alloue le bloc avec l’alignement requis par sa structure temporaire. L’alignement fait partie du calcul de taille avant activation.

## Versionnement

La version du bloc est indépendante :

- de la version globale de configuration ;
- de la version du firmware ;
- de l’identifiant du type.

Une évolution incompatible du contenu incrémente `parameterSchemaVersion` et nécessite une migration explicite lors du chargement.

## Noms

Le nom lisible utilise également une référence par offset et taille dans l’arène. Il n’est ni l’identité de l’équipement ni un `String` durable.

## Options rejetées

### Union de tous les paramètres

Rejetée : taille croissante, couplage global et nécessité de recompiler le noyau pour chaque nouveau type.

### Pointeur brut dans Equipment

Rejeté : difficile à valider, sérialiser et déplacer lors d’une activation d’arène.

### JSON conservé en runtime

Rejeté : coût mémoire, conversions répétées et validation moins stricte.

### Allocation individuelle par équipement

Rejetée : fragmentation et cycle de vie complexe.

## Conséquences

- la taille fixe de `Equipment` reste inférieure ou égale à 32 octets ;
- les paramètres consomment uniquement leur taille réelle ;
- les types peuvent évoluer indépendamment ;
- la configuration candidate peut calculer exactement son coût ;
- la persistance future devra sérialiser le bloc et son schéma, pas son offset runtime.

## Invariants

1. Un bloc appartient à la même arène que l’équipement qui le référence.
2. Aucun bloc actif n’est modifié en place.
3. Offset, taille et version sont validés avant activation.
4. La taille du bloc respecte le descripteur du type.
5. Aucun pointeur brut n’est persisté.
6. La migration d’un schéma incompatible est explicite.
