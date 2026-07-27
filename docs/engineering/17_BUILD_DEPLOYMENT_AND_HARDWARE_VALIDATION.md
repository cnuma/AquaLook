# AquaLook Engineering Reference — Compilation, déploiement et validation matérielle

- Version documentaire : 1.0
- Statut : référence initiale
- Dernière consolidation : 2026-07-27
- Sources : checkpoint `CHECKPOINT_2026-07-13_MAIN_STEP6_CLOSED.md`, `AGENTS.md`, `platformio.ini`
- Composants : PlatformIO, environnements legacy et V4, upload série, buildfs, tests matériels
- Maturité : D3

## Objet

Ce document définit la chaîne de compilation, d’upload et de validation utilisée pour reprendre AquaLook depuis le checkpoint de clôture de l’étape 6.

## Préconditions

- dépôt synchronisé sur la branche ciblée ;
- état Git propre ;
- PlatformIO installé ;
- carte ESP32 connectée ;
- port série identifié ;
- `platformio.ini` du commit ciblé présent ;
- aucun secret ou fichier local non versionné utilisé comme dépendance implicite.

## Synchronisation Git

```powershell
git switch main
git fetch --prune
git pull --ff-only origin main
git status
git log -3 --oneline
```

État attendu : branche à jour et répertoire de travail propre.

## Environnements validés

Le checkpoint confirme :

- compilation de `ProgrammeArrosage_legacy` ;
- compilation et upload de `ProgrammeArrosage_v4` ;
- essais matériels après upload.

Les noms exacts des environnements sont ceux de `platformio.ini`.

## Commandes de référence

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Le port `COM3` est un exemple issu du checkpoint. Il doit être remplacé par le port réel de la machine.

La commande d’upload compile déjà l’environnement V4. Une compilation V4 séparée avant l’upload n’est pas requise pour cette procédure.

## Ressources LittleFS

Après toute modification du dossier `data/` :

```powershell
pio run -e ProgrammeArrosage -t buildfs
```

L’environnement exact utilisé pour `buildfs` doit être confirmé dans `platformio.ini`. Le résultat doit être consigné dans le bilan de livraison.

## Validation fonctionnelle

Une compilation réussie ne suffit pas. La validation couvre toute la chaîne :

1. inclusion et instanciation ;
2. appel réel depuis `setup()`, `loop()`, tâche, callback ou route ;
3. conditions d’entrée ;
4. effet observable ;
5. absence d’annulation par un cache, redraw ou rechargement ;
6. vérification de tous les modes concernés.

## Validation matérielle minimale

- démarrage sans activation intempestive ;
- écran fonctionnel ;
- tactile fonctionnel ;
- serveur Web accessible ;
- NTP fonctionnel lorsque le réseau est disponible ;
- relais testés sur une seule zone, durée courte et sous surveillance ;
- ressources SD et fallbacks vérifiés ;
- EventLog et profiler observés ;
- aucune régression du fallback legacy.

## Tests ciblés

Les environnements ou commandes dédiés présents dans le dépôt sont utilisés lorsque le périmètre le nécessite, notamment :

- calibration tactile ;
- test relais ;
- diagnostic stockage ;
- suivi série du boot et du Runtime.

Les noms exacts des environnements doivent être extraits de `platformio.ini`.

## Critères de livraison

- `git diff --check` propre ;
- compilation réussie des environnements concernés ;
- `buildfs` réussi si `data/` a changé ;
- validation matérielle réalisée ou explicitement marquée non réalisée ;
- diff final relu ;
- aucune duplication HTML, CSS ou JavaScript ;
- aucune route ou structure persistée modifiée sans décision ;
- documentation et checkpoint consolidés.

## Gestion des échecs

Un échec de compilation, upload, montage SD, tactile ou relais est indiqué sans ambiguïté. Il n’est jamais remplacé par une supposition de réussite.

Lorsque Windows ou OneDrive bloque un changement de branche ou la suppression de dossiers temporaires :

1. interrompre la boucle de nouvelle tentative ;
2. fermer les fichiers dans VS Code et l’Explorateur ;
3. renommer temporairement le dossier bloqué ;
4. refaire le changement de branche ;
5. supprimer ensuite les sauvegardes temporaires.

## Invariants

### INV-BLD-001

Aucun code non compilé n’est poussé comme état validé.

### INV-BLD-002

Un test matériel n’est déclaré réussi qu’après observation réelle.

### INV-BLD-003

Une modification de `data/` impose `buildfs`.

### INV-BLD-004

La branche `main` reste stable ; les évolutions utilisent une branche dédiée.

### INV-BLD-005

Le checkpoint référence le commit exact utilisé pour compiler et tester.

## Références

- `docs/checkpoints/CHECKPOINT_2026-07-13_MAIN_STEP6_CLOSED.md` ;
- `AGENTS.md` — procédure avant livraison ;
- `platformio.ini` ;
- `11_CHECKPOINT_CONSOLIDATION.md`.

## Historique

### 1.0

Première consolidation de la chaîne de compilation, d’upload et de validation matérielle.