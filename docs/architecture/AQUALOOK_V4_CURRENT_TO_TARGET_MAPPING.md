# AquaLook V4 — Cartographie de l’existant vers la cible

**Run initial :** V4 Phase 0 — Run 0  
**Inspection détaillée :** V4 Phase 1 — Run 1.0  
**Base inspectée :** `8b0625fadcc684495625593945fed5b154634d89`  
**Branche :** `feature/relay-board-mapping`  
**Date :** 7 juillet 2026

## 1. Objet

Ce document relie l’architecture actuelle d’AquaLook à l’architecture cible V4.

Il identifie :

- ce qui existe déjà ;
- les fichiers, structures et fonctions impliqués ;
- ce qui peut être conservé ;
- ce qui doit être séparé ou réduit ;
- ce qui manque ;
- les contraintes de capacité et de mémoire ;
- l’ordre de transition recommandé.

Cette cartographie ne définit pas encore les structures C++ finales de V4.

## 2. Architecture actuelle constatée

```text
src/main.cpp
├── ConfigManager
├── RelaisManager
├── ScheduleManager
├── WiFiManager
├── NTPManager
├── WeatherManager
├── WebManager
├── DisplayManager
├── StorageManager
├── FaultManager
├── SystemDiagnostics
├── EventBus
└── EventLog
```

`main.cpp` constitue actuellement la racine de composition : il instancie globalement les managers, fixe l’ordre d’initialisation et câble le callback `ScheduleManager -> RelaisManager`.

Le flux exact est défini par :

```text
onRelayRequest(zone, state)
    -> relaisMgr.setRelay(zone, state)
```

Le travail validé sur `feature/relay-board-mapping` ajoute :

- `RelayTopology` ;
- `RelayAssignment` ;
- des rôles logiques de relais ;
- la résolution affectation vers carte/voie ;
- une image RAM des registres par carte ;
- la compatibilité `Zone N -> carte 0 -> voie N`.

## 3. Correspondance modules actuels / modules V4

| Existant | Responsabilité actuelle observée | Cible V4 | Décision de transition |
|---|---|---|---|
| `src/main.cpp` | assemblage, ordre de boot, callback relais | composition root | conserver ; remplacer progressivement le callback direct par des services |
| `ConfigManager` | LittleFS, NVS, migration JSON, stockage de toutes les configurations, application au planning | `ConfigurationService`, schémas de configuration, `PersistencePort`, adaptateur NVS | découpler sans réécriture globale |
| `ScheduleManager` | configuration planning en RAM, décision temporelle/pluie, état d’exécution, commande du callback | `AutomationEngine` + modèle d’exécution + futur orchestrateur | module le plus important à séparer |
| `RelaisManager` | topologie runtime, état logique par zone, image registres, I²C, timeout de sécurité | adaptateur d’actionneur relais | conserver comme infrastructure matérielle stricte |
| `RelayTopology` | cartes, affectations, rôles, validations et résolution | première implémentation de `HardwareAssignment` | conserver et généraliser plus tard |
| `WeatherManager` | acquisition et cache météo | fournisseur d’observations | conserver ; ajouter qualité et fraîcheur dans une phase dédiée |
| `NTPManager` | heure civile et epoch day | adaptateur `ClockPort` | conserver ; ne pas l’utiliser pour les durées monotones |
| `WiFiManager` | réseau local, reconnexion, portail captif | infrastructure réseau | conserver hors domaine |
| `WebManager` | API, sérialisation, commandes manuelles, modification directe des managers/configurations | adaptateur Web vers services applicatifs | réduire ses dépendances directes |
| `DisplayManager` | rendu LCD et commandes tactiles | adaptateur UI vers vues et services | même stratégie que le Web |
| `StorageManager` | SD et ressources statiques | adaptateur de stockage | conserver hors domaine |
| `FaultManager` | défauts globaux et masque actif | base du futur modèle de défaut | conserver temporairement ; ne pas le confondre avec le modèle métier complet |
| `EventBus` | flags globaux de rafraîchissement | notification technique légère | conserver temporairement |
| `EventLog` | journal borné | adaptateur `EventLogPort` | conserver et enrichir progressivement |

