# AquaLook Engineering Manual

- Version documentaire : 1.0
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
| `06_SCHEDULER.md` | planification et déclenchement | D3 |
| `07_CONFIGURATION_AND_PERSISTENCE.md` | NVS, LittleFS, SD et cycle de configuration | D3 |
| `08_RELAY_AND_EQUIPMENT_CONTROL.md` | relais, topologie et sécurités matérielles | D3 |
| `09_WEB_AND_HTTP_INTERFACES.md` | WebManager et inventaire des URL confirmées | D3 |
| `10_TIME_AND_EVENTLOG.md` | politique NTP, `millis()` et EventLog centralisé | D3 |

## Références spécialisées

| Référence | Objet |
|---|---|
| `docs/architecture/ARCHITECTURE_OVERVIEW.md` | dix piliers d'architecture |
| `docs/architecture/OBSERVABILITY.md` | logs, métriques et diagnostic |
| `docs/architecture/QUALITY.md` | stratégie qualité et validation |
| `docs/security/CYBERSECURITY_ARCHITECTURE.md` | architecture de sécurité |
| `docs/security/SECURITY_RISK_REGISTER.md` | registre des risques cyber |
| `docs/roadmap/ARCHITECTURE_GOVERNANCE.md` | gouvernance des futurs chantiers |
| `docs/checkpoints/` | états de reprise exacts et historiques |

## Règle de consolidation

Les checkpoints sont les sources principales pour consolider l’état réellement validé. Le code du commit ciblé prime en cas de divergence. Les roadmaps décrivent les évolutions ; elles ne doivent pas être recopiées comme fonctionnalités présentes.

## Niveaux de maturité documentaire

- D0 : absent ;
- D1 : description fonctionnelle ;
- D2 : architecture et interfaces ;
- D3 : invariants, séquences et tests ;
- D4 : exploitable par un nouvel ingénieur ;
- D5 : référence validée, maintenue et reliée au code.

Les composants critiques visent D5 : planification, orchestration, relais, persistance, stockage SD, OTA, Web, sécurité, MQTT et exploitation distante.
