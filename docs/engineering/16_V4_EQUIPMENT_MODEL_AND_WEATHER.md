# AquaLook Engineering Reference — Backend V4, modèle d’équipements et météo

- Version documentaire : 1.1
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Source de code : `src/EquipmentManager.*`, `src/EquipmentModel.*`, `src/EquipmentOutputRuntimeAdapter.*`, `src/WeatherManager.*`, `src/main.cpp`, `platformio.ini`
- Composants : backend V4, fallback legacy, modèle d’équipements, pompe shadow, OpenWeatherMap
- Maturité : D4

## Objet

Ce document décrit les frontières réellement implémentées entre le modèle d’équipements, la chaîne de sortie et la collecte météo. Il distingue les sorties exécutables des fonctions encore limitées au plan ou au dry-run.

## Sélection du backend

Les profils de compilation utilisent les macros `AQUALOOK_RELAY_BACKEND_LEGACY` et `AQUALOOK_RELAY_BACKEND_V4`. Le Runtime conserve un chemin de repli vers l’adaptateur de sorties lorsque `EquipmentManager::startZone()` ou `stopZone()` échoue.

Chaîne principale :

```text
ScheduleManager
  -> callback onRelayRequest(zone, state)
  -> EquipmentManager::startZone/stopZone
  -> EquipmentOutputRuntimeAdapter
  -> backend sélectionné
  -> RelaisManager / topologie I2C
```

## API confirmée d’EquipmentManager

```cpp
void begin(const EquipmentConfigSet* model,
           const RelayTopologyConfig* topology,
           uint8_t nbZones,
           RelaisManager* relayExecutor = nullptr);
void setRelayExecutor(RelaisManager* executor);
void setOutputAdapter(EquipmentOutputRuntimeAdapter* adapter);
bool isInitialized() const;
ZoneResolution resolveZone(uint8_t zone) const;
ZoneDependencyResolution resolveZoneDependencies(uint8_t zone) const;
ZoneExecutionPlan buildZoneStartPlan(uint8_t zone) const;
ZoneExecutionPlan buildZoneStopPlan(uint8_t zone) const;
ActionResult dryRunZoneStartPlan(uint8_t zone) const;
ActionResult dryRunZoneStopPlan(uint8_t zone) const;
ActionResult startZone(uint8_t zone);
ActionResult stopZone(uint8_t zone);
```

## Résultats et plans

`ActionResult` distingue notamment : non initialisé, zone invalide, lien ou équipement absent, mapping relais absent, rôle incompatible, exécuteur absent et échec d’exécution.

Un `ZoneExecutionPlan` contient au maximum quatre étapes. Actions possibles :

- `VALVE_ON` ;
- `VALVE_OFF` ;
- `PUMP_ON` ;
- `PUMP_OFF` ;
- `WAIT`.

Le plan indique la zone, la nécessité d’une pompe et les délais associés.

## Limite actuelle du modèle

Le commentaire contractuel de `EquipmentManager` est explicite : seules les électrovannes de zones sont exécutables. Les dépendances pompe sont résolues, planifiées et observables en dry-run, mais aucune action pompe n’est actuellement exécutée.

La pompe shadow est donc une capacité d’analyse et de préparation, pas une commande matérielle active. Elle ne doit pas être présentée comme opérationnelle tant que `PUMP_ON/PUMP_OFF` ne sont pas reliés à un exécuteur validé.

## Météo : API et données

`WeatherManager` expose :

```cpp
void begin(ConfigManager* config = nullptr);
void update(bool wifiConnected);
bool isRainExpected() const;
float getRainMm() const;
float getTempC() const;
bool hasFetched() const;
String getStatusStr() const;
ForecastDay getForecastDay(uint8_t offset) const;
```

Chaque `ForecastDay` contient pluie, températures min/max et ressentie, vent, rafales, direction, probabilité de pluie, humidité, nébulosité, pression moyenne, description, icône et validité pour cinq jours.

## Exécution météo non bloquante

`update()` reste dans la boucle principale et :

1. traite un changement de configuration via `EventBus::configDirty` ;
2. applique un résultat déjà produit ;
3. vérifie le Wi-Fi et l’échéance ;
4. copie la configuration utile ;
5. crée une tâche FreeRTOS `weather-fetch`.

