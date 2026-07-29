# AquaLook Engineering Reference — Schéma détaillé du modèle d’équipements

- Version documentaire : 1.0
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Sources : `src/EquipmentModel.*`, `src/EquipmentManager.*`, `src/RelayTopology.*`, `src/main.cpp`
- Maturité : D4

## Objet

Ce document décrit les structures, relations, validations et limites du modèle d’équipements V4 actuellement compilé.

## Frontières

Le namespace `EquipmentModel` décrit les actionneurs logiques. Il ne pilote pas le matériel, ne dépend pas de `ConfigManager` et ne persiste aucune donnée. `EquipmentManager` résout les liens et exécute les électrovannes via l’adaptateur ou `RelaisManager`.

## Constantes

| Constante | Valeur / origine | Usage |
|---|---|---|
| `MAX_EQUIPMENTS` | `RelayTopology::MAX_RELAY_ASSIGNMENTS` | taille du tableau d’équipements |
| `INVALID_INDEX` | `0xFF` | absence de référence |
| `EQUIPMENT_NAME_LENGTH` | 24 | taille fixe du nom |
| `MAX_PLAN_STEPS` | 4 | nombre maximal d’étapes d’un plan |

## Types d’équipement

| Valeur | Identifiant | Nom de contrat | Rôle relais attendu |
|---:|---|---|---|
| 0 | `EQUIP_UNUSED` | `unused` | `ROLE_UNUSED` |
| 1 | `EQUIP_ZONE_VALVE` | `zone_valve` | `ROLE_ZONE_VALVE` |
| 2 | `EQUIP_PUMP` | `pump` | `ROLE_PUMP` |
| 3 | `EQUIP_AUX_CONTACT` | `aux_contact` | `ROLE_AUX` |
| 4 | `EQUIP_GREENHOUSE_VENT` | `greenhouse_vent` | `ROLE_GREENHOUSE_VENT` |
| 5 | `EQUIP_LIGHTING` | `lighting` | `ROLE_LIGHTING` |
| 6 | `EQUIP_MISTER` | `mister` | `ROLE_AUX` |
| 7 | `EQUIP_FAN` | `fan` | `ROLE_AUX` |

Tous les types non `UNUSED` sont actuellement considérés comme utilisant un relais au niveau du modèle. Cela ne signifie pas qu’ils disposent tous d’un exécuteur fonctionnel.

## Structure `EquipmentConfig`

```cpp
struct EquipmentConfig {
    bool enabled;
    uint8_t type;
    uint8_t targetIndex;
    char name[24];
    uint8_t relayAssignmentIndex;
    uint16_t startupDelayMs;
    uint16_t shutdownDelayMs;
    uint16_t minOnSec;
    uint16_t minOffSec;
};
```

Valeurs par défaut : désactivé, type `UNUSED`, cible 0, nom vide, affectation `INVALID_INDEX`, temporisations nulles.

## Structure `ZoneEquipmentLink`

```cpp
struct ZoneEquipmentLink {
    bool enabled;
    uint8_t zoneIndex;
    uint8_t valveEquipmentIndex;
    uint8_t pumpEquipmentIndex;
};
```

`pumpEquipmentIndex == INVALID_INDEX` signifie que la zone fonctionne sans pompe pilotée par AquaLook.

## Agrégat

```cpp
struct EquipmentConfigSet {
    EquipmentConfig equipments[MAX_EQUIPMENTS];
    ZoneEquipmentLink zoneLinks[MAX_ZONES];
};
```

Relation logique :

```text
ZoneEquipmentLink
  -> une zone
  -> une électrovanne obligatoire
  -> zéro ou une pompe
EquipmentConfig
  -> une affectation RelayTopology
  -> une voie physique résolue par le backend
```

## Validation d’un équipement

`validateEquipment()` exige :

