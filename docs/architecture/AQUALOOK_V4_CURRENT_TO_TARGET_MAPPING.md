# AquaLook V4 — Cartographie de l’existant vers la cible

**Run :** V4 Phase 0 — Run 0  
**Base :** `feature/relay-board-mapping`  
**Référence initiale :** `b40c63720bc95a362bced9f26f3f78afaac76804`  
**Date :** 7 juillet 2026

## 1. Objet

Ce document relie l’architecture actuelle d’AquaLook à l’architecture cible V4.

Il ne décrit pas encore les structures C++ finales. Il identifie :

- ce qui existe déjà ;
- ce qui peut être conservé ;
- ce qui doit être déplacé ou réduit ;
- ce qui manque ;
- l’ordre de transition recommandé.

## 2. Architecture actuelle synthétique

```text
main.cpp
├── ConfigManager
├── RelaisManager
├── ScheduleManager
├── WiFiManager
├── NTPManager
├── WeatherManager
├── WebManager
├── DisplayManager
├── ScreenManager
├── EventBus
└── EventLog
```

Le système est organisé par managers. `main.cpp` initialise les modules et câble le callback de commande des relais utilisé par le planificateur.

Le travail déjà validé sur `feature/relay-board-mapping` ajoute :

- `RelayTopology` ;
- `RelayAssignment` ;
- des rôles logiques de relais ;
- une résolution carte/voie dans `RelaisManager` ;
- la compatibilité `Zone N -> carte 0 -> voie N`.

## 3. Correspondance modules actuels / modules V4

| Existant | Responsabilité actuelle | Cible V4 | Décision de transition |
|---|---|---|---|
| `main.cpp` | assemblage et ordre d’initialisation | composition root | conserver, réduire les décisions métier |
| `ConfigManager` | NVS, migration, nombreux réglages | `ConfigurationService` + `PersistencePort` + adaptateur NVS | découpler progressivement, ne pas réécrire d’un bloc |
| `ScheduleManager` | calcul planning et exécution | `AutomationEngine` pour la décision, `EquipmentOrchestrator` pour l’exécution | séparer production d’intention et commande |
| `RelaisManager` | commande physique et sécurité de durée | adaptateur actionneur relais | conserver comme couche matérielle stricte |
| `RelayTopology` | rôle logique vers carte/voie | `HardwareAssignment` relais | conserver comme première topologie d’actionneur |
| `WeatherManager` | acquisition météo et pluie | fournisseur d’observations externes | conserver, exposer qualité et fraîcheur |
| `NTPManager` | heure civile synchronisée | `ClockPort` / adaptateur NTP | conserver, distinguer temps civil et temps monotone |
| `WiFiManager` | connectivité et portail captif | infrastructure réseau | conserver hors domaine |
| `WebManager` | routes, API et parfois orchestration de commandes | adaptateur Web vers services applicatifs | retirer progressivement toute décision directe |
| `DisplayManager` | rendu et commandes tactiles | adaptateur UI vers vues et services | conserver, interdire les accès matériels métier directs |
| `ScreenManager` | veille écran | infrastructure présentation | conserver |
| `EventBus` | flags transversaux | mécanisme de notification léger | conserver temporairement, ne pas l’utiliser comme domaine V4 |
| `EventLog` | journal RAM | `EventLogPort` | conserver et généraliser progressivement |

## 4. Objets métier actuels et cible

### Zone

**Actuel :** zone liée aux programmes, paramètres pluie et commande de relais par index.

**Cible :** zone métier possédant une identité stable et référençant un équipement de distribution.

**Transition :** conserver les index actuels comme compatibilité runtime, puis introduire une identité indépendante avant la persistance V4.

### Programme d’arrosage

**Actuel :** règle temporelle et contexte d’exécution partiellement mêlés.

**Cible :** automatisme temporel produisant une intention de démarrage de zone.

**Transition :** ne pas changer l’algorithme jours fixes / intervalle lors de la Phase 1.

### Relais

**Actuel historique :** souvent assimilé à une zone.

**État de branche :** généralisé via `RelayAssignment` et rôles.

**Cible :** implémentation matérielle d’un actionneur binaire.

### Équipement

