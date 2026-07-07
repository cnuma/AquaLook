# ADR-0001 — Identifiants stables AquaLook V4

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 1 — Run 1.1

## Contexte

AquaLook utilise aujourd’hui principalement des index de tableaux : zone 0, relais 0, carte 0, voie 0. Ces index sont compacts mais ne sont pas stables lorsqu’un élément est supprimé, réordonné ou migré.

La V4 doit référencer durablement des zones, équipements, capteurs, cartes, ports, automatismes et exécutions sans stocker de chaînes longues dans chaque relation.

## Décision

Les entités configurables utilisent un identifiant stable compact sur 16 bits :

```text
EntityId = uint16_t
```

Valeurs réservées :

```text
0x0000 = identifiant invalide / absent
0x0001 à 0xFFFE = identifiants utilisables
0xFFFF = valeur sentinelle réservée
```

Chaque famille conserve son propre type C++ fort ou wrapper léger :

```text
ZoneId
EquipmentId
SensorId
BoardId
PortId
AutomationId
ExecutionId
```

Deux entités de familles différentes peuvent porter la même valeur numérique sans être interchangeables.

## Index runtime

Les tableaux embarqués restent adressés par index compact :

```text
index = uint8_t
```

La conversion est explicite :

```text
EntityId -> recherche dans registre -> index runtime
index runtime -> lecture de l’EntityId stocké dans l’entrée
```

L’identifiant n’est jamais supposé égal à l’index.

## Génération

La configuration attribue les identifiants de manière monotone dans chaque famille. Un identifiant supprimé n’est pas réutilisé automatiquement dans la même génération de configuration.

La persistance exacte du compteur sera définie en Phase 7.

## Identité des ports

Un port n’utilise pas un identifiant global autonome obligatoire. Il est identifié de façon stable par le couple :

```text
BoardId + portIndex
```

Un `PortBinding` référence donc :

```text
boardId
portIndex
```

Cette décision évite de réserver un identifiant supplémentaire pour chaque port physique tout en conservant une référence stable si la carte ne change pas.

## Identité des exécutions

`ExecutionId` utilise également 16 bits dans le modèle initial. Il est unique parmi les exécutions encore présentes dans le journal runtime. Le mécanisme de rollover devra comparer les identifiants avec leur contexte de génération ou d’horodatage si un historique long est ajouté.

## Options rejetées

### Utiliser uniquement les index

Rejeté : réordonnancement, suppression et migration casseraient les références.

### UUID 128 bits

Rejeté : coût mémoire, coût JSON et complexité disproportionnés pour un ESP32 local.

### Chaînes comme `zone-jardin-nord`

Rejeté comme identifiant interne : utiles comme alias humain, mais trop coûteuses dans chaque relation.

### Identifiant global typé dans les bits hauts

Différé : la séparation par types C++ apporte déjà la sécurité nécessaire sans réduire l’espace disponible.

## Conséquences

- relations compactes sur 2 octets ;
- registres nécessaires pour résoudre identifiant vers index ;
- les noms restent des attributs modifiables, jamais des identifiants ;
- les API pourront exposer les identifiants numériques et un alias lisible ;
- les index historiques de zones restent supportés par un adaptateur de compatibilité.

## Invariants

1. `0` signifie toujours « aucune référence ».
2. Un identifiant stable ne change pas lors d’un réordonnancement.
3. Un index runtime n’est jamais persisté comme référence durable.
4. Le nom d’une entité n’est pas son identité.
5. Toute conversion identifiant/index peut échouer et doit retourner un résultat explicite.
