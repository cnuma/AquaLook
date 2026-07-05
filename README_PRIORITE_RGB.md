# Correctif de priorité RGB

Base contrôlée :
- dépôt : cnuma/AquaLook
- branche : refactor/eventlog-centralise
- dernier état utilisateur observé : 6e93fa2

## Cause

`FaultManager` écrivait directement sur les GPIO avec `digitalWrite()`, tandis que
`ScreenManager` pilotait les mêmes broches en PWM avec `ledcWrite()`.

Le rainbow réécrivait donc immédiatement la LED après le signal d'alarme.

## Architecture corrigée

`ScreenManager` devient l'unique propriétaire matériel des trois canaux PWM.

Ordre appliqué à chaque passage de `ScreenManager::update()` :

1. calcul de la couleur fonctionnelle normale ;
2. appel de `FaultManager::resolveColor()` ;
3. application de la priorité :
   - erreur non acquittée : rouge 500 ms / éteint 500 ms ;
   - défaut actif acquitté : couleur normale avec rappel rouge 300 ms toutes les 5 s ;
   - aucun défaut : couleur normale ;
4. une seule écriture physique PWM via `renderLed()`.

Cette écriture est effectuée même lorsque l'écran est réveillé, afin que l'alarme
reste visible hors veille.

## Fichiers à remplacer

- src/FaultManager.h
- src/FaultManager.cpp
- src/ScreenManager.h
- src/ScreenManager.cpp

## Compilation

Non compilé par l'assistant.

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t upload
```

Aucun fichier LittleFS n'est modifié.
