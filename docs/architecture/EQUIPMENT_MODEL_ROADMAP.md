# AquaLook — Roadmap modèle équipements

Branche : `feature/relay-board-mapping`  
Base stable compilée : checkpoint `CHECKPOINT_2026-07-06_relay-assignment-roles_COMPILE_OK.md`

## 1. Pourquoi ce document

Le chantier relais a fait apparaître une limite importante : AquaLook ne doit pas seulement gérer des zones d'arrosage et des relais.

Dans une installation réelle, les relais peuvent piloter :

- des électrovannes ;
- une pompe électrique ;
- un contact sec ;
- des volets ou aérations de serre ;
- un éclairage ;
- un brumisateur ;
- un ventilateur ;
- d'autres équipements futurs.

Le relais est un moyen d'action physique. L'objet métier à piloter est l'équipement.

## 2. Principe cible

Séparer clairement quatre couches :

```text
Automatisation / planning / règle métier
    -> Équipement logique
        -> Affectation relais
            -> Carte relais I2C / voie physique
```

## 3. Définitions

### 3.1 Zone d'arrosage

Une zone reste une notion métier AquaLook :

- nom ;
- planning ;
- durée ;
- mode jours fixes / intervalle ;
- règle pluie ;
- éventuelle dépendance à une pompe.

Une zone utilise généralement une électrovanne, mais ne doit pas connaître directement l'adresse I2C ni la voie relais.

### 3.2 Équipement

Un équipement est un actionneur logique ou physique pilotable.

Exemples :

```text
Pompe forage
Électrovanne Zone 1
Électrovanne Zone 2
Volet serre Nord
Aération serre Sud
Éclairage serre
Brumisateur
Ventilateur
```

Chaque équipement possède un type et des paramètres propres.

### 3.3 RelayAssignment

`RelayAssignment` lie un usage logique à une voie relais physique.

Exemple :

```text
ROLE_ZONE_VALVE targetIndex=0 -> carte 0 voie 0
ROLE_PUMP       targetIndex=0 -> carte 1 voie 0
ROLE_LIGHTING   targetIndex=0 -> carte 1 voie 1
```

Cette couche existe déjà dans `RelayTopology`.

### 3.4 Carte relais

Une carte relais est un module I2C déclaré avec :

- contrôleur ;
- adresse ;
- nombre de voies ;
- logique directe/inverse ;
- état activé/désactivé.

## 4. Proposition de modèle Equipment

### 4.1 Types d'équipements

Proposition initiale :

```cpp
enum EquipmentType : uint8_t {
    EQUIP_UNUSED = 0,
    EQUIP_ZONE_VALVE = 1,
    EQUIP_PUMP = 2,
    EQUIP_AUX_CONTACT = 3,
    EQUIP_GREENHOUSE_VENT = 4,
    EQUIP_LIGHTING = 5,
    EQUIP_MISTER = 6,
    EQUIP_FAN = 7
};
```

### 4.2 Structure cible

```cpp
struct CfgEquipment {
    bool enabled;
    uint8_t type;
    char name[24];
    uint8_t relayAssignmentIndex;
    uint16_t startupDelayMs;
    uint16_t shutdownDelayMs;
    uint16_t minOnSec;
    uint16_t minOffSec;
};
```

Tous les champs ne seront pas utiles à tous les équipements, mais ils donnent une base simple et compacte.

## 5. Pompe électrique

### 5.1 Besoin

Quand l'eau n'arrive pas déjà sous pression, une pompe doit être démarrée avant l'ouverture d'une électrovanne.

### 5.2 Séquence cible

```text
Demande arrosage zone
    -> vérifier si la zone requiert une pompe
    -> activer pompe
    -> attendre délai montée pression
    -> ouvrir électrovanne
    -> attendre durée arrosage
    -> fermer électrovanne
    -> si aucune autre zone active ne demande la pompe :
        -> attendre délai arrêt éventuel
        -> couper pompe
```

