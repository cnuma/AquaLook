# AquaLook Developer Guide — Ajouter un contrat de cybersécurité CI

- Référence : DEV-016
- Statut : actif
- Maturité : D4

## Principe

Un contrat statique protège une propriété précise du dépôt par un test déterministe exécuté dans `.github/workflows/security-contracts.yml`.

## Étapes

1. formuler le comportement interdit ou exigé ;
2. choisir des fichiers et symboles stables à inspecter ;
3. écrire le test dans `tests/contracts/test_security_contracts.py` ;
4. fournir un message d'échec indiquant le fichier et la correction attendue ;
5. limiter les faux positifs et ne pas tester un simple commentaire ;
6. utiliser `expectedFailure` uniquement pour une dette réelle, tracée par issue ;
7. vérifier qu'une correction fait passer le test sans supprimer sa portée ;
8. exécuter la suite localement ;
9. contrôler le workflow sur la PR ;
10. mettre à jour `37_SECURITY_CONTRACTS_AND_CI.md` et la traçabilité.

## Règles

Ne jamais transformer un échec inattendu en `expectedFailure` pour faire passer la CI. Une dette corrigée doit perdre son statut attendu en échec dans la même PR.

## Références

- `.github/workflows/security-contracts.yml`
- `tests/contracts/test_security_contracts.py`
- `docs/engineering/37_SECURITY_CONTRACTS_AND_CI.md`
- issue #16
