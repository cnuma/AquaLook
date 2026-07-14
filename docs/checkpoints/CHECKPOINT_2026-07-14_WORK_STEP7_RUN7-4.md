# CHECKPOINT — AquaLook — RUN7.4 — exécution contrôlée de l’orchestrateur

Date : 14 juillet 2026

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Base validée : branche `work/step7-run7-3`
- Commit RUN7.3 : `578b9a60d74b53a7a20dd4bb7914410a1c7a6328`
- Branche de travail : `work/step7-run7-4`

## Objectif

Étendre le banc isolé afin de couvrir l’API d’exécution contrôlée introduite dans RUN7.3, sans brancher `EquipmentOrchestrator` dans `main.cpp` et sans commander de relais.

## Scénarios couverts

1. exécution demandée avant initialisation : aucune tentative ;
2. zone hors limites : aucune tentative ;
3. plan rejeté par `EquipmentManager` : aucune tentative ;
4. plan valide sans exécuteur : tentative explicite puis retour `ACTION_EXECUTOR_NOT_CONNECTED` ;
5. conservation de l’aperçu validé dans le résultat d’exécution ;
6. vérification séparée des chemins démarrage et arrêt.

## Fichier modifié

- `tools/run7-2/test_equipment_orchestrator.cpp`

Le banc historique RUN7.2 est conservé au même emplacement et devient le banc cumulatif de la Phase 7.

## Invariants préservés

1. Aucun changement dans `main.cpp`.
2. Aucun changement dans `ScheduleManager`.
3. Aucun changement NVS ou `ConfigManager`.
4. Aucun changement de `RelaisManager`, des drivers ou du backend physique.
5. Aucun exécuteur réel n’est connecté dans le banc.
6. Aucun accès I2C et aucune commande de relais.
7. Le fallback legacy reste inchangé.
8. La branche `dev/log-timestamps-ntp` reste indépendante.

## Validation complète à réaliser

### Synchronisation

```powershell
git fetch --prune
git switch work/step7-run7-4
git pull --ff-only
```

### Banc isolé RUN7.4

```powershell
pio run -c platformio.run7-2.ini -e test_equipment_orchestrator -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Résultat attendu :

```text
AquaLook V4 - RUN7.4 - Controlled orchestrator execution bench
RESULT: passed=... failed=0 status=SUCCESS
```

### Non-régression AquaLook complète

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Ne pas lancer séparément `pio run -e ProgrammeArrosage_v4` : la commande d’upload compile déjà cet environnement.

## Statut

Le code et le banc sont préparés dans GitHub. La compilation, l’upload et le monitoring ne seront déclarés réussis qu’après validation sur le poste et la carte AquaLook.

## Étape suivante proposée — RUN7.5

Préparer le branchement runtime de l’orchestrateur derrière un mode shadow ou un commutateur explicite, afin de comparer sa décision avec le chemin `EquipmentManager` actuel avant toute prise d’autorité réelle.
