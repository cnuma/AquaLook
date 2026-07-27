# AquaLook Engineering Reference — Relais et commande des équipements

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/RelaisManager.h`, `src/RelaisManager.cpp`, `src/RelayTopology.*`, `src/main.cpp`
- Composants : `RelaisManager`, `RelayTopology`, XL9535, MCP23017, backend legacy/V4
- Maturité : D4

## Mission

La chaîne relais transforme une demande logique de zone ou d’équipement en écriture physique I²C validée. Le Scheduler ne connaît ni le contrôleur, ni l’adresse, ni la logique électrique.

## API publique confirmée

```cpp
void begin(ConfigManager* config = nullptr);
void update();
void setXl9535SharedOutputState(Xl9535SharedOutputState* state);
void mirrorZoneState(uint8_t zone, bool state, uint32_t nowMs);
void setRelay(uint8_t zone, bool state);
bool getState(uint8_t zone) const;
bool setAssignment(uint8_t assignmentIndex, bool state);
bool getAssignmentState(uint8_t assignmentIndex) const;
const RelayTopologyConfig& topology() const;
```

`setRelay()` conserve l’API historique par index de zone. `setAssignment()` applique une affectation matérielle générique validée par `RelayTopology::resolveAssignment()`.

## Construction de la topologie

`RelaisManager::begin()` appelle `buildRuntimeTopology()`. Cette fonction lit dans `ConfigManager` :

- `nbZones()` ;
- `nbRelais()` ;
- `relayController()` ;
- `relayLogic()`.

Elle appelle ensuite `RelayTopology::buildLegacyCompatibleTopology()`. Les doublons de mapping sont détectés par `RelayTopology::hasDuplicateMappings()`.

## Contrôleurs pris en charge

| Contrôleur | Registres utilisés | Adresse par défaut documentée |
|---|---|---|
| XL9535 | sorties `0x02/0x03`, configuration `0x06/0x07` | fournie par `RelayTopology` |
| MCP23017 | `IODIRA/B` `0x00/0x01`, `OLATA/B` `0x14/0x15` | `0x20` |

Le choix réel est issu de la configuration et de la topologie. Une adresse générique ne doit pas être supposée sans lecture de la topologie active.

## Initialisation sûre

Au démarrage :

1. les états logiciels de zones et d’affectations sont remis à `false` ;
2. les registres miroirs sont initialisés à `0x00` en logique directe ou `0xFF` en logique inversée ;
3. l’état partagé XL9535 est amorcé lorsqu’il est présent ;
4. chaque carte valide est initialisée ;
5. `FaultManager::RELAY_I2C` reflète le résultat matériel.

Les sorties sont initialisées avant `ScheduleManager::begin()` dans `src/main.cpp`.

## Chaîne d’appel réelle

```text
ScheduleManager
  -> onRelayRequest(zone, state)
  -> EquipmentManager::startZone/stopZone
  -> EquipmentOutputRuntimeAdapter
  -> backend V4 si actif, sinon RelaisManagerBackend
  -> RelaisManager::setRelay ou setAssignment
  -> applyBoard
  -> writeReg I2C
```

En cas d’échec du modèle d’équipements, `onRelayRequest()` utilise `outputAdapter.setZoneValve()` comme repli. Le profil de compilation sélectionne le backend par `AQUALOOK_RELAY_BACKEND_LEGACY` et `AQUALOOK_RELAY_BACKEND_V4`.

## Application d’une affectation

`setAssignment()` :

1. résout et valide le mapping ;
2. refuse la commande si la carte n’est pas prête ;
3. convertit l’état logique en niveau physique selon `LOGIC_DIRECT` ou `LOGIC_INVERTED` ;
4. met à jour l’état partagé XL9535 ou le registre miroir local ;
5. écrit la carte avec `applyBoard()` ;
6. met à jour `FaultManager`, l’état d’affectation et l’EventLog.

Une erreur positionne `EventBus::displayDirty`. Une transition réussie ne force pas un redraw complet.

## Sécurité de durée maximale

`RelaisManager::update()` compare chaque zone active à `maxWateringMs()`. Lorsque la durée est dépassée, il journalise l’incident puis appelle `setRelay(zone, false)`.

## Invariants

- `INV-REL-001` : le Scheduler ne pilote jamais directement le matériel.
- `INV-REL-002` : une affectation invalide ou une carte absente conduit à un refus, jamais à une voie arbitraire.
- `INV-REL-003` : la logique directe/inverse est appliquée avant écriture physique.
- `INV-REL-004` : la sécurité de durée maximale reste active dans `update()`.
- `INV-REL-005` : les sorties sont placées dans leur état sûr avant activation du Scheduler.
- `INV-REL-006` : toute modification de contrôleur, adresse, mapping ou logique exige une validation matérielle.

## Validation

```text
pio run -e test_relais
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
```

Tests matériels : scan I²C, une zone, durée courte, correspondance zone/voie, logique directe/inverse, arrêt de sécurité, carte absente et redémarrage sûr.

## Références

- `src/RelaisManager.h` ;
- `src/RelaisManager.cpp` ;
- `src/RelayTopology.h` et `.cpp` ;
- `src/main.cpp` ;
- `platformio.ini` ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_STEP6_RUN6-26.md`.

## Historique

### 1.1

Consolidation D4 de l’API, de la topologie, des contrôleurs, de la logique électrique et de la chaîne d’appel physique.