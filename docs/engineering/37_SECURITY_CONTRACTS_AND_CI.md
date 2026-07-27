# AquaLook Engineering Reference — Contrats de cybersécurité et CI

- Version documentaire : 1.0
- Statut : référence reliée au code
- Dernière consolidation : 2026-07-27
- Sources : `tests/contracts/test_security_contracts.py`, workflow GitHub Actions, code du dépôt
- Maturité : D4

## Objet

Ce document définit les contrôles de cybersécurité exécutables sans carte ESP32. Ils complètent les compilations PlatformIO et les essais matériels, mais ne les remplacent pas.

## Exécution locale

```powershell
python -m unittest discover -s tests/contracts -p "test_*.py" -v
```

Le test utilise uniquement la bibliothèque standard Python et lit les sources versionnées.

## Exécution CI

Le workflow `.github/workflows/security-contracts.yml` s’exécute sur les pull requests et sur les changements de `main` affectant le code, les contrats ou les documents de sécurité.

Propriétés :

- permissions GitHub limitées à `contents: read` ;
- timeout de cinq minutes ;
- Python 3.12 ;
- aucune dépendance externe ;
- aucun secret nécessaire.

## Contrats actifs

| Contrat | État | Preuve |
|---|---|---|
| rejet des traversées de chemin SD | actif | `SdStaticHandler::mapRequestPath()` |
| exclusion des chemins `/api/` du handler SD | actif | test statique |
| absence de cache sur `/api/storage` | actif | en-tête `no-store` |
| timeout météo inférieur ou égal à 10 s | actif | `HTTPClient::setTimeout()` |
| routes sensibles déclarées comme POST JSON | actif | inventaire `POST_JSON` |

## Dettes exécutables

Trois tests portent `@unittest.expectedFailure`. La CI reste verte, mais affiche explicitement ces écarts à chaque exécution :

1. mot de passe Wi-Fi imprimé en clair par `handleSetWifi()` ;
2. point d’accès captif créé sans mot de passe ;
3. OpenWeatherMap appelé en HTTP non chiffré.

Lorsqu’une correction est intégrée, l’annotation `expectedFailure` correspondante doit être supprimée dans le même commit. Le test devient alors bloquant.

## Limites

Ces contrats sont statiques. Ils ne prouvent pas :

- l’authentification réelle des routes ;
- la résistance aux requêtes concurrentes ;
- la consommation mémoire TLS ;
- le comportement sur le matériel ;
- l’absence de vulnérabilité dans les bibliothèques.

## Critères D5

Le domaine passera en D5 lorsque :

- les trois dettes exécutables seront corrigées et les tests ordinaires ;
- les contrats HTTP négatifs seront exécutés contre un firmware réel ;
- les mesures mémoire et les tests de charge seront archivés ;
- une procédure de récupération aura été testée ;
- les résultats seront référencés par un checkpoint.

## Références

- `tests/contracts/test_security_contracts.py` ;
- `.github/workflows/security-contracts.yml` ;
- `19_HTTPS_AND_SESSIONS.md` ;
- `23_SECURITY_OPERATIONS.md` ;
- `30_TEST_AND_ANTI_REGRESSION_MATRIX.md` ;
- `docs/security/SECURITY_RISK_REGISTER.md`.
