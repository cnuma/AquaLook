# AquaLook V4 — Stratégie d’intégration runtime des sorties

**Date :** 8 juillet 2026  
**Phase :** 4 — Run 4.1  
**Statut :** stratégie d’intégration à valider avant code runtime

## 1. Objectif

Ce document définit la stratégie de transition entre le runtime historique AquaLook et le socle V4 de drivers binaires livré en Phase 3.

Le but n’est pas de brancher immédiatement les drivers au runtime, mais de fixer la frontière d’intégration avant toute modification de `RelaisManager`, de NVS ou du comportement matériel.

## 2. Décision terminologique

La couche runtime V4 doit utiliser une notion générique de **sortie commandable** plutôt qu’une notion directement liée aux relais.

Terminologie retenue :

```text
EquipmentOutput
```

Un `EquipmentOutput` représente une sortie logique pilotable par AquaLook :

- vanne de zone ;
- pompe ;
- auxiliaire ;
- éclairage ;
- ventilation de serre ;
- autre actionneur binaire futur.

La terminologie `Relay` reste valide uniquement pour la couche physique relais :

```text
RelayBoard
RelayChannel
RelayDriver
RelayTopology
RelayAssignment historique
```

Règle durable :

```text
Output / EquipmentOutput = domaine et runtime
Relay                  = backend physique relais
```

## 3. Frontière conceptuelle

La chaîne cible devient :

```text
Runtime historique / ScheduleManager
  -> décision métier d’activation
  -> EquipmentOutput
  -> EquipmentPortBinding
  -> PortDefinition
  -> ControllerDefinition
  -> BinaryActuatorDriverRegistry
  -> BinaryActuatorDriverOps
  -> driver concret
  -> adaptateur plateforme
```

Le runtime ne doit pas connaître directement :

- le bus I²C ;
- l’adresse d’une carte ;
- le nombre de voies d’une carte ;
- le type concret de driver ;
- le composant XL9535, GPIO ou autre.

## 4. Rôle des relais dans la stratégie

Les relais restent le premier backend matériel réellement utilisé par AquaLook.

Ils ne doivent cependant pas rester la notion centrale du modèle runtime V4.

Exemple de lecture correcte :

```text
Zone 1
  -> EquipmentOutput: zone_valve_1
  -> EquipmentPortBinding
  -> Controller: relay board 0
  -> Port: channel 1
  -> Driver: XL9535 ou GPIO selon la configuration matérielle
```

Ce modèle permet de préserver le matériel actuel tout en ouvrant l’architecture à d’autres sorties binaires.

## 5. Compatibilité historique obligatoire

Tant que la migration runtime n’est pas activement décidée, le comportement existant doit rester équivalent à :

```text
Zone N -> carte relais 0 -> voie N
```

Cette compatibilité doit être considérée comme un profil de compatibilité par défaut, pas comme le modèle cible durable.

## 6. Positionnement de RelaisManager

`RelaisManager` reste le point de pilotage historique tant que le runtime n’est pas migré.

À ce stade, il ne doit pas être renommé ni remplacé.

Son évolution doit être progressive :

```text
Run 4.1 : documenter la frontière Output / Relay
Run 4.2 : introduire un adaptateur Output -> Relay non branché au runtime
Run 4.3 : raccorder éventuellement RelaisManager derrière cet adaptateur
Run ultérieur : envisager un renommage ou remplacement si le runtime est stabilisé
```

`RelaisManager` doit donc être traité comme un adaptateur historique, pas comme le nom durable du domaine V4.

## 7. Interdits Run 4.1

Le Run 4.1 ne doit pas introduire :

- de modification NVS ;
- de migration de configuration persistée ;
- de changement de comportement matériel ;
- de remplacement de `RelaisManager` ;
- de raccord automatique des drivers Phase 3 au runtime ;
- de nouveau registre global implicite ;
- d’écriture I²C ou GPIO V4 au boot.

## 8. Travaux autorisés Run 4.1

Sont autorisés :

- documentation de la frontière `EquipmentOutput` / `Relay` ;
- analyse des points d’entrée de `RelaisManager` ;
- cartographie des appels runtime actuels ;
- définition d’un futur adaptateur ;
- préparation d’un plan de migration fichier par fichier ;
- ajout éventuel d’un squelette isolé non instancié, uniquement après validation explicite.

## 9. Plan de migration recommandé

### Étape A — Documentation

Créer et valider la stratégie d’intégration runtime sans code fonctionnel.

### Étape B — Cartographie

Identifier précisément :

- les méthodes publiques de `RelaisManager` utilisées par le runtime ;
- les dépendances vers `RelayTopology` et `RelayAssignment` ;
- les hypothèses de numérotation des zones ;
- les points de contact avec NVS.

### Étape C — Adaptateur isolé

Introduire un futur composant d’adaptation :

```text
EquipmentOutputRuntimeAdapter
```

Ce composant devra rester non instancié et sans effet au boot tant qu’un run dédié ne l’active pas.

### Étape D — Profil de compatibilité

Définir explicitement un mapping de compatibilité :

```text
EquipmentOutput zone_valve_N -> relay board 0 -> relay channel N
```

Ce mapping ne doit pas nécessiter de migration NVS dans un premier temps.

### Étape E — Branchement contrôlé

Brancher progressivement le runtime uniquement après validation :

- compilation PlatformIO ;
- logs de mapping ;
- absence d’écriture matérielle imprévue ;
- test de non-régression du fonctionnement historique.

## 10. Impact attendu sur les fichiers

### Run 4.1

```text
docs/architecture/AQUALOOK_V4_EQUIPMENT_OUTPUT_RUNTIME_INTEGRATION_STRATEGY.md
```

Création du présent document.

```text
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
```

Mise à jour de la prochaine étape pour pointer vers la Phase 4 — Run 4.1.

### Runs suivants possibles

```text
src/RelaisManager.h
src/RelaisManager.cpp
```

À analyser avant modification. Aucun changement runtime ne doit être fait sans plan validé.

```text
src/domain/EquipmentOutputRuntimeAdapter.h
src/domain/EquipmentOutputRuntimeAdapter.cpp
```

Emplacement potentiel pour un adaptateur futur, si la structure V4 le confirme.

## 11. Critères de réussite Run 4.1

Le Run 4.1 est réussi si :

1. la frontière `EquipmentOutput` / `Relay` est claire ;
2. la terminologie générique est réservée au domaine/runtime ;
3. la terminologie relais est limitée au backend physique ;
4. aucune modification NVS n’est introduite ;
5. aucun changement runtime n’est introduit ;
6. le prochain run peut analyser `RelaisManager` sans ambiguïté.

## 12. Décision de clôture

La stratégie retenue est :

```text
AquaLook V4 ne doit pas faire de Relay la notion centrale du runtime.
Le runtime manipule des EquipmentOutput.
Les relais sont une implémentation physique possible de ces sorties.
```
