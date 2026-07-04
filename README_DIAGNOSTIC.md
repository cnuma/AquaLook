# AquaLook — diagnostic système et Web

## Source de vérité

- Dépôt : `cnuma/AquaLook`
- Branche de référence : `fix/encodage-web`
- Commit de référence : `5a656541428b0f201a5e0b623cacfd55fa6ce7dd`

## Fichiers nouveaux

- `src/SystemDiagnostics.h`
- `src/SystemDiagnostics.cpp`
- `data/diagnostic.html`

## Fichiers à modifier

- `src/main.cpp`
- `src/WebManager.h`
- `src/WebManager.cpp`
- `data/index.html`

Le fichier `AquaLook_diagnostic_systeme.patch` décrit précisément les insertions.

## Fonctionnement

La page est accessible à l’adresse :

`http://<IP_AQUALOOK>/diagnostic.html`

L’API JSON est accessible à :

`http://<IP_AQUALOOK>/api/diagnostics`

Actualisation de la page toutes les 2 secondes, sans écriture flash.

## Mesures fournies

- uptime, fréquence CPU, cœur de la boucle, cause du dernier reset ;
- heap libre, minimum observé, plus grand bloc contigu ;
- PSRAM et marge de pile de la tâche Arduino ;
- durée actuelle, moyenne et maximale d’un tour de boucle ;
- âge du dernier passage de boucle et détection d’un retard supérieur à 2 s ;
- compteurs et temps de génération des réponses JSON ;
- état WiFi, IP, RSSI, canal et MAC.

## Charge CPU

La charge détaillée par tâche/cœur n’est volontairement pas inventée.
Le champ `cpuStatsAvailable` indique si les statistiques d’exécution FreeRTOS
sont activées dans la configuration de compilation.

Dans la configuration actuelle, les indicateurs les plus utiles sont :

- `loop.ageMs`
- `loop.maxDurationUs`
- `loop.maxPeriodUs`
- `web.maxGenerationUs`
- `memory.heapLargestBlock`

## Compilation

Commandes prévues :

```powershell
pio run -e ProgrammeArrosage
pio run -e ProgrammeArrosage -t uploadfs
```

Le fichier `diagnostic.html` étant placé dans `data/`, il faut téléverser
LittleFS après compilation du firmware.

## Validation matérielle

1. Ouvrir `/diagnostic.html` dans un navigateur normal.
2. Ouvrir la même page en navigation privée.
3. Vérifier que `loop.ageMs` reste inférieur à 2 000 ms.
4. Relever `web.maxGenerationUs`.
5. Surveiller `heapMin` et `heapLargestBlock` pendant plusieurs heures.
6. Comparer le RSSI ; sous environ -75 dBm, le WiFi devient une piste sérieuse.
7. Provoquer plusieurs chargements de `/index.html`, `/style.css`, `/api/status`
   et vérifier que le programmateur continue de mettre à jour ses zones.

## Limite actuelle

Les fichiers ont été préparés à partir du commit GitHub exact, mais la
compilation PlatformIO n’a pas été exécutée dans cette session : le dépôt privé
et les dépendances ne sont pas montés dans l’environnement d’exécution.
