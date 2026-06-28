# 06 — Protocole anti-régression

## Principe

La dernière version complète du dépôt ou le dernier checkpoint explicitement validé est la seule base autorisée.

## Avant modification

Documenter branche, commit, checkpoint éventuel, fichiers et fonctions concernés, invariants, comportement actuel et comportement attendu.

## Pendant la modification

- petites étapes ;
- aucun changement opportuniste ;
- pas de reformatage global ;
- pas de duplication ;
- sauvegardes hors du dépôt ou hors `data/` ;
- contrôle fréquent du diff.

## Contrôle frontend

Vérifier IDs dupliqués, fonctions JS dupliquées, blocs `<style>` ou `<script>` ajoutés plusieurs fois et hausse de taille.

Mesurer :

```powershell
Get-ChildItem .\data -File |
  Select-Object Name,Length |
  Sort-Object Length -Descending
```

## Contrôle source

- pas de second `LittleFS.begin()` ;
- pas d’accès matériel direct depuis ScheduleManager ;
- pas de nouveau stockage persistant hors ConfigManager ;
- pas de blocage long dans `loop()` ;
- pas de secret compilé en dur ;
- pas de changement de broche non documenté.

## Après modification

```powershell
git diff --check
pio run -e ProgrammeArrosage
```

Si `data/` a changé :

```powershell
pio run -e ProgrammeArrosage -t buildfs
```

## Revue du diff

Répondre aux questions :

1. Chaque ligne modifiée est-elle nécessaire ?
2. Un ID ou une route a-t-il changé ?
3. Une structure persistée a-t-elle changé ?
4. LittleFS a-t-il grandi ?
5. Le comportement relais est-il affecté ?
6. Un test matériel est-il requis ?
7. La documentation doit-elle être mise à jour ?

## Stop conditions

Arrêter et revenir à la base si : patch corrompu, transformation ambiguë, ancre non trouvée, occurrences inattendues, buildfs saturé, compilation cassée hors périmètre, différence de taille inexpliquée ou fichier entier remplacé sans justification.

## Livraison

Le bilan contient base utilisée, fichiers modifiés et non modifiés, compilation, buildfs, tests Web/LCD/matériels, risques résiduels, commit et nom du checkpoint.
