# 10 — Transmission de tâches à Codex

## Référence de reprise courante

La référence est toujours `main` et le checkpoint le plus récent de
`docs/checkpoints/` (point d’entrée : `docs/REPRISE_INGENIEUR.md`). Ce
document ne fige plus de branche ni de commit : l’ancienne référence OTA
(`agent/ota-3.0-download-test-v591`, 30 juillet 2026) est historique.

Avant toute proposition, l’agent doit lire `AGENTS.md`, `docs/codex/00_CONTEXT.md`, le checkpoint et les documents propres à la tâche. Un résumé de chat ne remplace jamais ces fichiers.

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
Branche de base : main
Commit de base : checkpoint documentaire courant
Checkpoint applicable : le plus récent de docs/checkpoints/
Documents obligatoires lus : AGENTS.md, 00_CONTEXT.md, 03_INVARIANTS.md, 05_BUILD_AND_TEST.md, 11_OTA_3_DOWNLOAD_VALIDATION.md
Objectif : définir puis implémenter un palier d’écriture contrôlée sans activation immédiate
Hors périmètre : ne pas activer la nouvelle partition, ne pas supprimer le rollback, ne pas modifier les relais
Fichiers pressentis : MaintenanceBoot, nouveau composant de staging OTA, MaintenanceResult, WebManager
Invariants : aucune activation pendant arrosage, validation taille/SHA, testé sur .141
Critères d’acceptation : écriture partition inactive uniquement, contrôle final, erreur persistée, partition active inchangée
Commandes : upload ProgrammeArrosage_s3 sur port confirmé, monitoring
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
Compilation ProgrammeArrosage_s3 :
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
- Compiler `ProgrammeArrosage_s3` après toute modification.
- Tester le chemin sur `.141`.
- Regrouper les commandes Git, build, upload et monitor dans un seul bloc continu.
- Ne jamais inclure une commande qui ferme le terminal en cas d’erreur.

## Interdictions de transmission

Ne pas demander une refonte, une optimisation, une sécurisation, une modification du planning, un passage à 16 zones ou une installation OTA complète sans critères précis et revue d’architecture.

## Définition de terminé

Une tâche est terminée quand les critères sont couverts, les tests requis sont exécutés ou explicitement laissés à faire, le diff est limité, les positions modifiées sont documentées, la documentation est à jour, le dépôt est propre et un checkpoint est créé si nécessaire.
