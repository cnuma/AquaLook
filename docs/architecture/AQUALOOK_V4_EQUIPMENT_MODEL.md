# AquaLook V4 — Modèle Equipment minimal

**Statut :** référence de Phase 1  
**Date :** 7 juillet 2026  
**Run :** Phase 1 — Run 1.2

## 1. Objectif

Introduire un objet métier `Equipment` compact, générique et indépendant :

- du planning ;
- de la persistance ;
- du Web ;
- du LCD ;
- des cartes et ports ;
- des drivers matériels.

Le modèle doit pouvoir être construit dans une arène mémoire bornée et rester immuable après activation de la configuration.

## 2. Fichiers source

```text
src/domain/DomainIdentifiers.h
src/domain/BoundedArena.h
src/domain/EquipmentModel.h
src/domain/EquipmentModel.cpp
```

## 3. Structure Equipment

```text
Equipment
├── EquipmentId id
├── EquipmentTypeId typeId
├── CapabilityMask capabilities
├── ParameterBlockRef parameters
├── TextRef name
├── EquipmentMode mode
├── SafeState safeState
├── flags
└── reserved
```

Taille vérifiée sur compilation hôte C++11 :

```text
sizeof(Equipment) = 28 octets
```

Une assertion de compilation interdit de dépasser 32 octets sans décision explicite.

## 4. Identités fortes

`StrongId<Tag>` encapsule une valeur `uint16_t` sans surcoût mémoire.

Exemples :

```text
EquipmentId
EquipmentTypeId
BoardId
SensorId
AutomationId
ExecutionId
ZoneId
```

Les types empêchent de comparer ou d’affecter accidentellement des identifiants de familles différentes.

Valeurs réservées :

```text
0x0000 : invalide
0xFFFF : sentinelle réservée
```

## 5. Type et descripteur

L’instance ne contient pas une grosse définition du type. Elle référence un `EquipmentTypeDescriptor` partagé par son `EquipmentTypeId`.

Le descripteur contient :

```text
capacités obligatoires
capacités supportées
version du schéma de paramètres
taille minimale du bloc
taille maximale du bloc
nom technique
```

Le catalogue de types sera ajouté ultérieurement. Le modèle actuel accepte déjà un descripteur fourni au validateur.

## 6. Capacités

Le masque actuel réserve 32 bits et définit :

```text
CAP_BINARY_COMMAND
CAP_PROPORTIONAL_COMMAND
CAP_BIDIRECTIONAL
CAP_TIMED_OPERATION
CAP_PULSE_COMMAND
CAP_POSITION_FEEDBACK
CAP_STATE_FEEDBACK
CAP_FAULT_FEEDBACK
CAP_SAFE_STATE
CAP_SHARED_RESOURCE
```

Les capacités expriment ce qu’un équipement sait faire, jamais la façon dont il est câblé.

## 7. Modes

```text
DISABLED
AUTOMATIC
MANUAL
MAINTENANCE
```

Le mode décrit l’autorisation fonctionnelle générale. Il ne constitue pas encore une machine d’états d’exécution.

## 8. État sûr

```text
UNSPECIFIED
INACTIVE
ACTIVE
HOLD_LAST
```

Cette information est une politique métier déclarative. Sa traduction vers une sortie physique appartient aux futures couches actionneur et matériel.

`HOLD_LAST` devra être refusé ultérieurement pour les types ou installations où il n’est pas sûr.

## 9. Paramètres spécifiques

Les paramètres sont stockés dans l’arène et référencés par :

```text
offset : uint32_t
taille : uint16_t
version de schéma : uint16_t
```

Exemple de bloc pour une vanne :

```cpp
struct ValveParameters {
    uint16_t maximumRunMinutes;
    uint16_t openingDelayMs;
};
```

Seuls quatre octets sont alors consommés pour ces paramètres, auxquels s’ajoute l’alignement éventuel.

Le noyau Equipment ne connaît pas la structure interne du bloc. Le descripteur du type et son futur validateur en sont responsables.

## 10. Noms

Le nom lisible est stocké une seule fois dans l’arène avec un terminateur nul.

`TextRef` contient :

```text
offset
longueur hors terminateur
```

Le nom n’est jamais utilisé comme identité.

## 11. Arène bornée minimale

`BoundedArena` reçoit un buffer externe et sa capacité.

Elle fournit :

```text
allocate(size, alignment)
create<T>()
appendBytes()
offsetOf()
pointerAt()
reset()
used()
remaining()
```

Propriétés :

- allocation séquentielle ;
- alignement contrôlé ;
- aucune libération individuelle ;
- refus explicite en cas de dépassement ;
- offsets stables dans l’arène ;
- aucune dépendance Arduino.

Cette classe est une fondation isolée. Elle ne fixe encore aucun budget global et ne gère pas l’activation atomique.

## 12. Validation

`validateEquipment()` vérifie :

- identité valide ;
- type valide et correspondant au descripteur ;
- absence de capacité inconnue ;
- présence des capacités obligatoires ;
- absence de capacité interdite ;
- validité du mode ;
- validité de l’état sûr ;
- présence et bornes du bloc de paramètres ;
- version du schéma ;
- validité de la référence du nom.

La validation retourne un code structuré, sans journalisation ni effet de bord.

## 13. Test hôte réalisé

Le modèle a été compilé avec :

```text
g++ -std=c++11 -Wall -Wextra -Werror
```

Le test a construit dans une arène de 256 octets :

- un équipement vanne ;
- un bloc de paramètres de 4 octets ;
- le nom `Vanne nord` ;
- un descripteur de type ;
- une validation complète ;
- une résolution du nom et des paramètres.

Résultat :

```text
Equipment size = 28 octets
arena used = 16 octets
validation = OK
```

## 14. Hors périmètre

Le Run 1.2 n’ajoute pas :

- de registre ou collection d’équipements ;
- de catalogue global de types ;
- de builder de configuration complet ;
- de sérialisation ;
- de migration ;
- d’affectation matérielle ;
- d’état demandé, appliqué ou observé ;
- de commande ;
- d’intégration runtime.

## 15. Position exacte dans l’architecture

```text
ConfigurationBuilder futur
        ↓ construit
BoundedArena candidate
        ↓ contient
Equipment + blocs de paramètres + noms
        ↓ validés par
EquipmentTypeDescriptor
        ↓ exposés plus tard à
services applicatifs et orchestrateur
```

Aucun chemin ne relie encore `Equipment` à `ScheduleManager` ou `RelaisManager`.

## 16. Prochaine évolution

Le Run 1.3 devra introduire les états et résultats sans gonfler l’objet de configuration :

```text
état demandé
état autorisé
état appliqué
état observé
santé
défaut
résultat d’opération
```

Ces états runtime devront rester séparés de l’en-tête `Equipment` immuable.
