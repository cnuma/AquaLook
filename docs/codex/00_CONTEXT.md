# 00 — Contexte du projet

## Produit

AquaLook est un programmateur d’arrosage autonome sur ESP32. Il regroupe :

- une logique d’arrosage non bloquante ;
- une interface Web locale ;
- un écran TFT tactile ;
- une configuration persistante ;
- une synchronisation horaire NTP ;
- une intégration météo OpenWeatherMap ;
- un pilotage de relais par expandeur I²C ;
- une chaîne OTA en cours de qualification progressive.

## Matériel inspecté

### Carte principale

- ESP32, cible PlatformIO `esp32dev`
- Flash : 4 Mo
- Partition : `min_spiffs.csv`
- Système de fichiers : LittleFS
- Cible V4 testée : `esp32-2432S028`
- Layout dual OTA validé sur la carte V4

### Affichage

- TFT ILI9341 240 × 320
- Backlight : GPIO 21
- Touch XPT2046 sur bus séparé
- Broches définies dans `platformio.ini`

### I²C

- SDA : GPIO 27
- SCL : GPIO 22
- XL9535 : adresse `0x20`

### Contrôleurs de relais

- XL9535, carte YellowCard
- MCP23017, prévu et exposé dans la configuration
- Limite fonctionnelle actuelle : 1 à 8 zones
- Capacité interne des tableaux : 16 zones

## Base logicielle

- Framework Arduino
- Plateforme `espressif32 @ 6.13.0`
- Bibliothèques principales : ArduinoJson 7, ESPAsyncWebServer, AsyncTCP, TFT_eSPI, TJpg_Decoder, XPT2046_Touchscreen

## Source de vérité courante

Le socle historique a été construit après inspection du dépôt GitHub réel et du checkpoint complet du 27 juin 2026.

Pour le palier OTA-3.0 validé le 30 juillet 2026, la référence de travail est :

- dépôt : `cnuma/AquaLook` ;
- branche : `agent/ota-3.0-download-test-v591` ;
- base volontaire : tag `v5.9.1` ;
- commit code validé avant documentation : `6808f58bb0f386a17a2c24d5bb25fe0500410d43` ;
- documentation : `docs/codex/11_OTA_3_DOWNLOAD_VALIDATION.md` ;
- checkpoint : `docs/checkpoints/CHECKPOINT_2026-07-30_OTA-3.0_DOWNLOAD_VERIFIED.md`.

La branche part volontairement de `v5.9.1` afin de vérifier qu’un module installé en 5.9.1 détecte et télécharge la release distante 5.9.2. Elle ne doit pas être réalignée ou reconstruite depuis `main` sans analyse explicite.

## Lecture obligatoire pour l’OTA

Après les lectures déjà imposées par `AGENTS.md`, lire obligatoirement :

1. `docs/codex/11_OTA_3_DOWNLOAD_VALIDATION.md` ;
2. `docs/checkpoints/CHECKPOINT_2026-07-30_OTA-3.0_DOWNLOAD_VERIFIED.md` ;
3. `platformio.ini` ;
4. les fichiers OTA réellement concernés.

Ne pas utiliser un extrait de chat comme source de vérité.

## Fonctionnalités actuelles

- 1 à 8 zones actives
- 5 créneaux maximum par jour et par zone
- mode jours fixes
- mode intervalle
- démarrage et arrêt manuel
- seuil de pluie par zone
- fenêtre météo par zone
- journal d’événements en RAM
- portail captif
- configuration utilisateur et administrateur
- personnalisation LCD et Web
- conservation de la configuration en NVS
- migration depuis l’ancien `/config.json` LittleFS
- vérification OTA de version via manifeste GitHub
- sélection de la cible V4
- téléchargement complet du firmware sans installation
- validation de taille et SHA-256
- affichage Web persistant du résultat
- attente Web animée et rechargement automatique après maintenance

## État OTA validé

Sur matériel V4, les commandes suivantes sont validées :

- `CHECK_VERSION` ;
- `DOWNLOAD_UPDATE_TEST`.

La validation observée comprend le redémarrage en maintenance minimale, le Wi-Fi, TLS GitHub, le manifeste, la sélection V4, le téléchargement de 1 365 088 octets, le SHA-256 conforme, la persistance et le retour au fonctionnement normal.

Aucune écriture de partition OTA n’est encore implémentée dans ce palier. `setInsecure()` est encore utilisé et interdit de présenter la chaîne comme prête pour la production.

## Interface administrateur

Le verrouillage actuel masque visuellement les réglages sensibles, utilise `sessionStorage`, la clé `aqualook-admin-unlocked` et le mot de passe temporaire `1598753`.

Ce mécanisme n’est pas une authentification serveur. Il ne doit pas être présenté comme une protection de sécurité forte.

## Contraintes fortes

- LittleFS est très proche de sa limite.
- Toute ressource déposée dans `data/` est embarquée.
- Les fichiers de sauvegarde dans `data/` provoquent une saturation.
- Le matériel relais peut être activé au boot si la logique est incorrecte.
- Les modifications de persistance exigent une compatibilité avec les données existantes.
- Toute évolution OTA doit préserver les builds Legacy et V4.
- Le port COM doit être reconfirmé avant chaque téléversement.
- Toute installation OTA future doit intégrer confiance TLS, partition inactive, contrôle final et rollback.