### 5.3 Garde-fous

- Ne pas couper la pompe si une autre zone est encore active.
- Prévoir une temporisation anti-cycles courts.
- Prévoir un défaut si la pompe est mappée mais la carte relais est absente.
- Plus tard : conditionner la pompe à un pressostat ou débitmètre.

## 6. Serre

Les équipements de serre ne doivent pas être commandés directement par le moteur d'arrosage.

Ils relèvent plutôt d'un futur manager :

```text
GreenhouseManager
```

Exemples de règles futures :

```text
Température > seuil -> ouvrir volet
Humidité basse -> brumisateur
Luminosité basse -> éclairage
Température haute -> ventilateur
```

## 7. Auxiliaires

Les relais auxiliaires doivent pouvoir être utilisés en mode manuel ou automatisé.

Exemples :

- contact sec portail local ;
- éclairage temporaire ;
- alimentation électrovanne générale ;
- vanne maîtresse ;
- alarme défaut.

## 8. Impacts sur l'architecture actuelle

### 8.1 RelaisManager

`RelaisManager` doit rester une couche matérielle.

Il ne doit pas décider :

- quand arroser ;
- quand ouvrir un volet ;
- quand allumer une lampe ;
- quand démarrer une pompe.

Il doit seulement exécuter :

```text
activer / désactiver une affectation relais valide
```

### 8.2 ScheduleManager

`ScheduleManager` continue de gérer les zones et les plannings.

Il pourra plus tard demander :

```text
EquipmentManager::startZone(zone)
```

au lieu d'appeler directement :

```text
RelaisManager::setRelay(zone, state)
```

### 8.3 Futur EquipmentManager

Un `EquipmentManager` pourrait orchestrer :

- pompe ;
- électrovanne ;
- temporisations ;
- dépendances ;
- états de sécurité.

Flux cible :

```text
ScheduleManager
    -> EquipmentManager::startZone(zone)
        -> EquipmentManager démarre pompe si nécessaire
        -> EquipmentManager active l'électrovanne via RelaisManager
```

## 9. Impacts NVS

Ne pas intégrer tout de suite la persistance tant que le modèle métier n'est pas stabilisé.

Quand le modèle sera validé, la migration NVS devra probablement ajouter :

```cpp
RelayTopologyConfig relayTopology;
CfgEquipment equipments[MAX_EQUIPMENTS];
CfgZoneEquipmentLink zoneLinks[MAX_ZONES];
```

## 10. Proposition de runs suivants

### Run A — Stabilisation actuelle

- Tester upload + monitor.
- Vérifier que zone 1 et zone 2 commandent toujours les bons relais.
- Corriger si besoin.

### Run B — Equipment model minimal

- Ajouter `EquipmentModel.h/.cpp` isolé.
- Définir types d'équipements.
- Définir structure compacte.
- Aucun impact runtime.

### Run C — EquipmentManager squelette

- Ajouter `EquipmentManager` sans orchestration avancée.
- Prévoir API interne pour `startZone()` / `stopZone()`.
- Garder fallback vers relais existants.

### Run D — Pompe

- Ajouter rôle pompe.
- Ajouter dépendance zone -> pompe.
- Ajouter temporisation de démarrage.
- Ajouter coupure pompe quand plus aucune zone active.

### Run E — Persistance NVS

- Seulement après validation du modèle.
- Incrémenter schéma.
- Migrer l'ancien modèle.
- Ajouter API Web plus tard.

## 11. Décision recommandée

Ne pas modifier `ConfigManager` immédiatement.

La meilleure suite est :

1. tester matériellement l'état compilé ;
2. poser `EquipmentModel` isolé ;
3. poser `EquipmentManager` squelette ;
4. intégrer la pompe seulement après validation du flux ;
5. persister quand le modèle est stable.

Cette approche évite de figer trop tôt une structure NVS incomplète.
