# AquaLook — Checkpoint évolution relais multi-cartes I2C — Run 1

Date : 2026-07-06  
Branche source : `refactor/static-assets-sd`  
HEAD annoncé par l'utilisateur : `9b7cf35`  
Branche de travail créée : `feature/relay-board-mapping`  
Issue GitHub : #2 — Évolution relais : cartes I2C, voies et mapping zones

## 1. Objectif du run

Démarrer proprement le chantier d'extension du nombre de relais en remplaçant progressivement le modèle actuel :

```text
nombre global de relais = 1 / 2 / 4 / 8
```

par un modèle matériel explicite :

```text
zone logique -> carte relais I2C -> voie de carte
```

Ce run ne modifie pas encore le moteur d'arrosage ni l'interface Web. Il pose le cadre projet, la branche, l'issue et la documentation d'architecture.

## 2. État constaté dans le code

Fichiers inspectés :

- `src/main.cpp`
- `src/RelaisManager.h`
- `src/RelaisManager.cpp`
- `src/ConfigManager.h`
- `src/ConfigManager.cpp`
- `platformio.ini`

Constats :

- `main.cpp` instancie `RelaisManager relaisMgr` et le callback `onRelayRequest()` appelle `relaisMgr.setRelay(zone, state)`.
- `RelaisManager` est le bon point d'abstraction à préserver.
- `RelaisManager` utilise actuellement un seul contrôleur relais I2C.
- `RelaisManager::i2cAddress()` retourne une seule adresse selon le contrôleur.
- `RelaisManager::setRelay()` applique actuellement `zone -> bit`, sans table de mapping.
- `ConfigManager` contient déjà :
  - `nbZones` ;
  - `nbRelaisPhysical` ;
  - `relayLogic` ;
  - `relayController`.
- Le stockage NVS est binaire, versionné et protégé par CRC.
- Toute évolution persistante doit donc être traitée comme une évolution de schéma.

## 3. Décision d'architecture

La zone AquaLook reste l'objet métier.

Le relais physique devient une affectation configurable.

Flux cible :

```text
ScheduleManager
    -> callback zone ON/OFF
        -> RelaisManager::setRelay(zone, state)
            -> ConfigManager::zoneRelayMapping(zone)
                -> carte I2C
                    -> voie relais
```

## 4. Modèle cible

### Carte relais

```cpp
struct CfgRelayBoard {
    bool    enabled;
    uint8_t controller;
    uint8_t i2cAddress;
    uint8_t channelCount;
    uint8_t logic;
};
```

### Mapping zone

```cpp
struct CfgZoneRelayMapping {
    bool    enabled;
    uint8_t boardIndex;
    uint8_t channelIndex;
};
```

### Limites initiales proposées

```cpp
MAX_RELAY_BOARDS = 8
RELAY_CHANNELS_MAX_PER_BOARD = 8
MAX_ZONES reste inchangé
MAX_ACTIVE_ZONES reste inchangé dans un premier temps
```

La capacité multi-cartes peut être introduite sans augmenter tout de suite le nombre de zones affichées.

## 5. Invariants à préserver

- Ne pas modifier le moteur d'arrosage pendant la première étape.
- Ne pas modifier la logique pluie.
- Ne pas modifier le mode intervalle.
- Ne pas modifier la couche SD/LittleFS validée.
- Ne pas casser le fonctionnement actuel carte unique.
- Conserver par défaut : `Zone N -> carte 0 -> voie N`.
- En cas de mapping invalide, ne jamais activer une sortie physique.
- Toute évolution NVS doit être explicite.

## 6. Fichier ajouté dans ce run

- `docs/architecture/RELAY_TOPOLOGY.md`

Ce fichier décrit :

- le contexte ;
- le modèle cible ;
- les structures proposées ;
- les règles de validation ;
- l'évolution cible de `RelaisManager` ;
- la stratégie de migration NVS ;
- le découpage des prochains runs.

## 7. Fichiers volontairement non modifiés dans ce run

- `src/main.cpp`
- `src/RelaisManager.h`
- `src/RelaisManager.cpp`
- `src/ConfigManager.h`
- `src/ConfigManager.cpp`
- fichiers Web
- fichiers SD/LittleFS
- moteur planning/arrosage

## 8. Prochain run recommandé

Run 2 : configuration persistante.

Objectif : ajouter le modèle de données relais sans encore changer l'interface Web.

Modifications prévues :

- `src/ConfigManager.h`
  - ajouter `CfgRelayBoard` ;
  - ajouter `CfgZoneRelayMapping` ;
  - ajouter getters ;
  - ajouter constantes `MAX_RELAY_BOARDS` et `RELAY_CHANNELS_MAX_PER_BOARD`.

- `src/ConfigManager.cpp`
  - intégrer les structures dans `PersistedConfig` ;
  - incrémenter `CFG_NVS_SCHEMA` ou préparer une lecture ancien schéma ;
  - initialiser une topologie par défaut compatible ;
  - ajouter normalisation et validation des mappings.

- `src/RelaisManager.*`
  - lecture de la topologie, mais conservation provisoire du comportement carte unique si besoin.

## 9. Risques identifiés

- Changement de structure NVS : risque de reset configuration si migration non gérée.
- Multi-cartes : risque de collision I2C si deux cartes ont la même adresse.
- Mapping Web futur : risque d'affecter deux zones à la même voie.
- Capacité relais supérieure à la capacité zones : à gérer proprement côté UI.
- Capacité zones supérieure à relais disponibles : autoriser seulement si zones non mappées ou bloquer selon décision utilisateur.

## 10. Statut validation

- Compilation : non lancée dans ce run documentaire.
- Test matériel : non applicable.
- Dépôt : branche dédiée créée.
- Suivi projet : issue créée.
- Documentation : ajoutée.
