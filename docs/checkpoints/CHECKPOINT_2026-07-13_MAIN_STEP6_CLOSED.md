# CHECKPOINT — AquaLook — main — étape 6 clôturée

Date : 13 juillet 2026

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche de reprise : `main`
- Commit de clôture matériel validé avant ce checkpoint : `adc5ff6d9c7e3a2bf335fa5303755dfa068cc7a8`
- Ce fichier est ajouté directement sur `main` afin que la reprise se fasse sans branche de travail obligatoire.

## État Git validé

La branche `main` a été avancée en fast-forward depuis `work/run6-26-stage6-closeout`.

Comparaison effectuée avant mise à jour :

- `main` était l’ancêtre exact de la branche de clôture ;
- 446 commits d’avance sur la branche de clôture ;
- aucun commit divergent ;
- aucun historique réécrit.

Les anciennes branches `work/run6-*` peuvent être supprimées localement et à distance après vérification que `main` est bien à jour.

## État fonctionnel validé

L’étape 6 est clôturée.

Validations acquises :

- compilation `ProgrammeArrosage_legacy` réussie ;
- compilation et upload `ProgrammeArrosage_v4` réussis ;
- tests matériels réussis ;
- fonctionnement des relais validé ;
- backend V4 fonctionnel avec fallback legacy conservé ;
- modèle d’équipements transitoire opérationnel ;
- scénario pompe shadow passif disponible selon configuration ;
- météo non bloquante fonctionnelle ;
- synchronisation NTP fonctionnelle ;
- suppression du redraw complet lors de la synchronisation NTP ;
- affichage LCD fonctionnel ;
- tactile XPT2046 fonctionnel sur bus VSPI séparé ;
- initialisation SPI tactile corrigée ;
- warning APB parasite couvert pendant toute l’initialisation tactile ;
- serveur Web fonctionnel ;
- ressources Web SD avec fallback LittleFS ;
- logo servi depuis SD, LittleFS ou fallback SVG firmware ;
- journal EventLog horodaté ;
- profiler runtime qualifié en temps mural ;
- pauses proches de 136 ms identifiées comme potentiellement liées à l’ordonnanceur plutôt qu’au composant actif ;
- `yield()` exclu des alertes de lenteur métier tout en restant mesuré dans les diagnostics.

## Derniers runs de l’étape 6

### RUN6.22 — runtime non bloquant

- météo streaming ;
- réduction des redraws ;
- horodatage des logs ;
- amélioration de la lisibilité du profiler.

### RUN6.23 — initialisation SPI tactile

- restauration du niveau de log ESP déplacée après `_touch.begin(_touchSPI)` ;
- tactile validé matériellement.

### RUN6.24 — ressource logo Web

- aucune modification supplémentaire nécessaire ;
- fallback SD / LittleFS / SVG firmware déjà présent dans `SdStaticHandler`.

### RUN6.25 — profiler runtime

- mesures explicitement qualifiées comme temps mural ;
- détection des stalls probablement liés à l’ordonnanceur ;
- tests matériels validés.

### RUN6.26 — clôture de l’étape 6

- checkpoint matériel consolidé ;
- intégration complète sur `main` en fast-forward.

## Commandes de reprise locale

Depuis PowerShell :

```powershell
git switch main
git fetch --prune
git pull --ff-only origin main
git status
git log -3 --oneline
```

État attendu :

```text
On branch main
Your branch is up to date with 'origin/main'.
nothing to commit, working tree clean
```

## Compilation et test

Chaîne standard :

```powershell
pio run -e ProgrammeArrosage_legacy
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Ne pas lancer séparément `pio run -e ProgrammeArrosage_v4` avant l’upload : la commande d’upload compile déjà cet environnement.

## Nettoyage local éventuel

Lors du passage vers `main`, Windows/OneDrive peut empêcher Git de supprimer certains dossiers temporaires comme :

- `tools/patches`
- `tools/run6-22`

Procédure sûre :

1. répondre `n` à la boucle de nouvelle tentative ;
2. fermer les fichiers concernés dans VS Code et l’Explorateur ;
3. renommer temporairement le dossier bloqué ;
4. refaire `git switch main` ;
5. supprimer ensuite les dossiers `_backup` une fois `main` synchronisée.

## Invariants de reprise

1. La source de vérité est désormais `main` après synchronisation.
2. Toujours inspecter l’état réel du dépôt avant modification.
3. Modifier le minimum nécessaire.
4. Conserver les fallbacks legacy tant que leur suppression n’est pas explicitement validée.
5. Ne pas déclarer une compilation ou un test matériel réussi sans résultat réel.
6. Pour plus de deux fichiers modifiés, fournir directement les fichiers concernés.
7. Avant toute nouvelle étape importante, créer une branche dédiée depuis `main`.

## Prochaine étape

La prochaine séquence est l’étape 7.

Avant de démarrer :

```powershell
git switch main
git pull --ff-only origin main
git switch -c work/step7-run7-1
```

Le périmètre exact de RUN7.1 doit être défini à partir de la roadmap et de l’état réel du dépôt, sans réouvrir les points déjà validés durant l’étape 6.
