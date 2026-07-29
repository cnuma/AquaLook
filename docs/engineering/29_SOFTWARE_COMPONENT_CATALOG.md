# AquaLook Engineering Reference — Catalogue logiciel

- Version documentaire : 1.0
- Statut : catalogue initial
- Dernière consolidation : 2026-07-27
- Sources : référentiel d’ingénierie, AGENTS.md et checkpoints
- Maturité : D3

## Objet

Ce catalogue recense les composants logiciels et indique leur responsabilité principale, leur état et leur document de référence.

## Composants principaux

| Composant | Responsabilité | État | Référence |
|---|---|---|---|
| Runtime | coordination, boucle principale, modes dégradés | validé | `15_RUNTIME_AND_PROFILING.md` |
| ScheduleManager | calcul et déclenchement des programmes | validé | `06_SCHEDULER.md` |
| ConfigManager | validation, NVS et montage LittleFS | validé | `07_CONFIGURATION_AND_PERSISTENCE.md` |
| chaîne relais | application sécurisée des états matériels | validée | `08_RELAY_AND_EQUIPMENT_CONTROL.md` |
| WebManager | routes et ressources HTTP | validé | `09_WEB_AND_HTTP_INTERFACES.md` |
| EventLog | événements et horodatage | validé | `10_TIME_AND_EVENTLOG.md` |
| DisplayManager | rendu LCD et rafraîchissement | validé | `13_DISPLAY_AND_TOUCH.md` |
| SdStaticHandler | résolution SD / LittleFS / firmware | validé | `14_SD_AND_STATIC_RESOURCES.md` |
| backend V4 | modèle d’équipements et orchestration transitoire | partiel | `16_V4_EQUIPMENT_MODEL_AND_WEATHER.md` |
| NetworkManager / Wi-Fi | connectivité locale et reprise | fonctionnel, frontières à consolider | `18_NETWORK_AND_WIFI.md` |
| HTTPS et sessions | authentification serveur et TLS | préliminaire | `19_HTTPS_AND_SESSIONS.md` |
| MQTTManager | télémétrie et commandes distantes | préliminaire | `20_MQTT.md` |
| OTAManager | mise à jour sécurisée | préliminaire | `21_OTA.md` |
| NotificationManager | diffusion issue des événements | préliminaire | `22_NOTIFICATIONS.md` |

## Fiche obligatoire d’un composant

- mission ;
- responsabilités et exclusions ;
- fichiers et classes ;
- point d’entrée réellement exécuté ;
- dépendances entrantes et sortantes ;
- interfaces exposées ;
- données possédées ;
- états et modes dégradés ;
- invariants ;
- tests ;
- événements ;
- maturité documentaire et fonctionnelle.

## Règles de dépendance

- le Scheduler ne pilote pas directement le matériel ;
- les composants métier ne montent pas les systèmes de fichiers ;
- les services distants ne deviennent pas nécessaires au fonctionnement local ;
- les interfaces utilisateur consomment l’état et ne remplacent pas la logique métier ;
- toute nouvelle classe ou route est reliée à un point d’entrée exécuté.

## Invariants

### INV-CAT-SW-001

Chaque responsabilité critique possède un propriétaire logiciel identifiable.

### INV-CAT-SW-002

Un composant préliminaire n’est pas présenté comme actif en production.

### INV-CAT-SW-003

Le catalogue est mis à jour lors de chaque checkpoint modifiant les frontières d’un composant.
