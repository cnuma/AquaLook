# AquaLook — Architecture relais multi-cartes I2C

Base de travail : `refactor/static-assets-sd`  
Run initial : `feature/relay-board-mapping`  
Issue de suivi : #2

## 1. Contexte

Le modèle actuel d'AquaLook expose un nombre de relais global : 1, 2, 4 ou 8. Ce modèle est suffisant pour une carte unique, mais il ne décrit pas le matériel réel lorsque plusieurs cartes relais I2C sont utilisées.

Les cartes relais utilisées reposent sur un contrôleur I2C adressable par broches d'adresse. Il faut donc représenter explicitement :

- les cartes relais physiques ;
- leur adresse I2C ;
- leur capacité utile ;
- la voie physique commandée ;
- le lien entre une zone logique AquaLook et une voie de relais.

## 2. Objectif d'architecture

Découpler définitivement la zone logique AquaLook de la voie relais physique.

Modèle cible :

```text
Programme d'arrosage
    -> Zone logique AquaLook
        -> Mapping zone/relais
            -> Carte relais I2C
                -> Voie physique de la carte
```

La zone reste l'objet métier central. Le relais devient une affectation matérielle configurable.

## 3. Définitions

### 3.1 Zone logique

Une zone est une zone d'arrosage AquaLook :

- nom ;
- planning ;
- mode jours fixes ou intervalle ;
- paramètres pluie ;
- état logique demandé par le moteur.

La zone ne doit pas connaître directement l'adresse I2C ni le bit matériel.

### 3.2 Carte relais

Une carte relais est une entité matérielle déclarée dans la configuration :

- `enabled` : carte activée ou non ;
- `controller` : type de contrôleur, par exemple XL9535 ou MCP23017 ;
- `i2cAddress` : adresse réelle sur le bus I2C ;
- `channelCount` : nombre de voies utilisées sur cette carte, typiquement 1, 2, 4 ou 8 ;
- `logic` : logique directe ou inverse ;
- `label` optionnel plus tard.

### 3.3 Mapping zone -> relais

Chaque zone active possède une affectation matérielle :

- `enabled` : mapping actif ;
- `boardIndex` : index de carte dans la table de configuration ;
- `channelIndex` : voie de la carte, indexée à partir de 0.

Exemple :

```text
Zone 1 -> carte 0, voie 0 -> adresse 0x20, relais 1
Zone 2 -> carte 0, voie 1 -> adresse 0x20, relais 2
Zone 3 -> carte 1, voie 0 -> adresse 0x21, relais 1
```

## 4. Structures cibles

Proposition de structures persistantes :

```cpp
static constexpr uint8_t MAX_RELAY_BOARDS = 8;
static constexpr uint8_t RELAY_CHANNELS_MAX_PER_BOARD = 8;

struct CfgRelayBoard {
    bool    enabled;
    uint8_t controller;
    uint8_t i2cAddress;
    uint8_t channelCount;
    uint8_t logic;
};

struct CfgZoneRelayMapping {
    bool    enabled;
    uint8_t boardIndex;
    uint8_t channelIndex;
};
```

La configuration persistée devra contenir :

```cpp
CfgRelayBoard relayBoards[MAX_RELAY_BOARDS];
CfgZoneRelayMapping zoneRelayMap[MAX_ZONES];
```

## 5. Compatibilité avec l'existant

Le comportement par défaut doit rester strictement équivalent à l'ancien comportement.

Configuration par défaut recommandée :

```text
1 carte relais active
contrôleur = XL9535
adresse = XL9535_ADDR actuelle
capacité = nbZones
logique = relayLogic actuelle
Zone N -> carte 0, voie N
```

Ainsi, une installation existante continue de fonctionner après migration.

## 6. Règles de validation

Un mapping est valide si :

- la zone est inférieure à `nbZones` ;
- le mapping est activé ;
- `boardIndex < MAX_RELAY_BOARDS` ;
- la carte ciblée est activée ;
- `channelIndex < channelCount` ;
- l'adresse I2C est cohérente avec le contrôleur ;
- aucune autre zone active ne cible la même carte et la même voie, sauf si le partage est explicitement autorisé plus tard.

En cas de mapping invalide :

- ne pas activer de sortie physique ;
- journaliser l'erreur ;
- conserver l'état logique interne de la zone ;
- remonter un défaut relais si la commande matérielle échoue.

## 7. RelaisManager cible

`RelaisManager` doit devenir la couche d'abstraction matérielle.

API publique à conserver autant que possible :

```cpp
void setRelay(uint8_t zone, bool state);
bool getState(uint8_t zone) const;
```

Interne cible :

```text
setRelay(zone, state)
    -> récupérer mapping de zone
    -> valider carte + voie
    -> modifier le registre RAM de la carte ciblée
    -> écrire uniquement la carte concernée sur I2C
```

Le moteur d'arrosage continue d'appeler uniquement `setRelay(zone, state)`.

## 8. Migration NVS

Le schéma NVS actuel stocke une structure binaire versionnée avec CRC.

Pour intégrer la topologie relais, il faudra :

1. augmenter `CFG_NVS_SCHEMA` ;
2. ajouter les nouvelles structures dans `PersistedConfig` ;
3. prévoir une lecture compatible de l'ancien schéma ;
4. générer automatiquement la topologie par défaut à partir de l'ancien couple `nbZones` / `nbRelaisPhysical` / `relayController` / `relayLogic`.

Point important : une modification directe de la structure sans migration provoquerait un reset de configuration. Ce comportement est acceptable seulement si explicitement validé, mais la cible propre est une migration.

## 9. Interface Web cible

À terme, la page Paramétrage devra permettre :

- déclarer le nombre de cartes relais ;
- choisir pour chaque carte : contrôleur, adresse, capacité, logique ;
- afficher la capacité totale disponible ;
- affecter chaque zone à une carte et une voie ;
- bloquer les doublons carte/voie ;
- afficher les cartes absentes détectées au scan I2C.

Cette évolution Web est hors run 1.

## 10. Découpage d'implémentation

### Run 1 — Socle projet

- Créer branche dédiée.
- Créer issue de suivi.
- Documenter architecture cible.
- Identifier fichiers impactés.

### Run 2 — Configuration persistante

- Ajouter `CfgRelayBoard` et `CfgZoneRelayMapping`.
- Ajouter getters dans `ConfigManager`.
- Ajouter defaults et migration.

### Run 3 — RelaisManager multi-cartes

- Ajouter table runtime par carte.
- Initialiser chaque carte active.
- Appliquer les commandes via mapping.

### Run 4 — API et Web

- Ajouter endpoints JSON.
- Ajouter UI Paramétrage.

### Run 5 — Validation matérielle

- Tester carte unique.
- Tester cartes multiples.
- Tester adresses absentes.
- Tester conflit de mapping.

## 11. Invariants

- Le moteur d'arrosage ne connaît pas les cartes relais.
- Le planning ne connaît pas les adresses I2C.
- Une zone logique reste stable même si son affectation matérielle change.
- Toute commande matérielle invalide doit échouer en sécurité.
- Aucun changement SD/LittleFS dans ce chantier.
- Aucun changement météo/pluie/intervalle dans ce chantier.
