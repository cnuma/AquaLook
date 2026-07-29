# AquaLook Developer Guide — Architecture du projet

- Référence : DEV-001
- Statut : actif
- Maturité : D4

## Lire le dépôt

Ordre recommandé :

1. `docs/START_HERE.md` ;
2. `docs/engineering/00_MANUAL_INDEX.md` ;
3. `docs/engineering/01_PROJECT_STATUS.md` ;
4. tome Engineering du domaine ;
5. fiche `docs/firmware/` du composant ;
6. code source et tests ;
7. dernier checkpoint pertinent.

## Couches

```text
Interfaces Web / écran
        ↓
Managers applicatifs
        ↓
Scheduler et modèle d’équipements
        ↓
Adaptateurs Runtime
        ↓
Backends matériels
        ↓
GPIO / I2C / stockage / réseau
```

## Propriétaires principaux

- `ConfigManager` : configuration et persistance ;
- `ScheduleManager` : planification ;
- `EquipmentManager` : résolution des équipements et plans ;
- `RelaisManager` : sorties physiques historiques ;
- `DisplayManager` : écran et tactile ;
- `StorageManager` : SD ;
- `WiFiManager` : états STA/AP ;
- `WebManager` : routes et actions différées ;
- `EventLog` / `FaultManager` : événements et défauts ;
- `main.cpp` : composition et orchestration.

## Règles structurantes

- aucune logique métier dans les drivers ;
- aucune adresse matérielle dans le Scheduler ;
- aucune écriture Flash longue dans un callback AsyncTCP ;
- aucune dépendance Internet pour l’arrosage local ;
- conserver legacy et V4 tant que la migration n’est pas clôturée ;
- le code du commit ciblé prime sur les documents ;
- toute évolution met à jour Engineering, Firmware, Developer si nécessaire et le checkpoint.

## Profils de compilation

Les environnements réels sont définis dans `platformio.ini`. Ne jamais déduire un profil à partir de son nom uniquement. Distinguer firmware nominal, profil expérimental et banc ciblé.

## Références

- `docs/engineering/02_SYSTEM_OVERVIEW.md`
- `docs/engineering/17_BUILD_DEPLOYMENT_AND_HARDWARE_VALIDATION.md`
- `docs/engineering/35_CODE_TRACEABILITY_REGISTER.md`