1. index inférieur à `MAX_EQUIPMENTS` ;
2. équipement activé ;
3. type supporté et différent de `UNUSED` ;
4. `relayAssignmentIndex` valide ;
5. terminaison NUL du nom dans les 24 octets.

Les temporisations et minimums ON/OFF sont stockés mais ne sont pas tous appliqués par l’exécution actuelle.

## Validation d’un lien de zone

`validateZoneLink()` exige :

1. index de lien valide et lien activé ;
2. zone dans `nbZones` et `MAX_ZONES` ;
3. électrovanne valide ;
4. type `ZONE_VALVE` ;
5. `targetIndex` identique à `zoneIndex` ;
6. pompe optionnelle valide et de type `PUMP`.

## Recherche

- `findByTypeAndTarget()` retourne le premier équipement valide correspondant, sinon `-1` ;
- `findZoneLink()` retourne le premier lien valide d’une zone, sinon `-1` ;
- les entrées invalides sont ignorées pendant les recherches.

## Résolution par `EquipmentManager`

`EquipmentManager` produit :

- `EquipmentResolution` : équipement, affectation relais et mapping physique ;
- `ZoneDependencyResolution` : lien, électrovanne et pompe optionnelle ;
- `ZoneExecutionPlan` : zone, besoin pompe et jusqu’à quatre étapes.

Actions de plan : valve ON/OFF, pompe ON/OFF, attente ou aucune action.

## Résultats d’action

Les erreurs distinguent notamment : manager non initialisé, zone invalide, lien absent, équipement absent ou invalide, mapping absent, rôle relais incorrect, exécuteur absent et échec d’exécution.

## Capacité réellement exécutable

Le code précise que seules les électrovannes de zones sont exécutables. Les dépendances pompe sont résolues, planifiées et observables en dry-run, mais aucune action physique pompe n’est exécutée.

La présence d’un type dans l’énumération n’est donc pas une preuve de prise en charge Runtime.

## Invariants

- `INV-EQM-001` : le modèle ne pilote jamais directement le matériel.
- `INV-EQM-002` : une zone valide possède exactement une électrovanne valide.
- `INV-EQM-003` : une pompe est facultative et explicitement référencée.
- `INV-EQM-004` : type d’équipement et rôle relais doivent correspondre.
- `INV-EQM-005` : un index invalide conduit à un refus, jamais à un mapping arbitraire.
- `INV-EQM-006` : une action planifiée mais non exécutée reste qualifiée dry-run.

## Tests

- `clear()` remet l’ensemble à l’état désactivé ;
- validation des bornes d’index ;
- rejet d’un nom non terminé ;
- rejet d’une valve dont `targetIndex` diffère de la zone ;
- lien sans pompe ;
- lien avec pompe valide et invalide ;
- résolution de rôle relais ;
- plans start/stop ;
- dry-run pompe ;
- repli lorsque l’adaptateur ou l’exécuteur est absent.

Environnement associé :

```powershell
pio run -e test_execution_engine
pio run -e ProgrammeArrosage_v4
```

## Écarts ouverts

- définir la persistance et le versionnement du modèle ;
- appliquer ou supprimer les champs `minOnSec` et `minOffSec` s’ils restent inutilisés ;
- implémenter explicitement la commande pompe avant de la déclarer active ;
- stabiliser les contrats JSON/API de configuration ;
- définir la cardinalité attendue pour les équipements non liés aux zones ;
- ajouter des tests automatisés exhaustifs de validation.

## Références

- `src/EquipmentModel.h` et `.cpp` ;
- `src/EquipmentManager.h` et `.cpp` ;
- `src/RelayTopology.h` et `.cpp` ;
- `src/EquipmentOutputRuntimeAdapter.*` ;
- `16_V4_EQUIPMENT_MODEL_AND_WEATHER.md` ;
- `30_TEST_AND_ANTI_REGRESSION_MATRIX.md`.

## Historique

### 1.0

Création du schéma D4 détaillé du modèle d’équipements.