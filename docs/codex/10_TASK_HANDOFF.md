# 10 — Transmission de tâches à Codex

## Référence de reprise courante

Pour toute tâche relative à l’OTA après le 30 juillet 2026, utiliser :

```text
Branche : agent/ota-3.0-download-test-v591
Checkpoint : docs/checkpoints/CHECKPOINT_2026-07-30_OTA-3.0_DOWNLOAD_VERIFIED.md
Document technique : docs/codex/11_OTA_3_DOWNLOAD_VALIDATION.md
Commit code validé avant documentation : 6808f58bb0f386a17a2c24d5bb25fe0500410d43
```

Avant toute proposition, l’agent doit lire `AGENTS.md`, `docs/codex/00_CONTEXT.md`, le document OTA et le checkpoint. Un résumé de chat ne remplace jamais ces fichiers.

## Format de mission

Chaque tâche doit préciser :

```text
Titre :
Branche de base :
Commit de base :
Checkpoint applicable :
Documents obligatoires lus :
Objectif :
Hors périmètre :
Fichiers pressentis :
Invariants :
Critères d’acceptation :
Commandes de test :
Tests matériels :
Livrables :
```

## Exemple

```text
Titre : Préparer le contrat d’écriture de la partition OTA inactive
Branche de base : agent/ota-3.0-download-test-v591
Commit de base : checkpoint documentaire courant
Checkpoint applicable : CHECKPOINT_2026-07-30_OTA-3.0_DOWNLOAD_VERIFIED.md
Documents obligatoires lus : AGENTS.md, 00_CONTEXT.md, 03_INVARIANTS.md, 05_BUILD_AND_TEST.md, 11_OTA_3_DOWNLOAD_VALIDATION.md
Objectif : définir puis implémenter un palier d’écriture contrôlée sans activation immédiate
Hors périmètre : ne pas activer la nouvelle partition, ne pas supprimer le rollback, ne pas modifier les relais
Fichiers pressentis : MaintenanceBoot, nouveau composant de staging OTA, MaintenanceResult, WebManager
Invariants : aucune activation pendant arrosage, validation taille/SHA, Legacy compilable, V4 testé sur matériel
Critères d’acceptation : écriture partition inactive uniquement, contrôle final, erreur persistée, partition active inchangée
Commandes : compilation Legacy et V4, upload V4 sur port confirmé, monitoring
Tests matériels : téléchargement valide, coupure réseau, SHA invalide, taille invalide, redémarrage
Livrables : sources, documentation, checkpoint de branche
```

## Réponse attendue de Codex avant code

1. compréhension de l’objectif ;
2. base, branche, commit et checkpoint ;
3. confirmation des fichiers de gouvernance lus ;
4. fichiers concernés ;
5. risques ;
6. invariants ;
7. plan de modification ;
8. plan de test ;
9. confirmation que le port COM sera redemandé avant upload.

## Réponse attendue après code

```text
Base utilisée :
Checkpoint utilisé :
Fichiers de gouvernance lus :
Fichiers modifiés :
Positions et fonctions modifiées :
Fichiers non modifiés :
Diff hors périmètre :
Compilation Legacy :
Compilation V4 :
LittleFS :
Tests Web :
Tests LCD :
Tests matériels :
Port COM confirmé :
Résultat observé :
Risques résiduels :
Commit proposé :
Checkpoint proposé :
```

## Règles OTA spécifiques

- Ne jamais reconstruire `MaintenanceBoot.cpp`, `WebManager.h` ou `OtaDownloadTest.cpp` depuis un extrait de conversation.
- Vérifier le contenu réel de la branche avant modification.
- Ne pas présenter `setInsecure()` comme une validation TLS de production.
- Ne pas appeler l’API `Update` ni écrire une partition sans décision explicite du palier.
- Ne pas confondre téléchargement validé et installation OTA validée.
- Compiler Legacy et V4 après toute modification commune.
- Tester le chemin V4 sur matériel.
- Regrouper les commandes Git, build, upload et monitor dans un seul bloc continu.
- Ne jamais inclure une commande qui ferme le terminal en cas d’erreur.

## Interdictions de transmission

Ne pas demander une refonte, une optimisation, une sécurisation, une modification du planning, un passage à 16 zones ou une installation OTA complète sans critères précis et revue d’architecture.

## Définition de terminé

Une tâche est terminée quand les critères sont couverts, les tests requis sont exécutés ou explicitement laissés à faire, le diff est limité, les positions modifiées sont documentées, la documentation est à jour, le dépôt est propre et un checkpoint est créé si nécessaire.
