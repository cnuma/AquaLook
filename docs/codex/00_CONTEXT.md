# 00 — Contexte du projet

Mis à jour le 10 octobre 2026 (base `main` = `46df836`). Le détail vivant
(architecture, état fonctionnel, pièges) est dans `docs/REPRISE_INGENIEUR.md` ;
l'état du jour est dans le checkpoint le plus récent de `docs/checkpoints/`.

## Produit

AquaLook est un programmateur d’arrosage autonome sur ESP32-S3. Il regroupe :

- une planification locale non bloquante (1 à 8 zones) et un **moteur
  d’exécution unique, V4** (zones → équipements → cartes → ports → voies) ;
- un pilotage de vannes par cartes relais I²C (XL9535, MCP23017), câblage
  décrit comme une donnée éditable ;
- un écran tactile local et une interface Web locale (servie depuis la SD) ;
- un moteur de scripts embarqué ;
- une configuration persistante en NVS ;
- NTP, météo (OpenWeatherMap ou Open-Meteo), notifications ntfy ;
- des mises à jour firmware et Web **toujours déclenchées par l’utilisateur** ;
- une synchronisation optionnelle avec l’espace en ligne (CloudSync,
  AlwaysData), enrôlement par code court, PIN sur le LCD.

**Principe directeur** : l’arrosage et les sécurités fonctionnent sans Wi-Fi,
sans Internet, sans serveur et sans carte SD.

Le moteur historique a été supprimé le 8 septembre 2026 (commit `2750866`).
Il n’existe plus : aucune procédure, comparaison ni compilation ne s’y réfère.

## Matériel de référence

| Élément | Valeur |
|---|---|
| Carte | Guition JC4827W543C_I, ESP32-S3 |
| Module de test | `.141` (`192.168.1.141`), N4R8 : 4 Mo de flash, PSRAM OPI 8 Mo, 5 zones câblées sur relais réels |
| Écran | NV3041A 480×272, QSPI, `GFX Library for Arduino` via `lib/tft_espi_compat_s3` |
| Tactile | GT911 capacitif, I²C dédié (SDA 8, SCL 4) |
| Relais | XL9535 sur le second bus I²C (`Wire1`, SCL 17 / SDA 18), adresse `0x20` |
| SD | SPI dédié (MISO 13, MOSI 11, SCK 12, CS 10) |
| Voyant | ruban WS2812 sur GPIO 46 |
| Partitions | `aqualook_partitions.csv` : deux slots OTA de 1 920 Kio, NVS 84 Kio, LittleFS 108 Kio |

Le brochage est centralisé dans la section `[jc4827w543c_i]` de
`platformio.ini` (macros `AQ_S3_*`).

L’ancienne carte CYD (ESP32-2432S028, env `ProgrammeArrosage`) est en cours
d’abandon : elle n’est compilée que sur demande explicite.

## Base logicielle

- Framework Arduino, plate-forme `espressif32 @ 6.13.0`
- Environnement de production et de validation : `ProgrammeArrosage_s3`
- Bibliothèques principales : ArduinoJson 7, ESPAsyncWebServer et AsyncTCP
  (patchés au build), SdFat, GFX Library for Arduino, TAMC_GT911,
  Adafruit NeoPixel
- Version fonctionnelle : fichier `VERSION` (source unique, lue par
  `tools/version_build.py`)

## Serveur

`cloud/php-mutualized/` (PHP + MySQL) en service sur AlwaysData
(`https://aqualook.alwaysdata.net`) : API module (`/v1/*`), console
d’administration, espace utilisateur (`app.html`), ressources Web publiées.
Publication par le propriétaire (FTP), jamais par l’agent.

## Interface administrateur Web

Le verrouillage actuel de `data/index.html` est **visuel** (`sessionStorage`,
mot de passe temporaire `1598753`). Ce n’est pas une authentification ; son
remplacement par une session Web locale est le lot F de D016.

## Contraintes fortes

- Le planificateur ne pilote jamais directement le matériel ; la durée
  maximale de sécurité ne se retire pas.
- LittleFS est proche de sa limite et ne contient que les secours
  indispensables (`littlefs/`).
- Toute évolution de la NVS est versionnée, avec migration ; sauvegarder la
  partition (`tools/nvs_backup.py`) avant un flash qui change le schéma.
- Une table de partitions ne se déploie pas par OTA.
- Aucun serveur distant ne déclenche une mise à jour (D014).
- Tout paramètre de service se règle sans recompilation (console ou NVS).
- Le dépôt est public : aucun secret dans Git.
- Le port COM est reconfirmé à chaque session.
