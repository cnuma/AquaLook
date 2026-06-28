# 01 — Architecture

## Vue d’ensemble

```text
main.cpp
 ├─ ConfigManager
 ├─ RelaisManager
 ├─ ScheduleManager
 ├─ WiFiManager
 ├─ NTPManager
 ├─ WeatherManager
 ├─ WebManager
 └─ DisplayManager
       └─ ScreenManager

Communication transversale limitée :
 ├─ EventBus
 └─ EventLog
```

## Démarrage

Ordre actuel dans `setup()` :

1. série ;
2. bus I²C ;
3. scan I²C de diagnostic ;
4. `ConfigManager::begin()` ;
5. initialisation TFT et splash ;
6. relais ;
7. planning ;
8. Wi-Fi ;
9. NTP ;
10. serveur Web ;
11. météo ;
12. affichage complet et touch.

L’ordre est important : LittleFS doit être monté avant l’accès aux ressources, la configuration doit être disponible avant les managers et le callback relais doit être câblé avant l’exécution du planning.

## Boucle principale

Dans `loop()` : Wi-Fi, NTP et météo si connecté, planificateur si NTP synchronisé, sécurité relais, Web event-driven, affichage/touch puis `yield()`.

Aucun traitement d’arrosage ne doit devenir bloquant.

## Modules

### ConfigManager

Montage LittleFS, chargement, migration JSON historique, persistance NVS binaire, CRC, schéma, application au planning et setters persistants.

### RelaisManager

Abstraction du contrôleur, logique directe/inverse, activation/désactivation par zone, durée maximale de sécurité et retour d’état.

### ScheduleManager

Créneaux jours fixes et intervalle, décision de démarrage, prise en compte de la pluie, état d’exécution et activation via callback uniquement.

### WiFiManager

Connexion station, reconnexion, portail captif, DNS de redirection et états réseau exclusifs.

### NTPManager

Synchronisation et exposition de l’heure, des minutes, du jour de semaine et du jour epoch.

### WeatherManager

Appels OpenWeatherMap, cache, agrégation, pluie et données détaillées Web/LCD.

### WebManager

Serveur asynchrone, fichiers LittleFS, API JSON, sauvegardes, actions manuelles, statut, logs et portail captif.

Routes observées : `/`, `/setup`, `/api/status`, `/api/adminStatus`, `/api/zone`, `/api/display`, `/api/wifi/scan`, `/api/captive`, `/api/resetConfig`, `/api/logs` et routes captive OS.

### DisplayManager

TFT, touch, navigation, rendu zones, planning, écran administrateur, thème, redraw et hot-reload.

### ScreenManager

Gestion de veille et état d’écran.

### EventBus

Flags statiques : `displayDirty`, `configDirty`, `wifiDirty`, `captiveRequested`.

### EventLog

Journal circulaire RAM de 60 entrées, messages de 72 caractères, niveaux INFO/WARN/ERROR, sans allocation dynamique.

## Données persistées

### NVS

Namespace `aqualook`, clé `config`, blob binaire versionné avec magic `ALOK`, schéma, taille et CRC32.

### LittleFS

Ressources : `data/index.html`, `data/app.js`, `data/style-base.css`, `data/style.css`, `data/splash.jpg`.

L’ancien `/config.json` n’est lu que pour migration.

## Frontend

Les IDs HTML sont des contrats entre `index.html` et `app.js`.
