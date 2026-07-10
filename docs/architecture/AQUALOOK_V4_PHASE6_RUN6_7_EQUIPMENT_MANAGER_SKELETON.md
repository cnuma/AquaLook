# AquaLook V4 — Phase 6 — Run 6.7

## Objet

Consolider le `EquipmentManager` existant comme point d'orchestration futur entre le planning et les sorties physiques, sans modifier le comportement d'arrosage actuel.

## Base

- branche : `feature/aqualook-v4-domain` ;
- base du run : `31ab6f151d79a0786647c9ae3adbf8226ce9972c` ;
- Run 6.6 compilé avec succès sur `ProgrammeArrosage_legacy`.

## Constat

Le dépôt contenait déjà :

- `EquipmentModel` ;
- `EquipmentManager` ;
- les liens zone -> vanne -> pompe ;
- la résolution des affectations relais ;
- l'exécution historique via `RelaisManager::setAssignment()`.

Il ne fallait donc pas créer un second manager V4.

## Modification

`EquipmentManager` accepte désormais un `EquipmentOutputRuntimeAdapter` optionnel.

Ordre d'exécution préparé :

```text
startZone / stopZone
    -> validation du modèle et du mapping
    -> adaptateur de sortie V4 si connecté
    -> fallback RelaisManager::setAssignment
```

L'adaptateur V4 conserve lui-même son fallback interne vers `RelaisManager` lorsqu'il est configuré ainsi.

## API ajoutée

```cpp
void setOutputAdapter(EquipmentOutputRuntimeAdapter* outputAdapter);
bool hasOutputAdapter() const;
bool hasExecutor() const;
```

## Hors périmètre

Le Run 6.7 ne modifie pas :

- `main.cpp` ;
- le callback actuel de `ScheduleManager` ;
- la NVS ;
- `ConfigManager` ;
- les délais pompe ;
- la séquence pompe / électrovanne ;
- le Web ;
- le LCD ;
- le nombre de zones migrées V4.

## Conséquence runtime

Aucune instance existante ne connecte encore l'adaptateur V4 au `EquipmentManager`.

Le chemin d'arrosage utilisé en production reste donc inchangé après ce run.

## Validation attendue

Compiler les deux profils :

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4
```

Aucun upload n'est requis pour ce run sans raccordement runtime.

## Suite proposée

Run 6.8 : raccorder un chemin pilote contrôlé vers `EquipmentManager`, avec une seule zone V4 et fallback immédiat, sans encore orchestrer la pompe.
