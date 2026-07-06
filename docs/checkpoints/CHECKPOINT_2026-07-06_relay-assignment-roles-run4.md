# AquaLook — Checkpoint relais généralisés par rôle — Run 4

Date : 2026-07-06  
Branche : `feature/relay-board-mapping`  
Issue : #2 — Évolution relais : cartes I2C, voies et mapping zones

## 1. Déclencheur

Pendant le chantier relais, un point de conception important a été identifié :

AquaLook ne doit pas supposer que tous les relais physiques sont des électrovannes de zones d'arrosage.

Exemples à couvrir :

- relais de zone d'arrosage ;
- relais contact sec pour démarrage de pompe ;
- relais auxiliaire ;
- relais de volet ou aération de serre ;
- relais d'éclairage ;
- autre équipement futur.

## 2. Décision d'architecture

Remplacer conceptuellement :

```text
ZoneRelayMapping
```

par :

```text
RelayAssignment
```

Une zone d'arrosage devient un cas particulier :

```text
RelayAssignment(role = ROLE_ZONE_VALVE, targetIndex = zoneIndex)
```

La couche relais ne doit plus être pensée comme uniquement liée aux zones.

## 3. Fichiers modifiés

### `src/RelayTopology.h`

Modifications :

- ajout de rôles logiques :

```cpp
ROLE_UNUSED
ROLE_ZONE_VALVE
ROLE_PUMP
ROLE_AUX
ROLE_GREENHOUSE_VENT
ROLE_LIGHTING
```

- remplacement de la structure centrale par :

```cpp
struct RelayAssignment {
    bool    enabled;
    uint8_t role;
    uint8_t targetIndex;
    uint8_t boardIndex;
    uint8_t channelIndex;
};
```

- conservation d'un alias de compatibilité conceptuelle :

```cpp
using ZoneRelayMapping = RelayAssignment;
```

- remplacement dans la topologie :

```cpp
RelayAssignment assignments[MAX_RELAY_ASSIGNMENTS];
```

- ajout des fonctions :

```cpp
const char* roleName(uint8_t role);
bool isSupportedRole(uint8_t role);
bool validateAssignment(...);
MappingResolution resolveAssignment(...);
MappingResolution resolveZoneValve(...);
bool hasDuplicateAssignments(...);
```

- conservation des fonctions de compatibilité :

```cpp
validateMapping(...)
resolveMapping(...)
hasDuplicateMappings(...)
```

Ces wrappers appellent désormais la logique générique.

### `src/RelayTopology.cpp`

Modifications :

- ajout de `roleName()` ;
- ajout de `isSupportedRole()` ;
- `clear()` initialise désormais `assignments[]` ;
- `buildLegacyCompatibleTopology()` crée des assignments de rôle `ROLE_ZONE_VALVE` ;
- ajout de `validateAssignment()` ;
- ajout de `resolveAssignment()` ;
- ajout de `resolveZoneValve()` ;
- ajout de `hasDuplicateAssignments()` ;
- les anciennes fonctions `validateMapping()`, `resolveMapping()` et `hasDuplicateMappings()` restent présentes comme wrappers.

## 4. Compatibilité avec `RelaisManager`

`RelaisManager` continue d'appeler :

```cpp
RelayTopology::resolveMapping(_topology, relay, nbZ)
```

Cette fonction reste disponible et route maintenant vers :

```cpp
resolveZoneValve(topology, zone, nbZones)
```

Le comportement actuel reste donc :

```text
Zone N -> assignment ROLE_ZONE_VALVE targetIndex=N -> carte 0 -> voie N
```

## 5. Nouvelle cible fonctionnelle

Le modèle permet maintenant de représenter :

```text
Zone 1        -> carte 0 voie 0, role=ROLE_ZONE_VALVE, targetIndex=0
Zone 2        -> carte 0 voie 1, role=ROLE_ZONE_VALVE, targetIndex=1
Pompe forage  -> carte 1 voie 0, role=ROLE_PUMP, targetIndex=0
Volet serre 1 -> carte 1 voie 1, role=ROLE_GREENHOUSE_VENT, targetIndex=0
Eclairage     -> carte 1 voie 2, role=ROLE_LIGHTING, targetIndex=0
```

## 6. Implications futures

### Pompe

À terme, le moteur d'arrosage devra pouvoir faire :

```text
avant ouverture vanne : activer pompe si nécessaire
attendre montée pression éventuelle
ouvrir électrovanne de zone
fermer électrovanne
si plus aucune zone active : couper pompe avec temporisation éventuelle
```

### Serre

Les équipements de serre ne doivent pas dépendre du moteur d'arrosage.

Ils devront probablement être gérés par un futur manager dédié :

```text
GreenhouseManager / AuxOutputManager
```

### Interface Web

La page de paramétrage devra afficher non seulement les zones, mais les rôles d'usage des relais :

- zone d'arrosage ;
- pompe ;
- auxiliaire ;
- volet/aération serre ;
- éclairage.

## 7. Position précise des modifications

### `src/RelayTopology.h`

- En haut du namespace : ajout des constantes de rôles.
- Remplacement du modèle `ZoneRelayMapping` par `RelayAssignment`.
- Ajout de l'alias `using ZoneRelayMapping = RelayAssignment`.
- `RelayTopologyConfig` contient maintenant `assignments[]`.
- `MappingResolution` contient maintenant `role` et `targetIndex`.
- Ajout des prototypes génériques et wrappers de compatibilité.

### `src/RelayTopology.cpp`

- Ajout de `roleName()` après `controllerName()`.
- Ajout de `isSupportedRole()` après `isSupportedChannelCount()`.
- `clear()` nettoie `assignments[]`.
- `buildLegacyCompatibleTopology()` affecte `ROLE_ZONE_VALVE`.
- Ajout des fonctions génériques de validation/résolution.
- Wrappers de compatibilité conservés en fin de fichier.

## 8. Fichiers volontairement non modifiés

- `src/RelaisManager.h`
- `src/RelaisManager.cpp`
- `src/ConfigManager.h`
- `src/ConfigManager.cpp`
- moteur arrosage
- interface Web
- ressources SD/LittleFS

## 9. Compilation recommandée

Commande recommandée pour test réel sur carte :

```powershell
git fetch origin
git switch feature/relay-board-mapping
git pull --ff-only
pio run -e ProgrammeArrosage -t upload ; pio device monitor -e ProgrammeArrosage
```

## 10. Risques

Risque principal : compilation à vérifier, car `RelayTopologyConfig` a changé de champ interne `mappings[]` vers `assignments[]`.

Le risque runtime reste contenu car `RelaisManager` utilise les wrappers de compatibilité.

## 11. Prochain run recommandé

Avant d'aller plus loin côté NVS, compiler ce run est utile.

Ensuite :

1. corriger si compilation KO ;
2. intégrer la persistance dans `ConfigManager` ;
3. introduire le rôle pompe dans le moteur d'arrosage ;
4. créer une API Web de configuration des assignments.
