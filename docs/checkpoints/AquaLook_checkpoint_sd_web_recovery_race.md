# Checkpoint — Course critique SD / serveur Web asynchrone

## Base

- Dépôt : `cnuma/AquaLook`
- Branche : `fix/sd-web-recovery`
- Base : `73ebf9aaef4861847e7ff4dc3311b038c37f3680`
- Firmware observé : AquaLook 5.9.1, build 839, cible V4

## Symptôme reproduit

Après plusieurs rafraîchissements de `/`, une lecture de ressource Web sur SD échoue ponctuellement (`/www/style-base.css`). `StorageManager` déclare alors immédiatement la SD indisponible, appelle `_sd.end()`, programme un remontage et déclenche les notifications d'incident.

Le serveur `ESPAsyncWebServer` possède encore des réponses chunkées et des `FsFile` actifs. La SD est remontée depuis une tâche FreeRTOS sur le cœur 0 pendant que `AsyncTCP` poursuit ses callbacks sur le cœur 1.

Le crash capturé est un `LoadProhibited` dans la chaîne :

- `memmove`
- `String::move`
- `AsyncBasicResponse::_respond`
- `AsyncWebServerRequest::_parseLine`
- `_async_service_task`

## Cause retenue

Course concurrente entre :

1. les lectures Web SD asynchrones ;
2. le démontage immédiat de la SD après une seule erreur de lecture ;
3. le remontage automatique lancé depuis une tâche distincte.

## Correctif attendu

Modifier :

- `src/StorageManager.h`
- `src/StorageManager.cpp`
- `src/SdStaticHandler.cpp`

Principes :

- compter les lectures Web SD actives ;
- mettre la SD en quarantaine après une erreur ;
- bloquer les nouvelles lectures ;
- différer le démontage dans `StorageManager::update()` ;
- attendre la fermeture de toutes les réponses actives ;
- réautoriser les lectures uniquement après remontage réussi ;
- ne jamais appeler `_sd.end()` depuis un callback `AsyncTCP`.

## Validation obligatoire

- compilation `ProgrammeArrosage_legacy` ;
- compilation `ProgrammeArrosage_v4` ;
- test matériel V4 ;
- rafraîchissements répétés de `/` et `/ota` ;
- absence de Guru Meditation ;
- récupération SD contrôlée ;
- surveillance ping et HTTP ;
- aucun staging OTA avant validation complète.
