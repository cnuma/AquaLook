# AquaLook V4 — Budget mémoire et capacité dynamique

**Phase :** V4 Phase 1 — Run 1.1 corrigé  
**Date :** 7 juillet 2026  
**Base inspectée :** `1ba0256fafab0242b254241ac9957f0a21e5f2c9`

## 1. Objet

Ce document établit le budget mémoire de la V4 sans imposer une configuration fonctionnelle figée.

La capacité réelle est construite à partir des cartes, ports, équipements, capteurs, automatismes, relations et bindings réellement déclarés.

Le firmware contrôle un budget mémoire, pas un nombre commercial ou fonctionnel arbitraire d’objets.

## 2. Tailles structurelles actuelles estimées

Les estimations reposent sur un alignement 32 bits comparable à l’ESP32 et devront être confirmées par compilation PlatformIO.

| Structure | Taille estimée |
|---|---:|
| `CfgSlot` / `TimeSlot` | 6 octets |
| `CfgDaySchedule` | 30 octets |
| `CfgRain` / `RainConfig` | 8 octets |
| `CfgZone` | 276 octets |
| `ZoneSchedule` | 256 octets |
| `ActiveSlot` | 16 octets |

## 3. Coût actuel des tableaux de zones

```text
16 × CfgZone      = 4 416 octets
16 × ZoneSchedule = 4 096 octets
16 × ActiveSlot   =   256 octets
Sous-total        = 8 768 octets
```

Ce total n’inclut pas :

- les 16 objets `String` de raisons ;
- les autres champs des managers ;
- la topologie relais ;
- les buffers Web, météo et affichage ;
- le blob NVS temporaire ;
- les surcoûts des bibliothèques.

## 4. Risque principal

Une troisième copie complète des zones et plannings ajouterait plusieurs kilo-octets inutiles.

Le domaine V4 initial doit uniquement ajouter les identités, relations et métadonnées qui n’existent pas dans le modèle historique.

## 5. Principe retenu

La mémoire de configuration suit l’option C : allocation proportionnelle au contenu dans une arène bornée.

```text
Déclaration
-> calcul des tailles
-> construction dans CandidateConfigurationArena
-> validation
-> activation atomique comme ActiveConfiguration
```

Une installation composée de :

```text
1 carte 4 sorties
1 carte 2 sorties
1 carte 4 entrées
```

construit seulement :

```text
3 instances de cartes
10 ports exposés ou décrits par références compactes
les bindings réellement présents
les équipements et capteurs réellement déclarés
```

Elle ne réserve pas automatiquement huit cartes, seize ports par carte ou soixante-quatre bindings.

## 6. Budgets mémoire à définir

Les valeurs exactes seront mesurées avant intégration runtime. Les catégories sont néanmoins fixées :

```text
CONFIGURATION_ARENA_BYTES
CANDIDATE_CONFIGURATION_ARENA_BYTES
RUNTIME_EXECUTION_ARENA_BYTES
CONFIGURATION_INPUT_BYTES
```

### Budget de configuration active

Contient :

- registres d’identités ;
- cartes installées ;
- références vers les descripteurs de modèles ;
- bindings ;
- équipements ;
- capteurs ;
- automatismes ;
- dépendances ;
- chaînes ou paramètres compacts nécessaires.

### Budget candidat

Permet de construire une configuration sans altérer l’active.

La stratégie finale pourra utiliser :

- deux arènes simultanées en RAM ;
- une arène active et une candidate temporaire en PSRAM si disponible ;
- une construction candidate dans un stockage intermédiaire avec validation progressive ;
- ou un basculement contrôlé après sérialisation validée.

Cette décision détaillée appartient à la Phase 7.

### Budget runtime

Séparé de la configuration, il couvre :

- intentions ;
- exécutions actives ;
- états d’orchestration ;
- résultats temporaires ;
- files bornées.

## 7. Estimation avant activation

`ConfigurationBuilder` doit calculer au minimum :

```text
bytesRequired
boardCount
totalPortCount
digitalInputCount
digitalOutputCount
analogInputCount
analogOutputCount
pulseCounterCount
bindingCount
equipmentCount
sensorCount
automationCount
dependencyCount
```

L’estimation doit inclure l’alignement mémoire et les tables auxiliaires nécessaires à la résolution des identifiants.

## 8. Gardes absolues

Des gardes techniques peuvent rester compilées en dur, mais elles ne préallouent rien et ne décrivent pas la configuration normale.

Exemples :

```text
MAX_CONFIGURATION_INPUT_BYTES
ABSOLUTE_MAX_OBJECTS
ABSOLUTE_MAX_RELATIONS
ABSOLUTE_MAX_PORTS_IN_ONE_BOARD_DESCRIPTOR
MAX_NESTING_DEPTH
```

Leur rôle est de :

- protéger le parseur ;
- empêcher les boucles excessives ;
- détecter une configuration corrompue ;
- garantir que les compteurs et tailles ne débordent pas.

## 9. Descripteurs de modèles de cartes

Les propriétés fixes d’un modèle de carte doivent être partagées et, lorsque possible, placées en flash :

```text
nom du modèle
bus supporté
nombre et description des ports
capacités électriques et logiques
driver associé
version du descripteur
```

Une instance de carte ne stocke que les propriétés variables :

```text
BoardId
modelId
adresse ou emplacement
enabled
état de présence
paramètres d’instance
```

Cette séparation réduit fortement le coût des installations contenant plusieurs cartes identiques.

## 10. Règles de réduction mémoire

1. identifiants stables sur 16 bits ;
2. index runtime compacts lorsque nécessaires ;
3. enums et masques bornés ;
4. chaînes regroupées ou bornées ;
5. aucun `String` durable ;
6. aucune allocation générale dans les chemins réguliers ;
7. descripteurs fixes partagés en flash ;
8. relations par identifiants ;
9. aucune copie des 40 créneaux dans le nouveau domaine ;
10. allocation séquentielle dans l’arène ;
11. abandon global de la candidate en cas d’erreur ;
12. mesure exacte obligatoire avant activation.

## 11. Mesures à intégrer

La compilation devra afficher :

```cpp
sizeof(CfgZone)
sizeof(ZoneSchedule)
sizeof(PersistedConfig)
sizeof(EquipmentHeader)
sizeof(SensorHeader)
sizeof(HardwareBoardInstance)
sizeof(PortBinding)
sizeof(Dependency)
sizeof(Execution)
```

Le diagnostic runtime devra relever :

- taille totale de la configuration active ;
- taille demandée par la candidate ;
- marge restante dans chaque arène ;
- heap libre après boot ;
- heap minimum observé ;
- plus grande allocation extérieure aux arènes ;
- durée de construction et de validation.

## 12. Critères d’acceptation d’une configuration

La candidate est refusée si :

- le budget est dépassé ;
- une taille déborde ;
- un descripteur est inconnu ou incompatible ;
- une référence est orpheline ;
- un binding cible un port absent ;
- les capacités du port ne couvrent pas la fonction ;
- un identifiant est dupliqué ;
- une dépendance interdite ou cyclique est détectée.

Le refus ne modifie jamais la configuration active.

## 13. Conclusion

La V4 n’est pas dimensionnée par une liste de nombres fixes de zones, cartes, capteurs ou équipements.

Elle est dimensionnée par :

```text
contenu réel de la configuration
+ coût mémoire calculé
+ budget borné
+ validation complète
```

Cette approche permet des configurations génériques, modifiables et extensibles tout en conservant un comportement prévisible sur ESP32.
