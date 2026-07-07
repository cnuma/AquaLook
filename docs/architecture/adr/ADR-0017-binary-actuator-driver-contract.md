# ADR-0017 — Contrat des actionneurs binaires et registre de drivers

- **Statut :** Acceptée
- **Date :** 7 juillet 2026
- **Phase :** V4 Phase 3 — Run 3.1

## Contexte

La Phase 2 a relié les équipements métier à des ports matériels génériques. La Phase 3 doit maintenant définir comment un port binaire est configuré, commandé, relu et placé dans son état sûr, sans dépendre d’Arduino ni raccorder immédiatement le moteur historique.

## Décision

Le contrat repose sur :

```text
BinaryActuatorDriverOps
BinaryActuatorDriverBinding
BinaryActuatorSession
BinaryActuatorDriverRegistry
```

Les opérations sont fournies par pointeurs de fonctions et reçoivent un contexte opaque `void*`.

## Opérations obligatoires

```text
configure
write
read
applySafeState
health
```

Un driver incomplet ne peut pas être enregistré.

## États binaires

```text
UNKNOWN
INACTIVE
ACTIVE
```

## Résultats

Chaque opération retourne :

```text
status
error
state
detail
```

Les statuts sont :

```text
APPLIED
ALREADY_APPLIED
FAILED
```

## Idempotence

Une commande identique à `lastApplied` retourne `ALREADY_APPLIED` sans rappeler le driver.

Cette règle :

- évite les écritures inutiles ;
- limite le trafic sur les bus ;
- réduit les commutations inutiles ;
- rend les répétitions de commande sûres.

## État sûr

L’état sûr est lu depuis `PortDefinition::safeState`.

Pour un actionneur binaire, les états sûrs directement applicables sont :

```text
INACTIVE
ACTIVE
```

`UNSPECIFIED`, `HIGH_IMPEDANCE` et `HOLD_LAST` ne sont pas interprétés arbitrairement par cette couche et produisent `SAFE_STATE_UNSUPPORTED`.

## Registre borné

Le registre :

- utilise un tableau fourni par l’appelant ;
- n’effectue aucune allocation ;
- associe un driver à un `ControllerTypeId` ;
- refuse les doublons ;
- refuse les dépassements de capacité ;
- permet la recherche par type de contrôleur.

## Compatibilité avec les profils compilés

Les futurs drivers concrets devront être enregistrés uniquement lorsque leur protocole et leur type de contrôleur sont disponibles dans le build.

Exemple futur :

```cpp
#if AQUALOOK_V4_ENABLE_I2C
register XL9535 driver
register MCP23017 driver
#endif
```

## Options rejetées

### Classes Arduino dans le contrat

Rejetées : le domaine doit rester compilable sur hôte.

### Allocation dynamique des drivers

Rejetée : capacité et fragmentation non déterministes.

### Déduire l’idempotence depuis une lecture matérielle à chaque commande

Rejeté comme comportement par défaut : coût bus inutile. Une rel lecture reste disponible explicitement.

### Appliquer un état sûr implicite

Rejeté : la configuration matérielle doit déclarer l’état sûr.

## Conséquences

- les drivers XL9535, MCP23017 ou GPIO pourront implémenter le même contrat ;
- le moteur futur commandera un port sans connaître son bus ;
- les erreurs peuvent être traduites vers `OperationError` ;
- aucune sortie du firmware actuel n’est modifiée.

## Invariants

1. Aucun driver incomplet n’est enregistré.
2. Une commande répétée est idempotente.
3. L’état sûr vient du port matériel.
4. Un driver est recherché par `ControllerTypeId`.
5. Aucun objet Arduino n’apparaît dans le contrat.
6. Aucun raccord à `RelaisManager` n’est réalisé dans ce run.
