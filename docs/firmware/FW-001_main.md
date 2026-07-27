# AquaLook Firmware — `main.cpp`

- Référence : FW-001
- Statut : relié au code
- Maturité : D4
- Sources : `src/main.cpp`, `platformio.ini`, Engineering Reference 15 et 35

## Mission

`src/main.cpp` constitue le point de composition du firmware. Il instancie les managers, câble leurs dépendances, exécute la séquence de démarrage et ordonnance la boucle principale.

## Responsabilités

- construire les composants globaux ;
- initialiser stockage, configuration, affichage, relais, réseau, temps, météo et Web ;
- câbler `ScheduleManager::setRelayCallback(onRelayRequest)` ;
- enregistrer les handlers statiques SD avant le démarrage du serveur ;
- exécuter les mises à jour non bloquantes dans `loop()` ;
- alimenter `RuntimeProfiler` et les diagnostics ;
- appliquer les événements différés de l’`EventBus`.

## Chaîne de commande

```text
ScheduleManager
  -> onRelayRequest()
  -> EquipmentManager
  -> EquipmentOutputRuntimeAdapter
  -> backend sélectionné
  -> RelaisManager / bus I2C
```

## Séquence de boot

L’ordre exact du commit courant doit toujours être relu dans `setup()`. Les invariants sont :

1. montage et lecture de la configuration avant les consommateurs ;
2. initialisation sûre des sorties avant toute exécution ;
3. affichage du splash et progression de boot ;
4. démarrage réseau et services sans bloquer le Runtime ;
5. enregistrement des routes avant `_server.begin()` ;
6. aucune activation intempestive au démarrage.

## Boucle principale

`loop()` reste coopérative et non bloquante. Les traitements longs, notamment la météo, sont déportés. Chaque manager expose une méthode `update()` ou un mécanisme équivalent.

## Points d’extension

- ajouter un manager : instanciation, `begin()`, `update()`, diagnostics, documentation et test ;
- ajouter une dépendance : injection explicite plutôt qu’accès global caché ;
- ajouter un backend : conserver le fallback legacy tant que son retrait n’est pas validé ;
- ajouter une tâche : définir propriétaire, pile, priorité, synchronisation et mode d’arrêt.

## Risques

- changement d’ordre d’initialisation ;
- appels bloquants dans `loop()` ;
- double propriétaire d’une ressource matérielle ;
- callback déclenchant une écriture Flash ou une opération longue ;
- divergence entre profils legacy et V4.

## Validation

- compilation `ProgrammeArrosage_legacy` et `ProgrammeArrosage_v4` ;
- boot série complet ;
- absence d’activation relais au démarrage ;
- fonctionnement sans Wi-Fi ;
- profiler sans dépassement durable ;
- vérification du point d’appel de chaque nouveau composant.

## Références

- `docs/engineering/15_RUNTIME_AND_PROFILING.md`
- `docs/engineering/17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md`
- `docs/engineering/35_CODE_TRACEABILITY_REGISTER.md`
- `docs/developer/DEV-002_Creer_un_Manager.md`