**Actuel :** concept non représenté comme objet autonome.

**Cible :** objet métier central pilotable, avec type, capacités, état sûr, mode et dépendances.

### Capteur

**Actuel :** données spécialisées par manager, principalement météo.

**Cible :** capteur et observation normalisée avec qualité, fraîcheur et unité.

### Intention

**Actuel :** implicite dans les appels de démarrage, arrêt et callbacks.

**Cible :** objet explicite portant origine, priorité, cible, action et corrélation.

### Exécution

**Actuel :** état runtime du planificateur.

**Cible :** instance d’exécution traçable, distincte de la règle qui l’a créée.

## 5. Flux actuels et flux cibles

### Démarrage automatique actuel

```text
NTPManager
-> ScheduleManager
-> callback
-> RelaisManager
-> RelayTopology
-> carte / voie
```

### Démarrage automatique cible

```text
ClockPort + observations
-> AutomationEngine
-> Intention
-> IntentArbiter
-> EquipmentOrchestrator
-> ActuatorPort
-> RelaisManager
-> RelayTopology
-> carte / voie
```

### Commande manuelle actuelle

La commande vient du Web ou du LCD et rejoint un chemin spécifique de démarrage ou d’arrêt.

### Commande manuelle cible

```text
Web ou LCD
-> ManualControlService
-> Intention manuelle
-> même arbitrage
-> même orchestrateur
```

## 6. Couplages à réduire

### ConfigManager trop central

Risque : transformer `ConfigManager` en nouveau modèle métier V4 et figer la persistance trop tôt.

Mesure : les modèles V4 sont d’abord créés en mémoire, avec un adaptateur de compatibilité vers la configuration existante.

### ScheduleManager décision + exécution

Risque : ajouter pompe et dépendances directement dans le planificateur.

Mesure : le planificateur doit progressivement produire une demande sans orchestrer le matériel.

### Commandes UI

Risque : multiplier les chemins Web, LCD et API ayant des comportements différents.

Mesure : toutes les commandes futures passent par des services applicatifs communs.

### États matériels présentés comme états réels

Risque : confondre commande envoyée et équipement physiquement actif.

Mesure : introduire les états demandé, autorisé, appliqué et observé.

## 7. Éléments à préserver sans changement en Phase 1

- algorithmes jours fixes et intervalle ;
- ancrage du mode intervalle ;
- règle pluie existante ;
- routes Web existantes ;
- format NVS ;
- ressources SD/LittleFS ;
- rendu LCD ;
- logique directe/inverse des relais ;
- sécurité de durée maximale ;
- ordre de boot validé ;
- compatibilité carte unique.

## 8. Éléments nouveaux nécessaires en Phase 1

- identités métier stables ;
- modèle minimal d’équipement ;
- capacités ;
- états métier distincts ;
- modèle d’intention ;
- modèle d’exécution ;
- modèle de défaut ;
- relations de dépendance ;
- validation du graphe ;
- limites embarquées explicites.

Ces éléments doivent rester isolés du runtime existant pendant les premiers runs.

## 9. Ordre de migration recommandé

1. définir identités et limites ;
2. introduire les types de domaine sans intégration runtime ;
3. valider références et dépendances ;
4. introduire intentions et exécutions ;
5. créer un adaptateur de lecture depuis la configuration actuelle ;
6. introduire l’actionneur binaire ;
7. faire passer une zone test par l’orchestrateur ;
8. seulement ensuite préparer la persistance V4.

## 10. Risques principaux

- surdimensionnement du modèle pour l’ESP32 ;
- multiplication d’objets dynamiques ;
- duplication temporaire d’états ;
- confusion entre identifiant stable et index compact ;
- extension prématurée du NVS ;
- contournement de l’orchestrateur par les interfaces existantes ;
- régression de l’arrosage validé pendant la transition.

## 11. Conclusion

L’architecture actuelle possède déjà deux bases utiles : la séparation par managers et le callback entre planning et relais. `RelayTopology` constitue également le premier découplage matériel durable.

La transformation V4 doit donc être incrémentale. Elle ne nécessite pas de remplacer immédiatement les managers existants, mais d’introduire progressivement un domaine explicite et des services communs entre eux et le matériel.
