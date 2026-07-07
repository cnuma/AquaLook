# ADR-0016 — Budget de capacité de l’inventaire matériel

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 2 — Run 2.5

## Contexte

La Phase 2 a introduit les bus, contrôleurs, cartes, ports, bindings et tables de migration. Leur coût doit être borné avant l’introduction des drivers concrets.

## Décision

Trois profils matériels sont définis :

```text
SMALL
STANDARD
EXTENDED
```

Le budget comprend :

- `BusDefinition` ;
- `ControllerDefinition` ;
- `BoardDefinition` ;
- `PortDefinition` ;
- `EquipmentPortBinding` ;
- `LegacyEquipmentKey` ;
- `LegacyPortKey`.

Les catalogues constants et le profil de protocoles ne sont pas comptés dans la RAM d’inventaire active.

## Tailles unitaires

```text
BusDefinition             16 octets
ControllerDefinition      24 octets
BoardDefinition           16 octets
PortDefinition            16 octets
EquipmentPortBinding      16 octets
LegacyEquipmentKey         4 octets
LegacyPortKey              4 octets
```

## Profils

### SMALL

```text
2 bus
4 contrôleurs
4 cartes
16 ports
16 bindings
16 clés Equipment historiques
16 clés Port historiques
```

Budget :

```text
832 octets
1 664 octets actif + candidat
```

### STANDARD

```text
4 bus
8 contrôleurs
8 cartes
64 ports
64 bindings
32 clés Equipment historiques
64 clés Port historiques
```

Budget :

```text
2 816 octets
5 632 octets actif + candidat
```

### EXTENDED

```text
8 bus
16 contrôleurs
16 cartes
128 ports
128 bindings
64 clés Equipment historiques
128 clés Port historiques
```

Budget :

```text
5 632 octets
11 264 octets actif + candidat
```

## Profil recommandé

Le profil `STANDARD` devient la référence de conception de la Phase 2.

Il couvre largement :

- plusieurs bus I²C ou GPIO ;
- huit contrôleurs ;
- huit cartes ;
- soixante-quatre ports ;
- un binding par port ;
- la migration de la topologie relais historique.

## Seuils verrouillés

```text
SMALL     <= 2 Kio
STANDARD  <= 4 Kio
EXTENDED  <= 8 Kio
```

Les assertions portent sur un inventaire unique. Le double buffering actif/candidat doit être pris en compte dans le budget global du firmware.

## Options rejetées

### Allouer les tables à la demande

Rejeté : capacité et fragmentation moins prévisibles.

### Compter les catalogues comme RAM mutable

Rejeté : ils sont constants et destinés à la mémoire programme.

### Dimensionner uniquement selon le nombre actuel de relais

Rejeté : le modèle doit couvrir capteurs, compteurs, cartes distantes et extensions futures.

## Conséquences

- la Phase 2 dispose d’un coût RAM explicite ;
- le profil STANDARD reste compact ;
- une configuration candidate complète peut coexister avec l’active ;
- les futurs drivers devront être budgétés séparément ;
- les buffers de protocoles ne sont pas inclus ici.

## Invariants

1. Les structures matérielles restent bornées.
2. Le profil STANDARD est la référence.
3. Les catalogues restent constants.
4. Les drivers et buffers sont exclus du budget de Phase 2.
5. Toute évolution de structure doit recalculer les trois profils.