## 4. Inventaire précis des structures actuelles

### 4.1 Configuration persistante

Fichiers :

```text
src/ConfigManager.h
src/ConfigManager.cpp
```

Structures principales :

```text
CfgWifi
CfgTouch
CfgManual
CfgNtp
CfgOwm
CfgSystem
CfgRain
CfgSlot
CfgDaySchedule
CfgZone
CfgDisplay
PersistedConfig
```

`PersistedConfig` contient directement :

```text
magic
schema
payloadSize
wifi
touch
manual
ntp
owm
system
display
zones[MAX_ZONES]
crc32
```

Le schéma courant est :

```text
CFG_NVS_SCHEMA = 1
namespace = aqualook
clé = config
```

La lecture NVS exige actuellement une égalité exacte entre la longueur du blob et `sizeof(PersistedConfig)`.

Conséquence : toute modification directe de la structure persistée invaliderait le blob courant en l’absence de migration.

### 4.2 Configuration d’une zone

`CfgZone` contient :

```text
name[24]
mode
intervalDays
rain
7 x CfgDaySchedule
1 x CfgDaySchedule intervalle
```

Chaque journée contient `MAX_SLOTS = 5` créneaux.

La configuration courante dimensionne donc chaque zone pour :

```text
7 jours x 5 créneaux
+ 5 créneaux intervalle
= 40 créneaux
```

### 4.3 Modèle planning runtime

Fichiers :

```text
src/ScheduleManager.h
src/ScheduleManager.cpp
```

Structures :

```text
TimeSlot
DaySchedule
RainConfig
ZoneSchedule
ActiveSlot
```

`ZoneSchedule` reproduit presque entièrement le contenu fonctionnel de `CfgZone`, à l’exception du nom et avec ajout de `intervalAnchorDay`.

`ScheduleManager` contient :

```text
ZoneSchedule _zones[MAX_ZONES]
ActiveSlot _active[MAX_ZONES]
String _lastReason[MAX_ZONES]
uint8_t _nbZones
uint16_t _manualDurationMin
uint32_t _lastCheckedMinute
RelayCallback _relayCallback
```

Il existe donc une duplication volontaire mais coûteuse entre :

```text
ConfigManager::_zones[MAX_ZONES]
ScheduleManager::_zones[MAX_ZONES]
```

### 4.4 État d’exécution actuel

`ActiveSlot` porte uniquement :

```text
running
startMs
durationMs
isManual
```

Les raisons sont stockées séparément dans :

```text
String _lastReason[MAX_ZONES]
```

Limites constatées :

- pas d’identifiant d’exécution ;
- pas d’origine structurée ;
- pas de programme source identifié ;
- pas d’état intermédiaire ;
- pas de motif d’échec structuré ;
- pas de liste d’équipements mobilisés ;
- pas de reprise explicite ;
- utilisation de `String` dans un tableau runtime durable.

### 4.5 Topologie relais

Fichiers :

```text
src/RelayTopology.h
src/RelayTopology.cpp
src/RelaisManager.h
src/RelaisManager.cpp
```

Limites définies :

```text
MAX_RELAY_BOARDS = 8
MAX_CHANNELS_PER_BOARD = 8
MAX_RELAY_ASSIGNMENTS = MAX_ZONES = 16
```

Structures :

```text
RelayBoardConfig
RelayAssignment
RelayTopologyConfig
MappingResolution
```

Rôles présents :

```text
ROLE_UNUSED
ROLE_ZONE_VALVE
ROLE_PUMP
ROLE_AUX
ROLE_GREENHOUSE_VENT
ROLE_LIGHTING
```

`RelayAssignment` contient encore :

```text
role + targetIndex
```

Ce couple est utile pour la transition, mais ne doit pas devenir l’identité durable d’un équipement V4.

## 5. Limites et capacités constatées

