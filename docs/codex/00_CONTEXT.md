# 00 — Contexte du projet

## Produit

AquaLook est un programmateur d’arrosage autonome sur ESP32. Il regroupe :

- une logique d’arrosage non bloquante ;
- une interface Web locale ;
- un écran TFT tactile ;
- une configuration persistante ;
- une synchronisation horaire NTP ;
- une intégration météo OpenWeatherMap ;
- un pilotage de relais par expandeur I²C.

## Matériel inspecté

### Carte principale

- ESP32, cible PlatformIO `esp32dev`
- Flash : 4 Mo
- Partition : `min_spiffs.csv`
- Système de fichiers : LittleFS

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

## État de référence

Le présent socle a été construit après inspection du dépôt GitHub réel, de la branche `main`, du commit `a2cf490aa446c7006557f8df62e1f995f6767359`, du checkpoint complet du 27 juin 2026 et de l’arborescence PlatformIO.

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

## Interface administrateur

Le verrouillage actuel masque visuellement les réglages sensibles, utilise `sessionStorage`, la clé `aqualook-admin-unlocked` et le mot de passe temporaire `1598753`.

Ce mécanisme n’est pas une authentification serveur. Il ne doit pas être présenté comme une protection de sécurité forte.

## Contraintes fortes

- LittleFS est très proche de sa limite.
- Toute ressource déposée dans `data/` est embarquée.
- Les fichiers de sauvegarde dans `data/` provoquent une saturation.
- Le matériel relais peut être activé au boot si la logique est incorrecte.
- Les modifications de persistance exigent une compatibilité avec les données existantes.
