# AquaLook Firmware — ScheduleManager

- Référence : FW-002
- Statut : relié au code
- Maturité : D4
- Sources : `src/ScheduleManager.h`, `src/ScheduleManager.cpp`, `src/main.cpp`

## Mission

`ScheduleManager` porte la planification locale des zones, le déclenchement, la fin d’exécution et le mode manuel. Il ne pilote pas directement le matériel.

## Structures principales

- `TimeSlot` : heure, minute, durée et activation ;
- `DaySchedule` : créneaux d’une journée ;
- `ZoneSchedule` : programmation hebdomadaire et par intervalle ;
- `ActiveSlot` : état d’une exécution en cours.

## API et interactions

L’API exacte est déclarée dans `ScheduleManager.h`. Les usages structurants sont :

- initialisation depuis la configuration ;
- `update()` depuis la boucle principale ;
- lecture et modification des programmes par zone ;
- durée manuelle ;
- callback de demande relais câblé par `setRelayCallback(onRelayRequest)`.

## Flux nominal

```text
NTP / temps local
  -> ScheduleManager::update()
  -> validation du créneau
  -> création de l’état actif
  -> callback onRelayRequest(zone, état)
  -> modèle d’équipements et backend
```

## Invariants

- le Scheduler ne dépend pas d’une adresse I2C ;
- une zone invalide n’est jamais activée ;
- le fonctionnement local continue sans Internet ;
- l’absence de synchronisation NTP ne doit pas provoquer une activation arbitraire ;
- la durée maximale système reste une barrière de sécurité ;
- le callback matériel doit rester court et déterministe.

## Persistance

La persistance appartient à `ConfigManager`. Lors d’une sauvegarde Web, l’état de programmation est recopié vers la configuration avant écriture.

## Points d’extension

- nouveau type de déclenchement : ajouter un modèle explicite, ses validations et sa migration ;
- nouvelle dépendance d’équipement : passer par `EquipmentManager` ;
- nouvelle règle météo : distinguer recommandation, inhibition et décision opérateur ;
- nouveau mode manuel : conserver les mêmes sécurités de durée et d’arrêt.

## Tests requis

- déclenchement et fin d’un créneau ;
- chevauchements et limites de journée ;
- reboot et relecture de configuration ;
- absence de NTP ;
- mode manuel ;
- zone hors limites ;
- backend legacy et V4 ;
- sécurité de durée maximale.

## Références

- `docs/engineering/06_SCHEDULER.md`
- `docs/engineering/08_RELAY_AND_EQUIPMENT_CONTROL.md`
- `docs/engineering/30_TEST_AND_ANTI_REGRESSION_MATRIX.md`
