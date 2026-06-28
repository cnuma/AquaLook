# 10 — Transmission de tâches à Codex

## Format de mission

Chaque tâche doit préciser :

```text
Titre :
Branche de base :
Commit de base :
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
Titre : Ajouter la lecture de débit via un ESP32-S2 secondaire
Branche de base : main
Commit de base : a2cf490
Objectif : recevoir les compteurs par I²C depuis un Lolin S2 Mini
Hors périmètre : ne pas modifier le pilotage relais
Fichiers pressentis : nouveau FlowManager, WebManager, ConfigManager
Invariants : I1, I2, I6, I18, I20
Critères d’acceptation : aucune perte de comptage, pas de blocage de loop, affichage Web, persistance des facteurs de calibration
Commandes : compilation firmware et buildfs
Tests matériels : 1 capteur, 4 capteurs, déconnexion du secondaire
Livrables : sources, documentation, checkpoint de branche
```

## Réponse attendue de Codex avant code

1. compréhension de l’objectif ;
2. base et branche ;
3. fichiers concernés ;
4. risques ;
5. invariants ;
6. plan de modification ;
7. plan de test.

## Réponse attendue après code

```text
Base utilisée :
Fichiers modifiés :
Fonctions modifiées :
Fichiers non modifiés :
Diff hors périmètre :
Compilation :
LittleFS :
Tests Web :
Tests LCD :
Tests matériels :
Risques résiduels :
Commit proposé :
Checkpoint proposé :
```

## Interdictions de transmission

Ne pas demander une refonte, une optimisation, une sécurisation, une modification du planning ou un passage à 16 zones sans critères précis et revue d’architecture.

## Définition de terminé

Une tâche est terminée quand les critères sont couverts, les tests requis sont exécutés ou explicitement laissés à faire, le diff est limité, la documentation est à jour, le dépôt est propre et un checkpoint est créé si nécessaire.
