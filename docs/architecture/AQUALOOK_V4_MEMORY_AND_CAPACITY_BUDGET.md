# AquaLook V4 — Budget mémoire et capacités

**Phase :** V4 Phase 1 — Run 1.1  
**Date :** 7 juillet 2026  
**Base inspectée :** `1ba0256fafab0242b254241ac9957f0a21e5f2c9`

## 1. Objet

Ce document établit un premier budget pour éviter que le modèle V4 ne duplique excessivement les données existantes.

Les mesures ci-dessous sont calculées à partir des structures courantes et d’un alignement 32 bits comparable à celui de l’ESP32. Elles devront être confirmées par compilation PlatformIO avec `sizeof()` dès que l’environnement local sera disponible.

## 2. Tailles structurelles actuelles estimées

| Structure | Taille estimée |
|---|---:|
| `CfgSlot` / `TimeSlot` | 6 octets |
| `CfgDaySchedule` | 30 octets |
| `CfgRain` / `RainConfig` | 8 octets |
| `CfgZone` | 276 octets |
| `ZoneSchedule` | 256 octets |
| `ActiveSlot` | 16 octets |

## 3. Coût des tableaux de zones

### Configuration

```text
16 × 276 = 4 416 octets
```

### Planning runtime

```text
16 × 256 = 4 096 octets
```

### États actifs

```text
16 × 16 = 256 octets
```

### Sous-total minimal

```text
4 416 + 4 096 + 256 = 8 768 octets
```

Ce sous-total n’inclut pas :

- les 16 objets `String` de raisons ;
- les autres champs de `ConfigManager` ;
- la topologie relais ;
- les buffers Web, météo et affichage ;
- l’allocation temporaire du blob NVS ;
- les surcoûts des objets C++.

## 4. Risque principal

Créer un troisième tableau V4 contenant planning, nom, pluie et état pour 16 zones ajouterait facilement 4 à 8 Kio supplémentaires.

Cette approche est interdite.

Le modèle V4 doit représenter uniquement les nouvelles identités, relations et métadonnées nécessaires, puis référencer temporairement les données historiques via un adaptateur.

## 5. Budget fixe initial V4

Objectif :

```text
RAM fixe du domaine V4 initial <= 12 Kio
```

Cible recommandée pour la Phase 1 :

```text
identités et registres       <= 2 Kio
équipements et capteurs      <= 3 Kio
intentions et exécutions     <= 2 Kio
dépendances                  <= 1 Kio
inventaire matériel compact  <= 3 Kio
marge                        >= 1 Kio
```

Ce budget exclut :

- copies historiques déjà existantes ;
- buffers réseau ;
- historique long ;
- ressources Web ;
- descripteurs constants placés en flash.

## 6. Capacités retenues

```text
MAX_ZONES_V4            = 16
MAX_EQUIPMENTS_V4       = 32
MAX_SENSORS_V4          = 32
MAX_AUTOMATIONS_V4      = 32
MAX_DEPENDENCIES_V4     = 64
MAX_ACTIVE_EXECUTIONS   = 16
MAX_HARDWARE_BOARDS     = 8
MAX_PORTS_PER_BOARD     = 16
MAX_PORT_BINDINGS       = 64
```

## 7. Taille cible des futures entrées

Ces valeurs sont des plafonds de conception, pas encore des structures figées.

| Élément | Taille cible par entrée | Coût maximal visé |
|---|---:|---:|
| registre d’équipement | 24 à 32 octets | 1 024 octets |
| registre de capteur | 20 à 24 octets | 768 octets |
| dépendance | 8 octets | 512 octets |
| exécution active | 24 à 32 octets | 512 octets |
| carte installée | 24 à 32 octets | 256 octets |
| binding de port | 8 à 12 octets | 768 octets |

Les ports ne doivent pas nécessairement être matérialisés par 128 structures lourdes. Les ports fixes peuvent être décrits par un descripteur de modèle partagé en flash.

## 8. Règles de réduction mémoire

1. identifiants sur 16 bits ;
2. index runtime sur 8 bits ;
3. enums et masques sur 8 ou 16 bits ;
4. noms dans des tables séparées ou buffers bornés ;
5. aucun `String` dans les registres du domaine ;
6. aucune allocation dynamique dans les chemins réguliers ;
7. descripteurs de modèles de cartes constants en flash ;
8. relations par identifiants, pas par pointeurs persistants ;
9. pas de copie des 40 créneaux par zone dans le domaine V4 initial ;
10. mesure `sizeof()` obligatoire à chaque ajout de structure.

## 9. Mesure à intégrer ultérieurement

Un outil ou environnement de diagnostic devra afficher à la compilation :

```cpp
sizeof(CfgZone)
sizeof(ZoneSchedule)
sizeof(PersistedConfig)
sizeof(Equipment)
sizeof(Sensor)
sizeof(Execution)
sizeof(HardwareBoard)
sizeof(PortBinding)
```

La compilation devra également relever :

- RAM globale utilisée ;
- flash utilisée ;
- plus grande allocation dynamique ;
- heap libre après boot ;
- heap minimum observé.

## 10. Conclusion

La V4 peut rester très largement dans les capacités de l’ESP32 si elle évite de recopier la configuration historique et si les modèles sont compacts.

La contrainte structurante n’est pas le nombre théorique de ports physiques, mais la quantité de métadonnées réservée pour chaque objet et relation.
