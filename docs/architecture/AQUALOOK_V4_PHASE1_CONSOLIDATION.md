# AquaLook V4 — Consolidation de la Phase 1

**Date :** 7 juillet 2026  
**Branche :** `feature/aqualook-v4-domain`  
**Run :** Phase 1 — Run 1.7

## 1. Domaine couvert

La Phase 1 a défini les briques indépendantes du matériel :

```text
Equipment
EquipmentRuntimeState
EquipmentFault
OperationResult
EquipmentIntent
EquipmentExecution
EquipmentDependency
BoundedArena
BoundedRegistry
EquipmentTypeCatalog
DomainCapacityPlan
```

## 2. Tailles unitaires

```text
Equipment                    28 octets
EquipmentRuntimeState        56 octets
EquipmentFault               16 octets
OperationResult              16 octets
EquipmentIntent              32 octets
EquipmentExecution           40 octets
EquipmentDependency          16 octets
```

Les tailles sont verrouillées par assertions de compilation dans les modèles concernés.

## 3. Plans de capacité

### SMALL

```text
16 équipements
16 états runtime
16 défauts
16 intentions
8 exécutions
24 dépendances
16 résultats
2 Kio d’arène
Budget : 5 168 octets
```

### STANDARD — profil recommandé

```text
32 équipements
32 états runtime
32 défauts
32 intentions
16 exécutions
64 dépendances
32 résultats
4 Kio d’arène
Budget : 10 592 octets
```

### EXTENDED

```text
64 équipements
64 états runtime
64 défauts
64 intentions
32 exécutions
128 dépendances
64 résultats
8 Kio d’arène
Budget : 21 184 octets
```

## 4. Registres bornés

`BoundedRegistry<T>` fournit un contrat commun à stockage externe :

```text
append
findIf
at
removeAt
clear
size
capacity
full
```

Aucune allocation dynamique n’est réalisée. Les spécialisations futures pourront ajouter des index par identifiant sans modifier les modèles stockés.

## 5. Catalogue minimal de types

Le catalogue initial fixe les identifiants suivants :

```text
1  ZONE_VALVE
2  PUMP
3  AUXILIARY
4  GREENHOUSE_VENT
5  LIGHTING
```

Chaque descripteur déclare :

- capacités obligatoires ;
- capacités autorisées ;
- version du schéma de paramètres ;
- taille minimale et maximale ;
- nom technique stable.

Les schémas binaires précis de paramètres et leurs validateurs métier restent à définir au moment de l’introduction concrète de chaque type.

## 6. Lecture du budget

Le budget du domaine ne représente pas la consommation totale du firmware.

Il exclut notamment :

```text
Wi-Fi et TCP/IP
ESPAsyncWebServer
buffers JSON
stacks FreeRTOS
écran et sprites
bibliothèques Arduino
heap et fragmentation
configuration candidate simultanée
historique et journaux
```

Le profil STANDARD à environ 10,3 Kio laisse une marge théorique importante, mais seule une compilation PlatformIO et des mesures runtime permettront de valider l’intégration complète.

## 7. État de clôture de Phase 1

### Modèles considérés suffisamment définis

- identifiants ;
- équipements et capacités ;
- configuration en arène ;
- états runtime ;
- défauts et résultats ;
- intentions ;
- exécutions ;
- dépendances et cycles ;
- registre borné générique ;
- budget de capacité.

### Éléments volontairement différés

- arbitre runtime complet ;
- files réellement instanciées dans le firmware ;
- politique de saturation ;
- index rapides par identifiant ;
- validateurs métier des paramètres ;
- synthèse de santé ;
- tolérances analogiques ;
- retry et compensation par type ;
- intégration au moteur historique.

## 8. Décision

La **Phase 1 peut être clôturée sur le plan architectural et des modèles isolés**.

Elle ne doit pas encore être considérée comme intégrée ou validée sur ESP32.

Le passage à la Phase 2 est autorisé, sous réserve de conserver deux validations différées :

```text
compilation PlatformIO complète
mesure flash, RAM statique, heap libre et heap minimum
```

## 9. Phase suivante

La Phase 2 doit porter sur l’inventaire matériel générique :

```text
bus
contrôleurs
cartes
adresses
ports
capacités matérielles
bindings Equipment vers ports
validation des collisions
```

Elle devra préserver la compatibilité avec `RelayTopology` sans raccorder immédiatement le nouveau domaine au moteur d’arrosage actuel.
