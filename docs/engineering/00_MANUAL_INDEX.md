# AquaLook Engineering Manual

- Version documentaire : 1.0
- Révision initiale : 2026-07
- État : référence d'ingénierie vivante
- Dépôt : `cnuma/AquaLook`

## Mission

Ce manuel doit permettre à un ingénieur qui ne connaît pas AquaLook de comprendre le produit, retrouver les sources détaillées, diagnostiquer le système et préparer une évolution sans dépendre de l'historique des conversations.

## Organisation

| Référence | Objet |
|---|---|
| `01_PROJECT_STATUS.md` | situation réelle, acquis, travaux ouverts, risques et priorités |
| `02_SYSTEM_OVERVIEW.md` | vue d'ensemble fonctionnelle, logicielle et matérielle |
| `03_DOCUMENTATION_GOVERNANCE.md` | règles de maintenance et source de vérité |
| `docs/architecture/ARCHITECTURE_OVERVIEW.md` | dix piliers d'architecture |
| `docs/architecture/OBSERVABILITY.md` | logs, métriques et diagnostic |
| `docs/architecture/QUALITY.md` | stratégie qualité et validation |
| `docs/security/CYBERSECURITY_ARCHITECTURE.md` | architecture de sécurité |
| `docs/security/SECURITY_RISK_REGISTER.md` | registre des risques cyber |
| `docs/roadmap/ARCHITECTURE_GOVERNANCE.md` | gouvernance des futurs chantiers |
| `docs/checkpoints/` | états de reprise exacts et historiques |

## Arborescence cible

```text
docs/engineering/
├── 00_MANUAL_INDEX.md
├── 01_PROJECT_STATUS.md
├── 02_SYSTEM_OVERVIEW.md
├── 03_DOCUMENTATION_GOVERNANCE.md
├── 01_SYSTEM/
├── 02_HARDWARE/
├── 03_SOFTWARE/
├── 04_NETWORK/
├── 05_SECURITY/
├── 06_OBSERVABILITY/
├── 07_TESTS/
├── 08_OPERATIONS/
├── 09_ARCHITECTURE_DECISIONS/
├── diagrams/
└── annexes/
```

Les sous-dossiers sont créés progressivement lorsqu'un contenu réel doit y être placé. Aucun fichier vide n'est requis pour donner artificiellement l'impression que le domaine est documenté.

## Niveaux de maturité documentaire

- D0 : absent ;
- D1 : description fonctionnelle ;
- D2 : architecture et interfaces ;
- D3 : invariants, séquences et tests ;
- D4 : exploitable par un nouvel ingénieur ;
- D5 : référence validée, maintenue et reliée au code.

Les composants critiques doivent viser D5 : planification, orchestration, relais, persistance, stockage SD, OTA, Web, sécurité, MQTT et exploitation distante.
