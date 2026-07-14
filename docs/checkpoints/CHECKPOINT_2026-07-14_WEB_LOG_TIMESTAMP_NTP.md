# CHECKPOINT — AquaLook — horodatage NTP du journal Web

Date : 14 juillet 2026

## Base

- Branche : `work/step7-run7-5`
- Correctif série validé : les nouveaux logs basculent sur l'heure locale après synchronisation NTP.
- RUN7.5 validé en shadow : orchestrateur prêt, `authority=no`, commandes zone 1 START/STOP fonctionnelles.

## Problème constaté

La page `/api/logs` continuait d'afficher uniquement le temps depuis le démarrage (`T+`) car `WebManager::handleGetLogs()` appelait directement :

```cpp
EventLog::msToHms(e.ms, tBuf, sizeof(tBuf));
```

L'epoch désormais stocké dans chaque `LogEntry` n'était donc pas utilisé.

## Correction préparée

Script contrôlé :

```text
tools/log-timestamps/apply_web_log_timestamp.ps1
```

Modifications ciblées dans `src/WebManager.cpp`, fonction `WebManager::handleGetLogs()` :

1. en-tête de colonne `T+` remplacé par `Heure / T+` ;
2. buffer timestamp élargi de 10 à 24 caractères ;
3. appel à `EventLog::formatEntryTimestamp(e, ...)` à la place de `msToHms(e.ms, ...)`.

Le pied de page `Uptime` conserve volontairement `msToHms(millis(), ...)`.

## Comportement attendu

- entrées créées avant NTP : temps depuis le boot, par exemple `00:00:05.695` ;
- entrées créées après NTP : heure locale réelle, par exemple `08:44:34.754` ;
- historique déjà enregistré avant la synchronisation non recalculé artificiellement.

## Application locale

```powershell
git pull --ff-only
powershell -ExecutionPolicy Bypass -File tools/log-timestamps/apply_web_log_timestamp.ps1
git diff --check
git diff -- src/WebManager.cpp
```

## Validation

Le fichier nominal `src/WebManager.cpp` étant modifié :

```powershell
pio run -e ProgrammeArrosage_v4 -t upload --upload-port COM3
pio device monitor --port COM3 --baud 115200
```

Puis ouvrir :

```text
http://<IP_AQUALOOK>/api/logs
```

Vérifier que les entrées postérieures à la synchronisation NTP affichent l'heure locale.

## Suite Phase 7

Après validation et commit de `src/main.cpp` et `src/WebManager.cpp`, créer RUN7.6 depuis cet état consolidé. RUN7.6 ajoutera les compteurs d'observation de l'orchestrateur shadow avant toute prise d'autorité.
