# AquaLook V4 — Phase 6 — Run 6.4

## Profils compile-time legacy / V4

**Date :** 10 juillet 2026  
**Statut :** implémenté, compilation à valider

## Objectif

Ajouter deux profils PlatformIO explicites permettant de distinguer le backend historique du backend V4 au moment de la compilation, sans activer le backend V4 dans le runtime et sans migrer de zone.

## Profils disponibles

### Profil nominal historique

```ini
[env:ProgrammeArrosage]
```

Macros :

```text
AQUALOOK_RELAY_BACKEND_LEGACY=1
AQUALOOK_RELAY_BACKEND_V4=0
```

Ce profil reste la référence nominale et conserve le comportement validé des Runs 4.11 à 4.13.

### Alias historique explicite

```ini
[env:ProgrammeArrosage_legacy]
extends = env:ProgrammeArrosage
```

Cet environnement permet de demander explicitement une compilation legacy sans dupliquer la configuration matérielle, les bibliothèques ou les filtres de sources.

### Profil V4 expérimental

```ini
[env:ProgrammeArrosage_v4]
extends = env:ProgrammeArrosage
```

Macros :

```text
AQUALOOK_RELAY_BACKEND_LEGACY=0
AQUALOOK_RELAY_BACKEND_V4=1
```

Les macros héritées du profil nominal sont retirées avec `build_unflags` avant d’ajouter les valeurs V4.

## Garantie d’inertie

Le profil `ProgrammeArrosage_v4` ne suffit pas à activer le backend physique V4.

Le Run 6.4 ne modifie pas :

```text
src/main.cpp
EquipmentOutputRuntimeAdapter
RelaisManagerBackend
NVS
Web
LCD
JSON
```

`V4RelayPhysicalBackend` n’est pas instancié dans `main.cpp` et son masque `_migratedZoneMask` reste initialisé à zéro sans API publique d’activation.

Par conséquent, les trois environnements compilent encore le même chemin runtime effectif :

```text
EquipmentOutputRuntimeAdapter
  -> RelaisManagerBackend
  -> RelaisManager
```

avec le fallback direct `RelaisManager` conservé.

## Position précise de la modification

Fichier :

```text
platformio.ini
```

Modifications :

1. Dans `[env:ProgrammeArrosage]`, au début de `build_flags`, ajout des macros legacy explicites.
2. Après la fin de `[env:ProgrammeArrosage]` et avant `[common]`, ajout de `[env:ProgrammeArrosage_legacy]`.
3. À la même position, ajout de `[env:ProgrammeArrosage_v4]` avec `build_unflags` et macros V4.

## Commandes de validation

PowerShell utilisé par l’utilisateur ne prenant pas en charge `&&`, utiliser des commandes séquentielles.

### Validation nominale et test matériel

```powershell
git pull --ff-only
pio run -e ProgrammeArrosage -t upload
if ($LASTEXITCODE -eq 0) {
    pio device monitor -e ProgrammeArrosage
}
```

### Validation du profil legacy explicite

```powershell
pio run -e ProgrammeArrosage_legacy
```

### Validation du profil V4 passif

```powershell
pio run -e ProgrammeArrosage_v4
```

Le profil V4 ne doit pas être téléversé pour valider ce run, car aucune différence runtime n’est encore attendue. Une compilation réussie suffit à vérifier la cohérence des macros et de l’héritage PlatformIO.

## Critères de réussite

- les trois environnements compilent ;
- aucune redéfinition de macro backend n’est signalée ;
- le profil nominal se téléverse et démarre normalement ;
- aucune activation inattendue de relais ;
- Web et LCD restent inchangés ;
- les métriques RAM/Flash sont relevées pour comparaison.

## Étape suivante

```text
AquaLook V4 — Phase 6 — Run 6.5
Activation contrôlée d’une zone pilote
```

Cette activation ne devra être engagée qu’après validation des trois profils du Run 6.4.