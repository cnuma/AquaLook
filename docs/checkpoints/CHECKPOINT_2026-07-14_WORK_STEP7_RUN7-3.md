# CHECKPOINT — AquaLook — RUN7.3 — contrat d’exécution contrôlée

Date : 14 juillet 2026

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Base validée : `work/step7-run7-2`
- Commit de départ : `ebcfcd40a45c0324d7d0c03f27fce781e00a4998`
- Branche de travail : `work/step7-run7-3`

## Objectif

Faire évoluer `EquipmentOrchestrator` d’un contrat strictement passif vers une API d’exécution contrôlée, sans le brancher dans `main.cpp` et sans modifier le runtime AquaLook validé.

## Réalisation

Ajout d’un résultat d’exécution contenant :

- l’aperçu du plan validé avant exécution ;
- le résultat retourné par `EquipmentManager` ;
- l’indication explicite qu’une exécution a réellement été tentée ;
- une méthode `success()` qui n’est vraie que si l’action a été exécutée et acceptée.

Nouvelles API :

- `executeStartZone(zone)` ;
- `executeStopZone(zone)`.

L’orchestrateur refuse toute exécution lorsque :

- il n’est pas initialisé ;
- la zone est hors limites ;
- le plan construit par `EquipmentManager` est rejeté.

## Fichiers modifiés

- `src/EquipmentOrchestrator.h`
- `src/EquipmentOrchestrator.cpp`
- ce checkpoint

## Invariants préservés

1. Aucun changement dans `main.cpp`.
2. Aucun changement dans `ScheduleManager`.
3. Aucun changement NVS ou `ConfigManager`.
4. Aucun changement dans `RelaisManager`, les drivers ou la topologie.
5. Aucun changement du backend physique ou du fallback legacy.
6. L’orchestrateur n’est pas encore utilisé par le runtime nominal.
7. La branche `dev/log-timestamps-ntp` reste indépendante de la Phase 7.

## Validation complète à réaliser

### Banc RUN7.2 existant

Le banc devra être étendu lors du run suivant pour couvrir les nouvelles méthodes d’exécution. Le banc passif existant doit néanmoins continuer à compiler :

```powershell
pio run -c platformio.run7-2.ini -e test_equipment_orchestrator -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

### Non-régression AquaLook

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Ne pas lancer séparément `pio run -e ProgrammeArrosage_v4` : la commande d’upload compile déjà cet environnement.

## Statut

Le code est préparé dans GitHub. Compilation, upload et validation du monitor restent à effectuer sur le poste et la carte AquaLook avant clôture de RUN7.3.

## Étape suivante proposée — RUN7.4

Étendre le banc isolé afin de valider :

- refus d’exécution sans initialisation ;
- refus d’une zone invalide ;
- exécution démarrage/arrêt vanne seule ;
- propagation des erreurs d’exécution ;
- absence d’appel à `EquipmentManager` lorsque l’aperçu est rejeté.