La tâche utilise une pile de `12288` octets et une priorité `1`. La configuration n’est pas lue depuis la tâche : elle est copiée dans `FetchRequest` avant lancement. Les échanges entre tâche et Runtime sont protégés par `g_weatherMux`.

## Source distante réelle

Endpoint utilisé :

```text
http://api.openweathermap.org/data/2.5/forecast
```

Paramètres selon la configuration : coordonnées ou `ville,pays`, `appid`, `units`, `cnt=40`.

Le transport est actuellement HTTP non chiffré. La clé API apparaît dans l’URL construite en mémoire. Ce point constitue un écart de cybersécurité.

Timeout HTTP : `8000 ms`. En cas d’échec, la prochaine tentative est préparée après `60000 ms`. Après une absence de clé, le prochain contrôle utilise `OWM_CHECK_INTERVAL_MS`.

## Traitement JSON

La réponse est désérialisée directement depuis le flux HTTP avec un filtre ArduinoJson limité aux champs utilisés. Cette stratégie évite de dupliquer la réponse complète en RAM.

Les 40 créneaux de trois heures sont regroupés sur cinq jours. L’algorithme utilise le fuseau renvoyé par OWM lorsqu’une époque valide est disponible ; sinon il replie les entrées par groupes de huit.

La description et l’icône retenues correspondent au créneau le plus proche de midi local. Les vents sont convertis de m/s en km/h. La probabilité `pop` est convertie en pourcentage.

## Influence météo

`WeatherManager` produit des données et un indicateur `rainExpected`. Il ne pilote jamais directement les relais. Les décisions d’annulation ou de report appartiennent au Scheduler et à sa configuration de pluie.

La pluie utilisée pour `rainExpected` est comparée au seuil copié depuis la configuration de la zone 0. Ce choix doit être considéré comme une limite actuelle lorsque plusieurs zones possèdent des seuils différents.

## Invariants

- `INV-V4-001` : le fallback legacy reste disponible tant que sa suppression n’est pas explicitement validée.
- `INV-V4-002` : seules les actions réellement reliées à un exécuteur sont déclarées exécutables.
- `INV-V4-003` : une résolution ou un mapping invalide conduit à un résultat d’erreur, jamais à une sortie arbitraire.
- `INV-V4-004` : les plans pompe restent passifs tant que leur exécution n’est pas câblée et testée.
- `INV-WEA-001` : l’absence de réseau, de clé ou de réponse météo ne bloque pas le Runtime.
- `INV-WEA-002` : HTTP et désérialisation s’exécutent hors de la boucle critique.
- `INV-WEA-003` : la configuration est copiée avant lancement de la tâche météo.
- `INV-WEA-004` : la météo ne commande pas directement les sorties.

## Validation

- compilation et boot legacy puis V4 ;
- résolution de zone valide et invalide ;
- mapping absent et exécuteur absent ;
- construction et dry-run des plans avec et sans pompe ;
- confirmation qu’aucune sortie pompe n’est activée ;
- météo sans Wi-Fi et sans clé ;
- timeout et erreur HTTP ;
- réponse JSON filtrée ;
- cinq jours et fuseau local ;
- reconfiguration OWM ;
- absence de blocage de la boucle principale.

## Écarts ouverts

- remplacer l’appel OWM HTTP par HTTPS et protéger la clé API ;
- décider si le seuil pluie doit être global ou calculé par zone ;
- versionner le schéma complet `EquipmentModel` et ses identifiants ;
- câbler ou supprimer les étapes pompe passives ;
- archiver des tests automatiques sur les plans et les résultats d’erreur ;
- préciser la politique de conservation des dernières données valides après échec ;
- vérifier le coût de la tâche et du buffer JSON sur les variantes matérielles sans PSRAM.

## Références

- `src/EquipmentManager.h` et `.cpp` ;
- `src/EquipmentModel.h` et `.cpp` ;
- `src/EquipmentOutputRuntimeAdapter.h` et `.cpp` ;
- `src/WeatherManager.h` et `.cpp` ;
- `src/main.cpp` ;
- `platformio.ini` ;
- `docs/checkpoints/CHECKPOINT_2026-07-13_MAIN_STEP6_CLOSED.md`.

## Historique

### 1.1

Consolidation D4 des APIs, plans d’exécution, limites de la pompe shadow, tâche météo, endpoint, timeout et traitement JSON.