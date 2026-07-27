# AquaLook Engineering Manual

- Version documentaire : 1.1
- Révision : 2026-07-27
- État : référence d'ingénierie vivante
- Dépôt : `cnuma/AquaLook`

## Mission

Ce manuel permet à un ingénieur qui ne connaît pas AquaLook de comprendre le produit, retrouver les sources détaillées, diagnostiquer le système et préparer une évolution sans dépendre de l'historique des conversations.

## Documents centraux

| Référence | Objet | Maturité |
|---|---|---|
| `01_PROJECT_STATUS.md` | situation réelle, acquis, travaux ouverts, risques et priorités | D3 |
| `02_SYSTEM_OVERVIEW.md` | vue d'ensemble fonctionnelle, logicielle et matérielle | D3 |
| `03_DOCUMENTATION_GOVERNANCE.md` | règles de maintenance et source de vérité | D4 |
| `04_REFERENCE_USE_RULES.md` | règles d’utilisation du dépôt comme référence | D4 |
| `05_EDITORIAL_STANDARD.md` | style, structure et interdiction d’inventer les interfaces | D4 |
| `06_SCHEDULER.md` | planification et déclenchement | D4 |
| `07_CONFIGURATION_AND_PERSISTENCE.md` | NVS, LittleFS, SD et cycle de configuration | D4 |
| `08_RELAY_AND_EQUIPMENT_CONTROL.md` | relais, topologie et sécurités matérielles | D4 |
| `09_WEB_AND_HTTP_INTERFACES.md` | WebManager et inventaire des URL confirmées | D4 |
| `10_TIME_AND_EVENTLOG.md` | politique NTP, `millis()` et EventLog centralisé | D4 |
| `11_CHECKPOINT_CONSOLIDATION.md` | consolidation obligatoire du référentiel à chaque checkpoint | D4 |
| `12_HARDWARE_PLATFORM.md` | plateforme CYD, SPI, écran, tactile, microSD et modes dégradés | D3 |
| `13_DISPLAY_AND_TOUCH.md` | rendu LCD, redraws, EventBus et tactile XPT2046 | D4 |
| `14_SD_AND_STATIC_RESOURCES.md` | ressources Web SD, fallback LittleFS et SVG firmware | D4 |
| `15_RUNTIME_AND_PROFILING.md` | Runtime non bloquant, temps mural et profiler | D4 |
| `16_V4_EQUIPMENT_MODEL_AND_WEATHER.md` | backend V4, équipements, pompe shadow et météo | D4 |
| `17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md` | compilation, upload, buildfs et validation matérielle | D4 |
| `18_NETWORK_AND_WIFI.md` | réseau local, Wi-Fi, reconnexion et modes hors ligne | D4 |
| `19_HTTPS_AND_SESSIONS.md` | état HTTP audité et cible HTTPS avec sessions | D3 |
| `20_MQTT.md` | contrat architectural MQTT préliminaire | D2 |
| `21_OTA.md` | intégrité, signature, partitions et rollback OTA | D2 |
| `22_NOTIFICATIONS.md` | diffusion multi-canal issue de l’EventLog | D2 |
| `23_SECURITY_OPERATIONS.md` | secrets, vulnérabilités, révocation, incidents et CI | D4 |
| `24_DIAGNOSTICS_AND_OBSERVABILITY.md` | métriques, santé, EventLog et profiler | D4 |
| `25_BACKUP_RESTORE_AND_MAINTENANCE.md` | sauvegarde, restauration, remplacement et maintenance | D3 |
| `26_DATA_MODEL_AND_JSON.md` | familles de données, validation et contrats JSON | D2 |
| `27_FILE_AND_STORAGE_MAP.md` | cartographie Flash, NVS, LittleFS, SD, RAM et dépôt | D3 |
| `28_HARDWARE_COMPONENT_CATALOG.md` | catalogue des composants matériels | D2 |
| `29_SOFTWARE_COMPONENT_CATALOG.md` | catalogue des composants logiciels | D3 |
| `30_TEST_AND_ANTI_REGRESSION_MATRIX.md` | matrice de validation et critères de blocage | D4 |
| `31_CHECKPOINT_INDEX.md` | index des états de reprise | D3 |
| `32_GLOSSARY.md` | définitions communes | D3 |
| `33_DOCUMENT_MATURITY_MATRIX.md` | maturité D0 à D5 et conditions de progression | D4 |
| `34_ENGINEERING_REFERENCE_INDEX.md` | point d’entrée global du référentiel | D4 |
| `35_CODE_TRACEABILITY_REGISTER.md` | ancrages code, niveaux de preuve et écarts ouverts | D4 |
| `36_DETAILED_EQUIPMENT_MODEL_SCHEMA.md` | schéma détaillé du modèle d’équipements | D4 |
| `37_SECURITY_CONTRACTS_AND_CI.md` | contrats statiques de cybersécurité et workflow CI | D4 |
| `CODE_LINKED_REFERENCE_PROCESS.md` | processus obligatoire de consolidation depuis le code | D4 |

## Références spécialisées

| Référence | Objet |
|---|---|
| `docs/architecture/ARCHITECTURE_OVERVIEW.md` | dix piliers d'architecture |
| `docs/architecture/OBSERVABILITY.md` | logs, métriques et diagnostic |
| `docs/architecture/QUALITY.md` | stratégie qualité et validation |
| `docs/security/CYBERSECURITY_ARCHITECTURE.md` | architecture de sécurité |
| `docs/security/SECURITY_RISK_REGISTER.md` | registre des risques cyber et preuves associées |
| `docs/roadmap/ARCHITECTURE_GOVERNANCE.md` | gouvernance des futurs chantiers |
| `docs/checkpoints/` | états de reprise exacts et historiques |

## Règle de consolidation

Le code du commit ciblé prime en cas de divergence. Les checkpoints consolident l’état validé. Les roadmaps décrivent les évolutions et ne sont jamais recopiées comme fonctionnalités présentes.

Toute évolution fonctionnelle met à jour les documents impactés, `35_CODE_TRACEABILITY_REGISTER.md` et les contrats exécutables concernés. Un `expectedFailure` matérialise une dette ouverte et interdit de déclarer le risque correspondant fermé.

## Niveaux de maturité documentaire

- D0 : absent ;
- D1 : description fonctionnelle ;
- D2 : architecture et interfaces ;
- D3 : invariants, séquences et tests ;
- D4 : exploitable par un nouvel ingénieur ;
- D5 : référence validée, maintenue, testée et reliée au code.
