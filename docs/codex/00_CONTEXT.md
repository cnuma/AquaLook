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
- Partition : table double OTA validée sur la branche
- Système de fichiers minimal : LittleFS
- Ressources Web principales : carte SD, répertoire `/www`
- PSRAM : absente

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

Pour la branche `work/storage-sd-recovery`, la source de vérité fonctionnelle validée est le commit `f16be3c0279643e4c4ae09144484be9e54b4f499`.

Le commit documentaire précédent est `e46c39261a7fe3adf18a0a6a6584d0cfe2662e86`.

Le checkpoint détaillé associé est :

`docs/checkpoints/CHECKPOINT_2026-07-23_work-storage-sd-recovery_notifications-zones-valides.md`

Un nouveau checkpoint officiel de reprise est créé le 28 juillet 2026 selon la procédure définie dans `AGENTS.md`.

## Fonctionnalités actuelles validées sur cette branche

- 1 à 8 zones actives
- 5 créneaux maximum par jour et par zone
- mode jours fixes
- mode intervalle
- démarrage et arrêt manuel
- seuil de pluie par zone
- fenêtre météo par zone
- journal d’événements en RAM et route texte `/api/logs.txt`
- portail captif
- configuration utilisateur et administrateur
- personnalisation LCD et Web
- conservation de la configuration en NVS
- migration NVS schéma 1 vers schéma 2
- récupération automatique de la carte SD
- persistance des incidents SD en NVS
- ressources Web principales servies depuis `/www` sur la carte SD
- configuration ntfy persistée en NVS
- transport ntfy HTTP sans TLS validé
- notification configurable par zone au démarrage et à l’arrêt
- déclenchement ntfy uniquement après succès réel du backend physique

## Notifications ntfy

Le TLS direct sur le contrôleur principal reste interdit dans cette architecture, car le handshake échoue par fragmentation mémoire malgré une heap libre suffisante en valeur totale.

Le transport validé est temporairement :

`AquaLook -> HTTP port 80 -> ntfy.sh -> téléphone`

Limites :

- contenu et topic non chiffrés ;
- ne pas utiliser de jeton sensible ;
- ne jamais libérer les sprites pour tenter de rendre TLS possible ;
- une future passerelle locale, MQTT, Home Assistant ou ESP32-S2 reste recommandée.

## Ressources Web et diagnostic

Les ressources complètes sont servies depuis la carte SD. Après modification de `data/app.js` ou `data/logs.html`, les fichiers doivent être copiés dans `/www` sur la carte SD ; `uploadfs` ne réalise pas cette opération.

Avant de conclure à une régression Web :

1. vérifier `/api/status` ;
2. vérifier directement `/app.js` ;
3. contrôler la cohérence firmware / SD / cache navigateur ;
4. effectuer `Ctrl+F5` ;
5. tester en navigation privée ;
6. consulter la console JavaScript.

## Interface administrateur

Le verrouillage actuel masque visuellement les réglages sensibles, utilise `sessionStorage`, la clé `aqualook-admin-unlocked` et le mot de passe temporaire `1598753`.

Ce mécanisme n’est pas une authentification serveur. Il ne doit pas être présenté comme une protection de sécurité forte.

## Contraintes fortes

- Le contrôleur principal ne dispose pas de PSRAM.
- Les ressources Web SD et le firmware doivent rester compatibles.
- L’absence ou le retrait de la SD ne doit pas bloquer l’arrosage ni l’accès de secours.
- Toute évolution de persistance exige une compatibilité avec les données existantes.
- Une notification ne doit jamais bloquer ni conditionner une commande de relais.
- Toute transition de zone notifiée doit avoir été confirmée par le backend réel.
- Le tactile, le portail captif et les sprites sont des invariants anti-régression.