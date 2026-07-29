# AquaLook Engineering Reference — Exploitation cybersécurité

- Version documentaire : 1.1
- Statut : référence reliée aux contrôles exécutables
- Dernière consolidation : 2026-07-27
- Sources : architecture cybersécurité, registre des risques, contrats CI
- Composants : firmware, Web, Wi-Fi, météo, MQTT, OTA, VPS, Git, secrets
- Maturité : D4

## Objet

Ce document traduit les principes de cybersécurité en procédures d’exploitation et relie les risques aux contrôles automatisables du dépôt.

## Responsabilités d’exploitation

- inventorier équipements, versions et identités ;
- contrôler comptes, certificats et secrets actifs ;
- appliquer les mises à jour de sécurité ;
- surveiller les échecs d’authentification et anomalies ;
- sauvegarder et tester les restaurations ;
- révoquer un équipement ou secret compromis ;
- conserver la traçabilité des changements ;
- préparer la fin de support.

## Gestion des secrets

Pour chaque secret : propriétaire, emplacement, provisionnement, durée de vie, rotation, révocation et comportement après perte.

Interdictions :

- secret dans Git, un checkpoint ou une archive ;
- secret complet dans un log série ou EventLog ;
- clé privée dans les ressources Web ou sur une SD distribuée ;
- identifiant universel partagé entre appareils ;
- clé API exposée dans une URL journalisée ou un diagnostic.

## Contrats automatisables

La commande de référence est :

```powershell
python -m unittest discover -s tests/contracts -p "test_*.py" -v
```

Le workflow `.github/workflows/security-contracts.yml` exécute ces contrôles sur les pull requests et sur `main`.

Les contrats actifs couvrent notamment :

- traversées de chemin SD ;
- exclusion des routes `/api/` du handler statique ;
- absence de cache sur les diagnostics de stockage ;
- timeout borné de la météo ;
- déclaration POST JSON des routes sensibles.

Les tests `expectedFailure` représentent des dettes confirmées, pas des succès : mot de passe Wi-Fi journalisé, AP ouvert et météo HTTP. Ils doivent devenir bloquants dans le commit qui corrige le firmware.

## Revue à chaque checkpoint

- exécuter les contrats statiques ;
- compiler les environnements impactés ;
- vérifier les nouvelles interfaces et entrées ;
- rechercher les secrets dans diff, logs et artefacts ;
- réviser les risques concernés ;
- documenter les tests négatifs et modes dégradés ;
- mettre à jour `35_CODE_TRACEABILITY_REGISTER.md` et `37_SECURITY_CONTRACTS_AND_CI.md`.

## Réponse à incident

1. détecter et qualifier ;
2. confiner l’équipement ou le service ;
3. révoquer ou faire tourner les secrets ;
4. préserver les preuves ;
5. restaurer depuis un état validé ;
6. vérifier les fonctions locales et sécurités relais ;
7. mettre à jour risques, contrats et documentation.

La mise en sécurité des relais et la continuité locale restent prioritaires.

## Critères de fermeture d’un risque

Un risque n’est fermé qu’après :

- implémentation ;
- test nominal et négatif ;
- documentation ;
- observabilité ;
- récupération testée ;
- preuve archivée dans un checkpoint.

Un `expectedFailure` encore présent interdit de déclarer le risque correspondant fermé.

## Risques prioritaires

- `SEC-001` : authentification Web ;
- `SEC-002` : point d’accès captif ;
- `SEC-006` et `SEC-011` : secrets et journaux ;
- `SEC-009` : dépendances vulnérables ;
- `SEC-010` : épuisement des ressources ;
- `SEC-012` : récupération utilisée comme porte dérobée ;
- `SEC-017` : révocation ;
- `SEC-018` : restauration.

## Invariants

- `INV-OPS-SEC-001` : un incident distant ne désactive pas les sécurités locales.
- `INV-OPS-SEC-002` : toute compromission présumée déclenche une rotation ou révocation adaptée.
- `INV-OPS-SEC-003` : un risque n’est fermé qu’après implémentation, test, documentation, observabilité et récupération.
- `INV-OPS-SEC-004` : les archives de reprise excluent les secrets.
- `INV-OPS-SEC-005` : les contrats CI visibles ne sont pas supprimés pour masquer une dette.

## Références

- `19_HTTPS_AND_SESSIONS.md` ;
- `37_SECURITY_CONTRACTS_AND_CI.md` ;
- `docs/security/CYBERSECURITY_ARCHITECTURE.md` ;
- `docs/security/SECURITY_RISK_REGISTER.md` ;
- `docs/roadmap/CYBERSECURITY_LIFECYCLE.md`.

## Historique

### 1.1

Ajout des contrats CI, des dettes exécutables et des règles de fermeture associées.