Fichier :

```text
include/config.h
```

Valeurs actuelles :

```text
MAX_ZONES = 16
MAX_RELAIS = 16
NB_ZONES = 2
MAX_ACTIVE_ZONES = 8
NB_DAYS = 7
MAX_SLOTS = 5
```

Le système possède donc trois notions différentes :

- capacité interne de 16 zones ;
- capacité fonctionnelle actuelle de 8 zones ;
- valeur par défaut de 2 zones.

`ConfigManager::normalizeActiveZones()` applique en plus une contrainte spécifique au XL9535 : le nombre de zones est ramené à une valeur paire entre 2 et 8.

Cette contrainte historique devra être séparée de la capacité métier V4 : le nombre de zones ne doit pas être déterminé par le type d’une carte relais.

## 6. Flux d’exécution exacts

### 6.1 Boot

Ordre actuel observé :

1. série ;
2. défauts et diagnostics ;
3. bus I²C ;
4. scan I²C complet ;
5. chargement configuration ;
6. TFT et SD ;
7. initialisation relais et topologie ;
8. initialisation planning ;
9. câblage callback relais ;
10. application configuration vers planning ;
11. Wi-Fi ;
12. NTP ;
13. Web ;
14. météo ;
15. affichage complet.

Point favorable : le matériel relais est initialisé avant le démarrage du planificateur.

Point de vigilance : les cartes sont initialisées avec une image de registres sûre dépendant de la logique directe/inverse ; ce comportement doit rester inchangé pendant la Phase 1.

### 6.2 Boucle automatique

```text
si Wi-Fi connecté
    -> mise à jour NTP
    -> mise à jour météo

si NTP synchronisé
    -> ScheduleManager::update(..., rainMm)
        -> checkSlotEnd()
        -> sélection du créneau
        -> shouldWater()
        -> activateZone()
        -> callback
        -> RelaisManager::setRelay()
        -> setAssignment()
        -> I²C
```

### 6.3 Décision `shouldWater()`

La méthode décide actuellement :

- si le jour est valide ;
- si l’intervalle est valide ;
- si la pluie bloque ;
- quelle raison textuelle conserver.

Elle constitue le noyau initial du futur `AutomationEngine`.

### 6.4 Activation

`activateZone()` :

- marque immédiatement la zone active ;
- enregistre `millis()` ;
- calcule la durée ;
- marque manuel ou planifié ;
- demande le rafraîchissement ;
- appelle le callback relais.

Il existe une incohérence possible entre état logique et état matériel : la zone est marquée active avant de connaître le succès de `RelaisManager::setAssignment()`.

Cette observation justifie la séparation V4 entre :

```text
état demandé
état autorisé
état appliqué
état observé
```

### 6.5 Arrêt

`deactivateZone()` remet l’état runtime à zéro avant d’appeler le callback OFF.

L’arrêt matériel ne renvoie aucun résultat au planificateur.

### 6.6 Arrosage manuel

`startManualWatering()` et `stopManualWatering()` sont directement portés par `ScheduleManager`.

Le manuel utilise le même callback matériel que le planning, ce qui est positif, mais ne possède pas encore une intention structurée ni un arbitrage de priorité.

## 7. Flux de configuration exact

`ConfigManager` contient à la fois :

- la représentation persistante ;
- les valeurs runtime de configuration ;
- la migration historique ;
- les setters de l’API ;
- l’application vers `ScheduleManager`.

Le Web dépend directement de :

```text
NTPManager
WeatherManager
RelaisManager
ScheduleManager
ConfigManager
WiFiManager
```

`WebManager` expose des handlers spécifiques pour :

- planning ;
- intervalle ;
- pluie ;
- manuel ;
- système ;
- Wi-Fi ;
- NTP ;
- météo ;
- affichage ;
- défauts et logs.

Cette forte dépendance confirme la nécessité future d’un niveau de services applicatifs, mais aucune refonte Web n’est requise en Phase 1.

## 8. États et erreurs constatés

### États de zone

