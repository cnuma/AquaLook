# AquaLook — Checkpoint évolution relais multi-cartes I2C — Run 2

Date : 2026-07-06  
Branche : `feature/relay-board-mapping`  
Issue : #2 — Évolution relais : cartes I2C, voies et mapping zones

## 1. Objectif du run

Ajouter un premier socle de code pour représenter la topologie relais multi-cartes sans modifier encore :

- le moteur d'arrosage ;
- `ConfigManager` ;
- le stockage NVS ;
- `RelaisManager` ;
- l'interface Web.

Ce choix évite une régression forte : le stockage actuel est binaire, versionné et protégé par CRC. Modifier directement `PersistedConfig` sans migration ferait perdre la configuration existante.

## 2. Fichiers ajoutés

### `src/RelayTopology.h`

Ajoute le modèle de données cible :

```cpp
namespace RelayTopology {

static constexpr uint8_t MAX_RELAY_BOARDS = 8;
static constexpr uint8_t MAX_CHANNELS_PER_BOARD = 8;

struct RelayBoardConfig {
    bool    enabled;
    uint8_t controller;
    uint8_t i2cAddress;
    uint8_t channelCount;
    uint8_t logic;
};

struct ZoneRelayMapping {
    bool    enabled;
    uint8_t boardIndex;
    uint8_t channelIndex;
};

struct RelayTopologyConfig {
    RelayBoardConfig boards[MAX_RELAY_BOARDS];
    ZoneRelayMapping mappings[MAX_ZONES];
};

struct MappingResolution {
    bool valid;
    uint8_t boardIndex;
    uint8_t channelIndex;
    uint8_t controller;
    uint8_t i2cAddress;
    uint8_t logic;
};

}
```

### `src/RelayTopology.cpp`

Ajoute les helpers :

- `controllerName()` ;
- `isSupportedController()` ;
- `isSupportedChannelCount()` ;
- `normalizeChannelCount()` ;
- `defaultAddressForController()` ;
- `clear()` ;
- `buildLegacyCompatibleTopology()` ;
- `validateBoard()` ;
- `validateMapping()` ;
- `resolveMapping()` ;
- `totalEnabledChannels()` ;
- `hasDuplicateMappings()`.

## 3. Position précise des modifications

### Ajout 1

Fichier : `src/RelayTopology.h`  
Position : nouveau fichier complet  
Rôle : modèle public de topologie relais.

### Ajout 2

Fichier : `src/RelayTopology.cpp`  
Position : nouveau fichier complet  
Rôle : validation, normalisation et résolution de mapping.

### Ajout 3

Fichier : `docs/checkpoints/CHECKPOINT_2026-07-06_relay-topology-code-run2.md`  
Position : nouveau fichier complet  
Rôle : checkpoint de reprise.

## 4. Compatibilité avec l'existant

Aucun appel existant n'est modifié dans ce run.

Le comportement runtime actuel reste donc :

```text
ScheduleManager -> RelaisManager::setRelay(zone, state) -> zone = bit relais
```

Le nouveau module permet déjà de construire une topologie équivalente :

```text
Zone 1 -> carte 0, voie 0
Zone 2 -> carte 0, voie 1
...
```

via :

```cpp
RelayTopology::buildLegacyCompatibleTopology(
    topology,
    nbZones,
    nbRelaisPhysical,
    controller,
    logic
);
```

## 5. Règles de sécurité introduites

Le module refuse un mapping si :

- la zone est hors bornes ;
- le mapping est désactivé ;
- la carte ciblée est hors bornes ;
- la carte est désactivée ;
- le contrôleur est inconnu ;
- la capacité de carte n'est pas 1, 2, 4 ou 8 ;
- l'adresse I2C n'est pas dans `0x20..0x27` ;
- la voie demandée dépasse la capacité de la carte.

Une fonction dédiée détecte également les doublons :

```cpp
RelayTopology::hasDuplicateMappings(topology, nbZones)
```

## 6. Fichiers volontairement non modifiés

- `src/ConfigManager.h`
- `src/ConfigManager.cpp`
- `src/RelaisManager.h`
- `src/RelaisManager.cpp`
- `src/main.cpp`
- fichiers Web
- ressources SD/LittleFS
- moteur planning/arrosage

## 7. Compilation

Compilation non lancée côté agent.

Risque de compilation identifié : faible, car le module est autonome et n'altère aucune API existante.

Point à vérifier localement :

```powershell
pio run -e ProgrammeArrosage
```

## 8. Prochain run recommandé

Run 3 : intégration contrôlée dans `ConfigManager`.

Objectif : rendre cette topologie persistante sans reset brutal de configuration.

Options possibles :

### Option A — Migration propre recommandée

- incrémenter `CFG_NVS_SCHEMA` ;
- conserver un lecteur de l'ancien `PersistedConfig` ;
- migrer automatiquement vers la nouvelle structure ;
- générer `RelayTopologyConfig` à partir de l'ancien couple :
  - `nbZones` ;
  - `nbRelaisPhysical` ;
  - `relayController` ;
  - `relayLogic`.

### Option B — Reset accepté

- modifier directement `PersistedConfig` ;
- laisser l'ancien bloc NVS devenir invalide ;
- repartir sur une config par défaut.

Option B est plus simple mais moins propre. Elle n'est pas recommandée pour une carte déjà configurée.

## 9. Invariant projet confirmé

Le moteur d'arrosage ne doit pas connaître les cartes relais. Il continuera à commander des zones logiques. La traduction vers les cartes et voies doit rester dans la couche relais/configuration.
