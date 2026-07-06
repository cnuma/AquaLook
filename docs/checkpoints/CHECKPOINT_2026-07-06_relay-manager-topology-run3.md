# AquaLook — Checkpoint évolution relais multi-cartes I2C — Run 3

Date : 2026-07-06  
Branche : `feature/relay-board-mapping`  
Issue : #2 — Évolution relais : cartes I2C, voies et mapping zones

## 1. Objectif du run

Intégrer le modèle `RelayTopology` dans `RelaisManager` sans modifier encore :

- le stockage NVS ;
- `ConfigManager` ;
- l'interface Web ;
- le moteur d'arrosage ;
- les ressources SD/LittleFS.

Le but est de remplacer le chemin interne direct :

```text
zone -> bit relais
```

par :

```text
zone -> mapping -> carte relais -> voie relais
```

tout en générant pour l'instant une topologie compatible avec l'ancien modèle.

## 2. Fichiers modifiés

### `src/RelaisManager.h`

Modifications :

- ajout de `#include "RelayTopology.h"` ;
- remplacement des registres uniques `_regP0` / `_regP1` par des registres par carte :

```cpp
uint8_t _regP0[RelayTopology::MAX_RELAY_BOARDS];
uint8_t _regP1[RelayTopology::MAX_RELAY_BOARDS];
bool _boardReady[RelayTopology::MAX_RELAY_BOARDS];
```

- ajout d'une topologie runtime :

```cpp
RelayTopology::RelayTopologyConfig _topology;
```

- ajout des helpers internes :

```cpp
void buildRuntimeTopology();
bool initBoard(uint8_t boardIndex);
bool applyBoard(uint8_t boardIndex);
bool writeReg(uint8_t addr, uint8_t reg, uint8_t val);
uint8_t readReg(uint8_t addr, uint8_t reg);
```

- suppression des helpers centrés sur un contrôleur unique :

```cpp
controller();
i2cAddress();
applyHardware();
writeReg(reg, val);
readReg(reg);
```

## 3. `src/RelaisManager.cpp`

Modifications principales :

### 3.1 Construction de topologie runtime

Ajout de :

```cpp
RelaisManager::buildRuntimeTopology()
```

Cette fonction construit une topologie compatible avec l'ancien comportement à partir de :

- `configMgr.nbZones()` ;
- `configMgr.nbRelais()` ;
- `configMgr.relayController()` ;
- `configMgr.relayLogic()`.

Le mapping obtenu est :

```text
Zone 1 -> carte 0, voie 1
Zone 2 -> carte 0, voie 2
...
```

En interne les voies restent indexées à partir de 0.

### 3.2 Initialisation multi-cartes

`initHardware()` parcourt maintenant les cartes déclarées dans la topologie runtime.

Pour chaque carte valide :

```cpp
_boardReady[b] = initBoard(b);
```

Même si une seule carte est active aujourd'hui, la structure est prête pour plusieurs cartes.

### 3.3 Commande relais par mapping

`setRelay(zone, state)` ne calcule plus directement un bit depuis l'index de zone.

Nouveau flux :

```cpp
MappingResolution mapping = RelayTopology::resolveMapping(_topology, relay, nbZ);
```

Puis :

```text
mapping.boardIndex
mapping.channelIndex
mapping.i2cAddress
mapping.controller
mapping.logic
```

sont utilisés pour écrire le bon registre de la bonne carte.

### 3.4 Logs enrichis

Les logs indiquent maintenant :

- la zone ;
- l'état ON/OFF ;
- la carte ciblée ;
- le contrôleur ;
- l'adresse I2C ;
- la voie ;
- la logique directe/inverse.

Exemple attendu :

```text
Relais: zone 1 ON -> carte 0 XL9535 0x20 voie 1 logique=directe
```

## 4. Compatibilité attendue

Pour une installation existante avec une seule carte :

```text
Avant : Zone N -> bit N
Après : Zone N -> carte 0, voie N
```

Le comportement physique attendu reste identique.

## 5. Fichiers volontairement non modifiés

- `src/ConfigManager.h`
- `src/ConfigManager.cpp`
- `src/main.cpp`
- `src/ScheduleManager.*`
- fichiers Web
- ressources SD/LittleFS

## 6. Position précise des modifications

### `src/RelaisManager.h`

- En-tête : ajout `RelayTopology.h`.
- Bloc private : remplacement de l'état matériel unique par une topologie et des registres par carte.
- Déclarations privées : ajout des helpers multi-cartes.

### `src/RelaisManager.cpp`

- `begin()` : construit la topologie, initialise les registres par carte, initialise le matériel.
- Nouvelle fonction `buildRuntimeTopology()` : génère le modèle compatible legacy.
- `initHardware()` : parcourt les cartes.
- Nouvelle fonction `initBoard()` : initialise une carte selon son contrôleur.
- `setRelay()` : résout le mapping avant d'écrire le registre.
- Nouvelle fonction `applyBoard()` : écrit seulement la carte concernée.
- `writeReg()` et `readReg()` : prennent maintenant l'adresse I2C en paramètre.

## 7. Points à vérifier à la compilation

Compilation locale recommandée :

```powershell
pio run -e ProgrammeArrosage
```

Points de vigilance :

- inclusion correcte de `RelayTopology.h` ;
- compilation de `RelayTopology.cpp` par PlatformIO via `build_src_filter = +<*>` ;
- absence de conflit de noms avec les constantes existantes ;
- logs `EventLog::log()` avec types `uint8_t`.

## 8. Test matériel recommandé après compilation

Sur la configuration actuelle carte unique :

1. boot du module ;
2. vérifier dans les logs :

```text
Relais: topologie legacy, carte0=XL9535 0x20, voies=...
Relais: carte 0 XL9535 0x20 voies=... OK
```

3. activer manuellement la zone 1 ;
4. vérifier que le relais 1 commute ;
5. activer manuellement la zone 2 ;
6. vérifier que le relais 2 commute ;
7. vérifier qu'aucun défaut `RELAY_I2C` ne reste actif.

## 9. Prochain run recommandé

Run 4 : persistance propre dans `ConfigManager`.

Objectif : rendre la topologie réellement configurable et stockée.

Travail à prévoir :

- ajouter `RelayTopologyConfig` dans `ConfigManager` ;
- incrémenter le schéma NVS ;
- créer une structure legacy de lecture de l'ancien schéma ;
- migrer automatiquement vers la nouvelle structure ;
- exposer des getters vers `RelaisManager` ;
- remplacer `buildLegacyCompatibleTopology()` par la topologie persistée si disponible.

## 10. Statut

- Code modifié : oui.
- Compilation : non lancée côté agent.
- Test matériel : non lancé.
- Risque : modéré, car `RelaisManager` est maintenant modifié.
- Retour arrière simple : revenir au commit précédent de la branche si la compilation échoue.
