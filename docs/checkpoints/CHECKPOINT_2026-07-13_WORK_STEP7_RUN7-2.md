# CHECKPOINT — AquaLook — RUN7.2 — banc passif de l’orchestrateur

Date : 13 juillet 2026

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Base validée : branche `work/step7-run7-1`
- Commit RUN7.1 validé : `9b57118343d9184627940b9999e3ea534f44386f`
- Branche de travail : `work/step7-run7-2`

## Objectif

Valider sur un banc PlatformIO isolé le contrat passif de `EquipmentOrchestrator`, sans brancher l’orchestrateur dans `main.cpp` et sans commander de relais.

## Scénarios couverts

1. orchestrateur non initialisé ;
2. zone hors limites ;
3. aperçu démarrage et arrêt d’une vanne sans pompe ;
4. aperçu démarrage et arrêt avec dépendance pompe et temporisations ;
5. propagation d’un rejet produit par `EquipmentManager`.

## Fichiers concernés

- `tools/run7-2/test_equipment_orchestrator.cpp` : banc déterministe isolé des sources nominales ;
- `platformio.run7-2.ini` : configuration PlatformIO dédiée au banc ;
- ce checkpoint.

Le fichier `platformio.ini` nominal n’est volontairement pas modifié.

## Invariants préservés

1. Aucun changement dans `main.cpp`.
2. Aucun changement dans `ScheduleManager`.
3. Aucun changement NVS ou `ConfigManager`.
4. Aucun changement de `RelaisManager`, des drivers ou du backend physique.
5. Aucun appel à `EquipmentManager::startZone()` ou `stopZone()`.
6. Aucun accès I2C et aucune commande de relais par le banc.
7. Le fallback legacy reste inchangé.
8. Les environnements nominaux ne compilent pas le fichier du banc RUN7.2.

## Validation à réaliser

### Synchronisation

```powershell
git fetch --prune
git switch work/step7-run7-2
git pull --ff-only
```

### Banc RUN7.2

Le banc utilise son fichier de configuration PlatformIO dédié :

```powershell
pio run -c platformio.run7-2.ini -e test_equipment_orchestrator -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Résultat attendu dans le monitor :

```text
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

Le code et le banc sont préparés dans GitHub. La compilation, l’upload et les résultats du monitor ne sont pas déclarés réussis tant qu’ils n’ont pas été exécutés sur le poste et la carte AquaLook.