Le système distingue aujourd’hui essentiellement :

```text
running / non running
manual / planned
lastReason texte
```

### États relais

`RelaisManager` conserve :

```text
_state[MAX_ZONES]
_startMs[MAX_ZONES]
_assignmentState[MAX_RELAY_ASSIGNMENTS]
_boardReady[MAX_RELAY_BOARDS]
_hardwareReady
```

`_state[zone]` est mis à jour avant validation du mapping et avant succès I²C.

### Défauts

`FaultManager` est utilisé pour le défaut `RELAY_I2C`.

Les erreurs sont également publiées dans `EventLog` et déclenchent `EventBus::displayDirty`.

Le modèle de défaut V4 pourra réutiliser ces mécanismes comme adaptateurs, mais devra conserver source, cible, sévérité, horodatage et caractère bloquant.

## 9. Contraintes mémoire préliminaires

Les tailles exactes devront être mesurées par compilation avec `sizeof()` dans le Run 1.1. Les constats suivants sont néanmoins certains :

- 16 configurations complètes de zone sont stockées dans `ConfigManager` ;
- 16 plannings complets sont dupliqués dans `ScheduleManager` ;
- chaque zone réserve 40 créneaux ;
- 16 objets `String` sont conservés pour les raisons ;
- la topologie réserve 8 cartes et 16 affectations même en configuration simple ;
- `loadNvs()` réalise une allocation temporaire de `sizeof(PersistedConfig)` avec `malloc()` ;
- les managers sont instanciés globalement, donc leur mémoire fixe est réservée dès le démarrage.

Estimation structurelle, à confirmer sur la cible :

- les deux tableaux de zones représentent plusieurs kilo-octets chacun ;
- la duplication configuration/planning est probablement la première dépense RAM du domaine d’arrosage ;
- la Phase 1 ne doit pas ajouter une troisième copie complète des 16 zones ;
- le modèle V4 initial doit être compact, statique et limité ;
- les textes de raisons doivent tendre vers des codes bornés plutôt que des `String` persistants.

## 10. Objets métier actuels et cible

### Zone

**Actuel :** index de tableau, planning, pluie et état commandé.

**Cible :** identité stable, politique d’arrosage, référence vers un équipement de distribution.

### Programme ou automatisme

**Actuel :** créneau directement évalué par `ScheduleManager`.

**Cible :** règle temporelle produisant une intention.

### Exécution

**Actuel :** `ActiveSlot` par zone.

**Cible :** instance identifiée avec origine, progression, état, résultat et équipements mobilisés.

### Équipement

**Actuel :** absent.

**Cible :** objet central piloté.

### Actionneur

**Actuel :** implicite dans `RelayAssignment` et `RelaisManager`.

**Cible :** contrat abstrait entre équipement et matériel.

### Capteur

**Actuel :** valeur spécialisée fournie directement, par exemple `rainMm`.

**Cible :** observation qualifiée et horodatée.

## 11. Couplages à réduire

### 11.1 ConfigManager inclut ScheduleManager

`ConfigManager.h` inclut `ScheduleManager.h` afin d’utiliser `ZoneSchedule` et `TimeSlot` et expose `applyToSchedule()` et `syncZoneFromSchedule()`.

Conséquence : persistance et runtime métier sont liés dans les deux sens.

### 11.2 ScheduleManager possède quatre responsabilités

Il porte actuellement :

1. la configuration runtime ;
2. l’évaluation des règles ;
3. l’état d’exécution ;
4. la demande matérielle.

La migration doit séparer ces responsabilités sans casser l’algorithme existant.

### 11.3 RelaisManager conserve encore un état par zone

L’API historique `setRelay(zone, state)` reste utile pour la compatibilité, mais `_state[MAX_ZONES]` et `_startMs[MAX_ZONES]` gardent un couplage conceptuel à la zone.

La future API générique devra suivre l’équipement ou l’actionneur, tout en maintenant temporairement l’API historique.

### 11.4 WebManager dépend de tous les managers

