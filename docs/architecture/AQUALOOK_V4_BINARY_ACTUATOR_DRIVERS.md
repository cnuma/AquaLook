# AquaLook V4 — Actionneurs binaires et registre de drivers

**Date :** 7 juillet 2026  
**Run :** Phase 3 — Run 3.1

## 1. Chaîne d’appel cible

```text
EquipmentIntent
  -> EquipmentExecution
      -> EquipmentPortBinding
          -> PortDefinition
              -> ControllerDefinition
                  -> BinaryActuatorDriverRegistry
                      -> BinaryActuatorDriverOps
```

Le moteur métier ne connaît pas le bus ni le composant matériel concret.

## 2. Contrat

Un driver binaire fournit :

```text
configure
write
read
applySafeState
health
```

Les fonctions reçoivent :

- un contexte opaque ;
- le contrôleur ;
- le port ;
- éventuellement l’état demandé.

## 3. Session

`BinaryActuatorSession` conserve :

```text
PortId
lastApplied
configured
revision
```

Taille :

```text
6 octets
```

Cette session permet l’idempotence sans dépendre du driver concret.

## 4. Résultat

`BinaryActuatorDriverResult` occupe :

```text
8 octets
```

Il contient :

```text
status
error
state
detail
```

## 5. Idempotence

Exemple :

```text
commande ACTIVE
écriture driver

commande ACTIVE répétée
ALREADY_APPLIED
aucune nouvelle écriture
```

Une lecture explicite peut toujours resynchroniser la session avec le matériel.

## 6. État sûr

Le port déclare son état sûr.

```text
PortSafeState::INACTIVE -> sortie inactive
PortSafeState::ACTIVE   -> sortie active
```

Les autres valeurs ne sont pas inventées par la couche binaire.

## 7. Registre

Le registre est borné, sans allocation, et indexé par `ControllerTypeId`.

Il refuse :

```text
driver incomplet
type invalide
doublon de type
capacité dépassée
```

## 8. Traduction des erreurs

Les erreurs de driver sont convertibles vers `OperationError` :

```text
UNAVAILABLE            -> ACTUATOR_UNAVAILABLE
COMMUNICATION_ERROR    -> COMMUNICATION_ERROR
READBACK_ERROR         -> COMMUNICATION_ERROR
UNSUPPORTED_PORT       -> CAPABILITY_NOT_SUPPORTED
INTERNAL_ERROR         -> INTERNAL_ERROR
```

## 9. Validation hôte

Cas validés :

- résultat compact de 8 octets ;
- session compacte de 6 octets ;
- configuration ;
- première commande appliquée ;
- deuxième commande identique ignorée ;
- une seule écriture réellement effectuée ;
- registre borné ;
- doublon refusé ;
- dépassement refusé ;
- recherche par type.

Résultat :

```text
Compilation hôte OK
Résultat : 8 6 1
Compilation registre OK
Résultat : 2 2
```

## 10. Hors périmètre

- driver GPIO concret ;
- driver XL9535 concret ;
- driver MCP23017 concret ;
- accès I²C ou GPIO ;
- registre instancié dans le firmware ;
- raccord à `RelaisManager` ;
- persistance ;
- tâches ou synchronisation concurrente.

## 11. Suite

Le Run 3.2 devra implémenter un premier driver binaire isolé, probablement un driver mémoire simulé ou GPIO conditionnel, puis valider le contrat sans toucher au moteur d’arrosage historique.
