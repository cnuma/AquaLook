# AquaLook Engineering Reference — Index global

- Version documentaire : 1.1
- Statut : point d’entrée principal
- Dernière consolidation : 2026-07-27
- Maturité : D4

## Démarrage rapide

| Besoin | Document |
|---|---|
| comprendre le projet | `02_SYSTEM_OVERVIEW.md` |
| connaître l’état réel | `01_PROJECT_STATUS.md` |
| reprendre un développement | `AGENTS.md`, `31_CHECKPOINT_INDEX.md` et dernier checkpoint |
| modifier la planification | `06_SCHEDULER.md` |
| modifier la configuration | `07_CONFIGURATION_AND_PERSISTENCE.md`, `26_DATA_MODEL_AND_JSON.md` |
| intervenir sur les relais | `08_RELAY_AND_EQUIPMENT_CONTROL.md`, `28_HARDWARE_COMPONENT_CATALOG.md` |
| modifier le Web | `09_WEB_AND_HTTP_INTERFACES.md` |
| diagnostiquer l’heure ou les logs | `10_TIME_AND_EVENTLOG.md`, `24_DIAGNOSTICS_AND_OBSERVABILITY.md` |
| intervenir sur écran/tactile | `12_HARDWARE_PLATFORM.md`, `13_DISPLAY_AND_TOUCH.md` |
| gérer SD/LittleFS | `14_SD_AND_STATIC_RESOURCES.md`, `27_FILE_AND_STORAGE_MAP.md` |
| compiler et tester | `17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md`, `30_TEST_AND_ANTI_REGRESSION_MATRIX.md` |
| travailler sur réseau et sécurité | `18_NETWORK_AND_WIFI.md`, `19_HTTPS_AND_SESSIONS.md`, `23_SECURITY_OPERATIONS.md` |
| préparer MQTT, OTA ou notifications | `20_MQTT.md`, `21_OTA.md`, `22_NOTIFICATIONS.md` |
| sauvegarder ou restaurer | `25_BACKUP_RESTORE_AND_MAINTENANCE.md` |
| retrouver un terme | `32_GLOSSARY.md` |
| évaluer la documentation | `33_DOCUMENT_MATURITY_MATRIX.md` |
| relier une affirmation au code | `35_CODE_TRACEABILITY_REGISTER.md` |
| consolider un tome depuis le code | `CODE_LINKED_REFERENCE_PROCESS.md` |

## Registres spécialisés

- architecture : `docs/architecture/` ;
- cybersécurité : `docs/security/` ;
- roadmaps : `docs/roadmap/` ;
- checkpoints : `docs/checkpoints/` ;
- règles Codex : `docs/codex/` ;
- référentiel : `docs/engineering/`.

## Index des interfaces

- URL et méthodes HTTP : `09_WEB_AND_HTTP_INTERFACES.md` ;
- protocoles réseau : `18_NETWORK_AND_WIFI.md` ;
- sessions et HTTPS : `19_HTTPS_AND_SESSIONS.md` ;
- MQTT : `20_MQTT.md` ;
- OTA : `21_OTA.md` ;
- stockages et fichiers : `27_FILE_AND_STORAGE_MAP.md` ;
- composants matériels : `28_HARDWARE_COMPONENT_CATALOG.md` ;
- composants logiciels : `29_SOFTWARE_COMPONENT_CATALOG.md` ;
- ancrages vers le code : `35_CODE_TRACEABILITY_REGISTER.md`.

## Règle de vérité

L’ordre de confiance est : code du commit ciblé, checkpoint correspondant, invariants et décisions versionnés, référentiel d’ingénierie, architectures spécialisées, roadmaps, conversations.

Une interface future n’est jamais ajoutée comme active avant extraction du code et validation.

Une affirmation D4 ou D5 doit être reliée à un ancrage de code et à un niveau de preuve dans `35_CODE_TRACEABILITY_REGISTER.md`.

## Maintenance

À chaque checkpoint :

1. mettre à jour les tomes impactés ;
2. vérifier les URL, fichiers, interfaces et composants ;
3. réviser la matrice de maturité ;
4. mettre à jour l’index des checkpoints ;
5. mettre à jour le registre de traçabilité ;
6. vérifier cet index global.