Ce couplage ne doit pas être traité pendant la Phase 1. La cible est un futur service applicatif commun, pas une refonte immédiate des routes.

## 12. Éléments à préserver sans changement en Phase 1

- algorithmes jours fixes et intervalle ;
- ancrage du mode intervalle ;
- règle pluie ;
- format NVS et schéma 1 ;
- migration JSON historique ;
- routes Web ;
- ressources SD/LittleFS ;
- rendu LCD ;
- callback historique ;
- logique directe/inverse ;
- image sûre des registres au boot ;
- sécurité de durée maximale dans `RelaisManager::update()` ;
- ordre de boot ;
- compatibilité carte unique.

## 13. Éléments nouveaux nécessaires en Phase 1

- identifiants métier stables et compacts ;
- plafonds explicites pour équipements, capteurs, dépendances et exécutions ;
- modèle Equipment minimal ;
- capacités distinctes du type ;
- codes d’état et de résultat bornés ;
- intention ;
- exécution ;
- dépendance ;
- validation sans allocation dynamique ;
- tests hôte ou tests de compilation isolée lorsque possible.

## 14. Contraintes à imposer au modèle V4 initial

1. aucune modification de `PersistedConfig` ;
2. aucune nouvelle copie complète de `CfgZone[MAX_ZONES]` ;
3. aucun tableau durable de `String` ;
4. aucun lien matériel dans `Zone` ou `Equipment` ;
5. aucun appel à `RelaisManager` depuis les nouveaux modèles ;
6. aucune dépendance de domaine vers ArduinoJson, LittleFS, Preferences, Web ou TFT ;
7. structures bornées et tailles mesurables ;
8. identifiant stable distinct de l’index, avec conversion explicite ;
9. validation des références avant intégration runtime ;
10. comportement existant strictement inchangé jusqu’au Run 1.7.

## 15. Ordre de migration recommandé

1. mesurer les tailles exactes existantes ;
2. décider identifiants et limites ;
3. introduire types et capacités ;
4. introduire états et résultats ;
5. introduire intentions ;
6. introduire exécutions ;
7. introduire dépendances ;
8. construire une vue V4 légère depuis la configuration existante ;
9. introduire ensuite l’actionneur ;
10. préparer la persistance seulement après validation runtime.

## 16. Fichiers qui seront probablement concernés en Phase 1

### Nouveaux fichiers pressentis

Les noms restent à décider après les ADR :

```text
src/domain/EquipmentModel.*
src/domain/DomainIdentifiers.*
src/domain/IntentModel.*
src/domain/ExecutionModel.*
src/domain/DependencyModel.*
src/domain/FaultModel.*
```

### Fichiers existants à ne pas modifier dans les premiers runs

```text
src/ConfigManager.*
src/ScheduleManager.*
src/RelaisManager.*
src/WebManager.*
src/DisplayManager.*
src/main.cpp
```

Une intégration à ces fichiers n’est prévue qu’après stabilisation des modèles isolés.

## 17. Risques principaux

- ajouter une troisième représentation complète des zones ;
- utiliser des identifiants trop coûteux ;
- stocker des chaînes dans tous les objets métier ;
- reproduire les champs spécifiques de tous les équipements dans une structure universelle ;
- déplacer prématurément le planning existant ;
- faire dépendre le domaine d’Arduino ;
- confondre les rôles de relais avec les types d’équipement ;
- présenter `_state[zone]` comme une confirmation matérielle ;
- modifier le NVS avant la Phase 7.

## 18. Conclusion du Run 1.0

L’inspection confirme que la stratégie progressive est réalisable.

Les deux actifs techniques à préserver sont :

- le découplage historique par callback entre planning et relais ;
- le nouveau découplage `RelayAssignment -> carte/voie`.

Le premier travail de conception à effectuer n’est pas l’ajout d’`EquipmentManager`. Il est de choisir des identifiants et limites compatibles avec la mémoire disponible, puis de créer un modèle de domaine minimal sans dupliquer les plannings existants.
